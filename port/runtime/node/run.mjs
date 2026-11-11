// Pen Pen port, S3: a minimal stand-in for the retrowser host in node (inside the sandbox only): it
// gives the wasm module env.rtw_disc_toc / rtw_disc_read over a Redump .cue + .bin set (same LBA model
// as port/runtime/disc_native.c and retrowser spec 4.10) and env.rtw_log, then calls rtw_start.
// Usage: node run.mjs penpen.wasm DISC_DIR [--frames N] [--verbose N] [--pad SCRIPT] [--digest N] [--frame-log 0|1]
import fs from 'node:fs';
import path from 'node:path';

const [wasmPath, discDir, ...rest] = process.argv.slice(2);
const opt = (k, d) => { const i = rest.indexOf(k); return i >= 0 ? Number(rest[i + 1]) : d; };

const cueName = fs.readdirSync(discDir).find((n) => n.toLowerCase().endsWith('.cue'));
const tracks = [];
let file = null;
for (const line of fs.readFileSync(path.join(discDir, cueName), 'latin1').split(/\r?\n/)) {
  const f = line.match(/^\s*FILE\s+"(.*)"/);
  if (f) file = path.join(discDir, f[1]);
  const t = line.match(/^\s*TRACK\s+(\d+)\s+(\S+)/);
  if (t) tracks.push({ num: +t[1], data: t[2].startsWith('MODE'), file, idx1: 0 });
  const x = line.match(/^\s*INDEX\s+01\s+(\d+):(\d+):(\d+)/);
  if (x) tracks[tracks.length - 1].idx1 = (+x[1] * 60 + +x[2]) * 75 + +x[3];
}
let lba = 0;
for (const t of tracks) {
  t.sectors = Math.floor(fs.statSync(t.file).size / 2352);
  if (t.num === 3) lba = 45000;
  t.lbaFile = lba;
  lba += t.sectors;
}

let mem;
const fds = new Map();
const raw = Buffer.alloc(2352);
const env = {
  rtw_log(p, len) { process.stdout.write(Buffer.from(mem.buffer, p, len).toString('utf8') + '\n'); },
  rtw_disc_toc(p, max) {
    const v = new Int32Array(mem.buffer, p, Math.min(max, tracks.length) * 4);
    tracks.slice(0, max).forEach((t, i) => {
      v[4 * i] = t.num; v[4 * i + 1] = t.lbaFile + t.idx1; v[4 * i + 2] = t.sectors - t.idx1;
      v[4 * i + 3] = (t.data ? 1 : 0) | (t.num >= 3 ? 2 : 0);
    });
    return tracks.length;
  },
  rtw_disc_read(num, l, count, p) {
    const t = tracks.find((x) => x.num === num);
    if (!t) return -1;
    const first = t.lbaFile + t.idx1, last = t.lbaFile + t.sectors;
    if (l < first || l >= last) return -2;
    count = Math.min(count, 256, last - l);
    if (!fds.has(num)) fds.set(num, fs.openSync(t.file, 'r'));
    const out = new Uint8Array(mem.buffer);
    for (let k = 0; k < count; k++) {
      if (fs.readSync(fds.get(num), raw, 0, 2352, (l + k - t.lbaFile) * 2352) !== 2352) return -3;
      if (t.data) out.set(raw.subarray(16, 16 + 2048), p + k * 2048);
      else out.set(raw, p + k * 2352);
    }
    return count;
  },
};

const mod = await WebAssembly.compile(fs.readFileSync(wasmPath));
const imports = { env: {} };
for (const im of WebAssembly.Module.imports(mod)) {
  if (im.module === 'env' && env[im.name]) imports.env[im.name] = env[im.name];
  else if (im.kind === 'function') {
    (imports[im.module] ??= {})[im.name] = () => { throw new Error('boot import not provided ' + im.module + '.' + im.name); };
  }
}
const extra = WebAssembly.Module.imports(mod).filter((i) => !(i.module === 'env' && i.name.startsWith('rtw_')));
console.log('boot wasm imports outside env.rtw_*: ' + extra.length);
const inst = await WebAssembly.instantiate(mod, imports);
mem = inst.exports.memory;
inst.exports._initialize?.();
inst.exports.rt_set_frames?.(opt('--frames', 3));
inst.exports.rt_set_verbose?.(opt('--verbose', 600));
inst.exports.rt_set_digest?.(opt('--digest', 1));
inst.exports.rt_set_frame_log?.(opt('--frame-log', 1));
{
  const i = rest.indexOf('--pad');
  if (i >= 0) {
    const b = Buffer.from(rest[i + 1] + '\0', 'latin1');
    new Uint8Array(mem.buffer, inst.exports.rt_arg_buf(), b.length).set(b);
    if (inst.exports.rt_set_pad() < 0) { console.log('boot bad pad script'); process.exit(2); }
  }
}
const t0 = Date.now();
try {
  inst.exports.rtw_start();
} catch (e) {
  console.log('boot wasm stopped (' + (e instanceof WebAssembly.RuntimeError ? 'trap' : 'error') + ') after ' + (Date.now() - t0) + ' ms');
}
