// Pen Pen S6: end-to-end test of the retrowser module, run by tools/rtw.sh e2e inside one --network none
// container (image penpen-rtw:1): retrowser's own local host (its e2e harness: the Worker handler on dist/ and the
// GitHub mock) serves the module from /build/rtw; Playwright's headless Chromium opens the game page, the
// player's disc archive (/raw/archive.7z) is picked on the disc screen exactly as a player would, the host
// unpacks it into OPFS and boots the game. Keyboard input (the host's Dreamcast defaults: Enter = Start,
// X = A, arrows = stick) goes through the menus into a race. Checked: frames run, no errors, the VMU save is
// stored by the host and read back after a reload, rumble reaches the page. Screenshots of the game screen go to
// /build/e2e (looked at with tools/rtw.sh look). Only our own log lines and counts are printed ("e2e ...").
// Args: --skip-reload (one boot only), --race-seconds N (default 40).
import { mkdirSync, writeFileSync } from 'node:fs';
import type { Server } from 'node:http';
import { chromium, type Page } from '/rtw/node_modules/playwright-core/index.mjs';
import { chromePath } from '/rtw/e2e/harness.ts';
import { serveWorker } from '/rtw/tools/serve-worker.ts';
import { startMockGithub } from '/rtw/tools/mock-github.ts';

const MODULE_DIR = '/build/rtw', OUT = '/build/e2e', DISC = '/raw/archive.7z';
const SHA = '5e6e5e6e5e6e5e6e5e6e5e6e5e6e5e6e5e6e5e6e';
const REPO = 'penpen/triicelon';
const args = process.argv.slice(2);
const raceSeconds = Number(args[args.indexOf('--race-seconds') + 1] || 40) || 40;
mkdirSync(OUT, { recursive: true });

const say = (s: string) => console.log('e2e ' + s);
const t0 = Date.now();
const secs = () => ((Date.now() - t0) / 1000).toFixed(0) + 's';
const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

// retrowser's own pieces (as its e2e harness), but a persistent Chromium profile on the work volume: an
// off-the-record context keeps OPFS in memory with a small quota, too small for a 1.2 GB disc
const mock = await startMockGithub({ [REPO]: { refs: { main: SHA }, commits: [{ sha: SHA, dir: MODULE_DIR }] } });
const port = 8800 + Math.floor(Math.random() * 100);
const server: Server = await serveWorker(port, undefined, mock.endpoints.raw, {});
const ctx = await chromium.launchPersistentContext('/work/profile', {
  executablePath: chromePath(), headless: true, reducedMotion: 'reduce',
  args: ['--mute-audio', '--enable-unsafe-swiftshader'],
});
await ctx.addInitScript((ep: string) => {
  try { if (location.hostname === 'localhost') localStorage.setItem('rtw:test-endpoints', ep); } catch { /* ignore */ }
}, JSON.stringify(mock.endpoints));
const h = {
  site: `http://localhost:${port}`, context: ctx,
  async close() {
    await ctx.close();
    await mock.close();
    await new Promise<void>((ok) => { server.closeAllConnections(); server.close(() => ok()); });
  },
};
await h.context.addInitScript(() => {
  const w = window as unknown as { __vib: number[] };
  w.__vib = [];
  try { Object.defineProperty(navigator, 'vibrate', { value: (p: number | number[]) => { w.__vib.push(Array.isArray(p) ? p.length : p); return true; }, configurable: true }); } catch { /* ignore */ }
});
const page = await h.context.newPage();
await page.setViewportSize({ width: 1000, height: 800 });
const errors: string[] = [];
page.on('pageerror', (e) => errors.push(e.message));

const frames = async (p: Page) => Number(await p.getAttribute('[data-state=playing]', 'data-frames').catch(() => '0') ?? 0);
const log = async (p: Page) => (await p.getAttribute('[data-state=playing]', 'data-log').catch(() => '')) ?? '';
async function failIfError(p: Page) {
  const err = await p.$('.play .error, [data-state=error]');
  if (err) { say('FAIL the host shows an error: ' + ((await err.textContent()) ?? '').replace(/[^ -~]/g, ' ').slice(0, 600)); throw new Error('host error'); }
}
async function untilFrame(p: Page, n: number, timeoutS = 600) {
  const end = Date.now() + timeoutS * 1000;
  for (;;) {
    const f = await frames(p);
    if (f >= n) return f;
    await failIfError(p);
    if (Date.now() > end) throw new Error(`frame ${n} not reached (at ${f})`);
    await sleep(200);
  }
}
async function press(p: Page, key: string, atFrame: number, holdMs = 250) {
  const f = await untilFrame(p, atFrame);
  await p.keyboard.down(key);
  await sleep(holdMs);
  await p.keyboard.up(key);
  say(`press ${key} at frame ~${f}`);
}
async function shot(p: Page, name: string) {
  const f = await frames(p);
  const png = await p.locator('[data-field=screen]').screenshot();
  writeFileSync(`${OUT}/${name}.png`, png);
  say(`shot ${name} at frame ~${f} (${png.length} bytes)`);
}
async function focusGame(p: Page) {
  await p.locator('[data-field=screen]').click({ position: { x: 5, y: 5 } }).catch(() => {});
}
/** the save the host stores for this game (IndexedDB), summarised: size and the VMU directory's files */
async function storedSave(p: Page) {
  return p.evaluate(async () => {
    const db = await new Promise<IDBDatabase>((ok, ko) => { const r = indexedDB.open('retrowser'); r.onsuccess = () => ok(r.result); r.onerror = () => ko(r.error); });
    const all = await new Promise<{ key: unknown; bytes?: Uint8Array }[]>((ok) => {
      const out: { key: unknown; bytes?: Uint8Array }[] = [];
      const r = db.transaction('saves').objectStore('saves').openCursor();
      r.onsuccess = () => { const c = r.result; if (!c) { ok(out); return; } out.push({ key: c.key, bytes: (c.value as { bytes?: Uint8Array }).bytes }); c.continue(); };
    });
    db.close();
    return all.map(({ key, bytes }) => {
      if (!bytes) return { key: String(key), size: 0, formatted: false, files: [] as string[] };
      const b = new Uint8Array(bytes);
      const formatted = b.slice(255 * 512, 255 * 512 + 16).every((x) => x === 0x55);
      const files: string[] = [];
      for (let blk = 253; blk > 240; blk--) for (let i = 0; i < 16; i++) {
        const o = blk * 512 + i * 32;
        if (b[o] === 0x33 || b[o] === 0xcc) files.push(`type ${b[o].toString(16)} first ${b[o + 2] | b[o + 3] << 8} blocks ${b[o + 0x18] | b[o + 0x19] << 8}`);
      }
      return { key: String(key), size: b.length, formatted, files };
    });
  });
}
const ourLog = (s: string) => s.split('\n').filter((l) => /^(hle (vmu|rumble|vib|last)|boot |rt |penpen)/.test(l));

try {
  // ---- first boot: pick the archive on the disc screen ----
  await page.goto(`${h.site}/g/${REPO}`);
  await page.click('[data-action=cartridge]');
  await page.waitForSelector('[data-state=rom][data-disc], [data-state=playing], [data-state=error]', { timeout: 120_000 });
  await failIfError(page);
  say(`disc screen at ${secs()}`);
  await page.setInputFiles('[data-field=disc]', DISC);
  say(`archive picked at ${secs()}; the host unpacks it`);
  await page.waitForSelector('[data-state=playing], [data-state=wrong-rom], [data-state=disc-error], .play .error', { timeout: 30 * 60_000 });
  if (await page.$('[data-state=wrong-rom]')) { say('FAIL wrong-disc warning: ' + ((await page.textContent('[data-state=wrong-rom]')) ?? '').slice(0, 300)); throw new Error('wrong disc'); }
  if (await page.$('[data-state=disc-error]')) { say('FAIL disc error: ' + ((await page.textContent('[data-state=disc-error]')) ?? '').slice(0, 300)); throw new Error('disc error'); }
  await failIfError(page);
  say(`playing at ${secs()}`);
  await focusGame(page);

  await untilFrame(page, 100); await shot(page, 'b1-logo');
  await untilFrame(page, 330); await shot(page, 'b1-logo2');
  await untilFrame(page, 520); await shot(page, 'b1-title');
  await press(page, 'Enter', 600);
  await untilFrame(page, 660); await shot(page, 'b1-menu');
  await press(page, 'KeyX', 700);
  await untilFrame(page, 900); await shot(page, 'b1-house');
  // the save made at the mode menu reaches the host's storage
  let saved: Awaited<ReturnType<typeof storedSave>> = [];
  for (let i = 0; i < 50; i++) { saved = await storedSave(page); if (saved.some((s) => s.files.length)) break; await sleep(200); }
  say('stored saves: ' + JSON.stringify(saved));
  // the host's notice that this (headless) browser refused persistent storage: dismiss it
  await page.getByRole('button', { name: 'OK', exact: true }).click({ timeout: 2000 }).then(() => say('dismissed the storage notice'), () => {});
  await press(page, 'Enter', 1100);
  await press(page, 'KeyX', 1200);
  await untilFrame(page, 1300); await shot(page, 'b1-select');
  await press(page, 'KeyX', 1500);
  await untilFrame(page, 1600); await shot(page, 'b1-course');
  await press(page, 'KeyX', 1700);
  await press(page, 'KeyX', 1760);
  await untilFrame(page, 2300); await shot(page, 'b1-race-start');
  // race: hold A (run), steer left and right; measure the frame rate meanwhile
  const fA = await untilFrame(page, 2400);
  const tA = Date.now();
  await page.keyboard.down('KeyX');
  for (let k = 0; k * 4 < raceSeconds; k++) {
    await page.keyboard.down(k % 2 ? 'ArrowRight' : 'ArrowLeft');
    await sleep(2000);
    await page.keyboard.up(k % 2 ? 'ArrowRight' : 'ArrowLeft');
    await sleep(2000);
    if (k === 2) await shot(page, 'b1-race');
    await failIfError(page);
  }
  await page.keyboard.up('KeyX');
  const fB = await frames(page), tB = Date.now();
  say(`race frame rate: ${((fB - fA) * 1000 / (tB - tA)).toFixed(1)} game frames/s over ${((tB - tA) / 1000).toFixed(0)} s (frames ${fA}..${fB})`);
  await shot(page, 'b1-race-late');
  const vib = await page.evaluate(() => (window as unknown as { __vib: number[] }).__vib.length);
  const l1 = await log(page);
  say(`rumble: navigator.vibrate called ${vib} times; module log rumble lines: ${l1.split('\n').filter((l) => l.startsWith('hle rumble')).length}`);
  for (const l of ourLog(l1)) say('log1 ' + l);
  await failIfError(page);

  if (!args.includes('--skip-reload')) {
    // ---- second boot: reload the page; the disc is remembered, the host gives the module the stored VMU ----
    await page.reload();
    await page.click('[data-action=cartridge]');
    await page.waitForSelector('[data-state=playing], [data-action=use-disc]', { timeout: 120_000 });
    if (await page.$('[data-action=use-disc]')) await page.click('[data-action=use-disc]');
    await page.waitForSelector('[data-state=playing]', { timeout: 120_000 });
    say(`second boot playing at ${secs()}`);
    await focusGame(page);
    await press(page, 'Enter', 600);
    await press(page, 'KeyX', 700);
    await untilFrame(page, 900); await shot(page, 'b2-house');
    const l2 = await log(page);
    for (const l of ourLog(l2)) say('log2 ' + l);
    const loaded = l2.includes('hle vmu load 5 blocks');
    say(loaded ? 'VMU: the save written in the first boot was found and loaded after the reload' : 'FAIL VMU: no load after the reload');
    say('stored saves after reload: ' + JSON.stringify(await storedSave(page)));
  }
  if (errors.length) say('page errors: ' + errors.join(' | ').replace(/[^ -~]/g, ' ').slice(0, 800));
  say(`done at ${secs()}`);
} catch (e) {
  say('FAIL ' + String((e as Error).message).replace(/[^ -~]/g, ' ').slice(0, 400));
  for (const l of ourLog(await log(page))) say('log ' + l);
  try { await page.screenshot({ path: `${OUT}/fail-page.png` }); say('shot fail-page'); } catch { /* ignore */ }
  process.exitCode = 1;
} finally {
  await h.close();
}
