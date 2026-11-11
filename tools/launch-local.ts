// Local launcher: serves the retrowser host (from a local retrowser checkout) plus a GitHub mock that hands it
// the Pen Pen module from a local folder, so the port can be played in the user's own browser on localhost.
// Run through tools/launch-local.sh (copies the module out of the penpen-build volume first).
// Usage: node tools/launch-local.ts <module-dir> <retrowser-checkout> [port]
import { resolve } from 'node:path';

const [moduleDir, rtwDir, portArg] = process.argv.slice(2);
if (!moduleDir || !rtwDir) {
  console.error('usage: node tools/launch-local.ts <module-dir> <retrowser-checkout> [port]');
  process.exit(2);
}
const rtw = resolve(rtwDir);
const { serveWorker } = await import(`${rtw}/tools/serve-worker.ts`);
const { startMockGithub } = await import(`${rtw}/tools/mock-github.ts`);

const REPO = 'penpen/triicelon';
const SHA = 'a'.repeat(40);
const port = Number(portArg) || 8790;
// fixed mock port, so the one-time localStorage line below stays valid across launches
const mock = await startMockGithub({ [REPO]: { refs: { main: SHA }, commits: [{ sha: SHA, dir: resolve(moduleDir) }] } }, port + 1);
await serveWorker(port, `${rtw}/dist`, mock.endpoints.raw, {});

const site = `http://localhost:${port}`;
console.log(`
Pen Pen TriIcelon (local, work in progress)

1. Open ${site}/ in Chrome.
2. First time only: open DevTools (Cmd+Opt+J), paste this line, press Enter:
   localStorage.setItem('rtw:test-endpoints', ${JSON.stringify(JSON.stringify(mock.endpoints))})
3. Go to ${site}/g/${REPO}
   Click the cartridge, pick "PenPen TriIcelon (USA).7z" (unpacked once into the browser, ~1.2 GB, ~1 min).

Keys: Enter = Start, X = A, C = B, Z = X, S = Y, arrows = stick, A / D = L / R, T F G H = D-pad (or a gamepad).
Music (CD tracks) and sound effects; MIDI sequences are still silent. Ctrl+C here to stop.
`);
