// Pen Pen port, S5: render draw-list dumps to PNGs in headless Chromium (SwiftShader WebGL2), all inside the
// sandbox container (tools/s5.sh shots): a static server on 127.0.0.1 for the page + backend (read-only
// repo) and the dumps, Chromium over the DevTools protocol (node's own WebSocket, no packages).
//   node shot.mjs RENDER_DIR SHOT_DIR OUT_DIR
import { spawn } from 'node:child_process';
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';

const [renderDir, shotDir, outDir] = process.argv.slice(2);
fs.mkdirSync(outDir, { recursive: true });
const names = fs.readdirSync(shotDir).filter((n) => n.endsWith('.ppdl')).sort();
const types = { '.html': 'text/html', '.js': 'text/javascript' };
const srv = http.createServer((req, res) => {
  const u = decodeURIComponent(req.url.split('?')[0]);
  let f = null;
  if (u.startsWith('/render/')) f = path.join(renderDir, path.normalize(u.slice(8)));
  else if (u.startsWith('/shots/')) f = path.join(shotDir, path.basename(u));
  if (!f || !fs.existsSync(f)) { res.writeHead(404); res.end(); return; }
  res.writeHead(200, { 'content-type': types[path.extname(f)] || 'application/octet-stream' });
  res.end(fs.readFileSync(f));
}).listen(0, '127.0.0.1');
await new Promise((r) => srv.on('listening', r));
const port = srv.address().port, dport = 9222;
const ch = spawn('/usr/bin/chromium', ['--headless=new', '--no-sandbox', '--disable-dev-shm-usage', '--mute-audio',
  '--user-data-dir=/tmp/chrome-prof', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist',
  `--remote-debugging-port=${dport}`, '--window-size=700,520', 'about:blank'], { stdio: 'ignore' });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let targets;
for (let i = 0; i < 100 && !targets; i++) { await sleep(200); try { targets = await (await fetch(`http://127.0.0.1:${dport}/json`)).json(); } catch {} }
const page = targets && targets.find((t) => t.type === 'page');
if (!page) { console.log('shot no Chromium page'); ch.kill(); process.exit(1); }
const ws = new WebSocket(page.webSocketDebuggerUrl);
await new Promise((r) => ws.addEventListener('open', r));
let id = 0;
const pending = new Map();
ws.addEventListener('message', (m) => { const msg = JSON.parse(m.data); if (msg.id && pending.has(msg.id)) { pending.get(msg.id)(msg); pending.delete(msg.id); } });
const send = (method, params = {}) => new Promise((r) => { const i = ++id; pending.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const evaluate = async (expr) => {
  const m = await send('Runtime.evaluate', { expression: expr, returnByValue: true, awaitPromise: true });
  if (m.result?.exceptionDetails) throw new Error(m.result.exceptionDetails.exception?.description || 'page error');
  return m.result?.result?.value;
};
await send('Page.enable');
await send('Page.navigate', { url: `http://127.0.0.1:${port}/render/shot/shot.html` });
let ok = false;
for (let i = 0; i < 100 && !ok; i++) { await sleep(100); try { ok = (await evaluate('document.title')) === 'READY'; } catch {} }
if (!ok) { console.log('shot page not ready: ' + (await evaluate('document.title'))); ch.kill(); process.exit(1); }
console.log('shot renderer ' + (await evaluate("(() => { const g = document.createElement('canvas').getContext('webgl2'); const e = g.getExtension('WEBGL_debug_renderer_info'); return e ? g.getParameter(e.UNMASKED_RENDERER_WEBGL) : g.getParameter(g.RENDERER); })()")));
// CMDS=1: per command list, texture, mode, triangles, screen box and 1/w range (from the dump, no GL)
function cmdInfo(n) {
  const b = fs.readFileSync(path.join(shotDir, n)), buf = b.buffer.slice(b.byteOffset, b.byteOffset + b.length);
  const h = new Uint32Array(buf, 0, 24), C = new Uint32Array(buf, h[9], h[8] * 8), V = new Float32Array(buf, h[11], h[10] * 7);
  for (let i = 0; i < h[8]; i++) {
    const f = C[8 * i + 1], cnt = C[8 * i + 2];
    let x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9, w0 = 1e9, w1 = -1e9;
    for (let k = f; k < f + cnt; k++) { const x = V[7 * k], y = V[7 * k + 1], w = V[7 * k + 2];
      x0 = Math.min(x0, x); y0 = Math.min(y0, y); x1 = Math.max(x1, x); y1 = Math.max(y1, y); w0 = Math.min(w0, w); w1 = Math.max(w1, w); }
    if (process.env.PROBE) { // triangles covering a pixel: their 1/w there
      const [px, py] = process.env.PROBE.split(',').map(Number);
      for (let k = f; k + 2 < f + cnt; k += 3) {
        const P = [0, 1, 2].map((j) => [V[7 * (k + j)], V[7 * (k + j) + 1], V[7 * (k + j) + 2]]);
        const d = (P[1][1] - P[2][1]) * (P[0][0] - P[2][0]) + (P[2][0] - P[1][0]) * (P[0][1] - P[2][1]);
        if (!d) continue;
        const a = ((P[1][1] - P[2][1]) * (px - P[2][0]) + (P[2][0] - P[1][0]) * (py - P[2][1])) / d;
        const b = ((P[2][1] - P[0][1]) * (px - P[2][0]) + (P[0][0] - P[2][0]) * (py - P[2][1])) / d;
        if (a >= 0 && b >= 0 && a + b <= 1) console.log(`shot probe cmd ${i} list ${C[8 * i]} tri ${(k - f) / 3} iw ${(a * P[0][2] + b * P[1][2] + (1 - a - b) * P[2][2]).toExponential(4)}`);
      }
    }
    console.log(`shot cmd ${i} list ${C[8 * i]} tex ${C[8 * i + 3]} mode ${C[8 * i + 4].toString(16)} tris ${cnt / 3} box ${x0 | 0} ${y0 | 0} ${x1 | 0} ${y1 | 0} iw ${w0.toExponential(3)} ${w1.toExponential(3)}`);
  }
}
for (const n of names) {
  if (process.env.CMDS === '1') cmdInfo(n);
  try {
    const v = await evaluate(`renderOne(${JSON.stringify(n)}, ${Number(process.env.LISTS ?? 15)})`);
    fs.writeFileSync(path.join(outDir, n.replace('.ppdl', '.png')), Buffer.from(v.png.slice(22), 'base64'));
    const s = v.stats;
    console.log(`shot ${n} cmds ${s.cmds} draws ${s.draws} verts ${s.verts} textures ${s.textures} op ${s.op} tr-tris ${s.trTris} om-tris ${s.omTris} culled ${s.culled}`);
  } catch (e) { console.log(`shot ${n} failed: ${String(e.message).slice(0, 200).replace(/[^ -~]/g, '?')}`); }
}
ws.close(); ch.kill(); srv.close();
process.exit(0);
