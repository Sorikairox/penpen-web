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
for (const n of names) {
  try {
    const v = await evaluate(`renderOne(${JSON.stringify(n)})`);
    fs.writeFileSync(path.join(outDir, n.replace('.ppdl', '.png')), Buffer.from(v.png.slice(22), 'base64'));
    const s = v.stats;
    console.log(`shot ${n} cmds ${s.cmds} draws ${s.draws} verts ${s.verts} textures ${s.textures} op ${s.op} tr-tris ${s.trTris} om-tris ${s.omTris} culled ${s.culled}`);
  } catch (e) { console.log(`shot ${n} failed: ${String(e.message).slice(0, 200).replace(/[^ -~]/g, '?')}`); }
}
ws.close(); ch.kill(); srv.close();
process.exit(0);
