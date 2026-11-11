// Pen Pen port, S3: a minimal stand-in for the retrowser host in node (inside the sandbox only): it
// gives the wasm module env.rtw_disc_toc / rtw_disc_read over a Redump .cue + .bin set (same LBA model
// as port/runtime/disc_native.c and retrowser spec 4.10) and env.rtw_log, then calls rtw_start.
// Usage: node run.mjs penpen.wasm DISC_DIR [--frames N] [--verbose N] [--pad SCRIPT] [--digest N] [--frame-log 0|1]
//          [--audio 0|1] [--wav FILE]
//          [--shots F1,F2,... --shot-dir DIR]
// S5: env.penpen_gl_frame(ptr, len) receives each frame's draw list (port/render/rd.c). This host has no GL: it
// keeps the uploaded textures and, for the frames listed in --shots, writes a self-contained dump DIR/fNNNNN.ppdl
// (the draw list with upload records for every texture it uses, pointers rebased into the file) that
// port/render/shot renders with the same WebGL2 backend in headless Chromium.
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
const strOpt = (k) => { const i = rest.indexOf(k); return i >= 0 ? rest[i + 1] : null; };
const shots = new Set((strOpt('--shots') || '').split(',').filter(Boolean).map(Number));
const shotDir = strOpt('--shot-dir');
const texStore = new Map(); // id -> {w, h, data}
let glFrames = 0;
function glFrame(p, len) {
  glFrames++;
  const buf = mem.buffer, h = new Uint32Array(buf, p, 24);
  const nup = h[12], upOff = h[13], nfree = h[14], freeOff = h[15];
  const fr = new Uint32Array(buf, p + freeOff, nfree);
  for (const id of fr) texStore.delete(id);
  const up = new Uint32Array(buf, p + upOff, nup * 4);
  for (let i = 0; i < nup; i++) {
    const w = up[4 * i + 1], hh = up[4 * i + 2];
    texStore.set(up[4 * i], { w, h: hh, data: new Uint8Array(buf, up[4 * i + 3], w * hh * 4).slice() });
  }
  const frame = h[2];
  if (!shotDir || !shots.has(frame)) return;
  // dump: header, commands, vertices, uploads (every texture used), fog; then the texture bytes
  const ncmd = h[8], cmdOff = h[9], nvert = h[10], vertOff = h[11], fogOff = h[17];
  const cmds = new Uint32Array(buf, p + cmdOff, ncmd * 8);
  const used = [...new Set(Array.from({ length: ncmd }, (_, i) => cmds[8 * i + 3]).filter((id) => id && texStore.has(id)))];
  const hdr = 96, csz = ncmd * 32, vsz = nvert * 28, usz = used.length * 16, gsz = fogOff ? 512 : 0;
  let size = hdr + csz + vsz + usz + gsz;
  const texAt = used.map((id) => { const at = size; size += texStore.get(id).data.length; return at; });
  const out = new Uint8Array(size), o32 = new Uint32Array(out.buffer);
  out.set(new Uint8Array(buf, p, hdr), 0);
  out.set(new Uint8Array(buf, p + cmdOff, csz), hdr);
  out.set(new Uint8Array(buf, p + vertOff, vsz), hdr + csz);
  used.forEach((id, i) => {
    const t = texStore.get(id), k = (hdr + csz + vsz) / 4 + 4 * i;
    o32[k] = id; o32[k + 1] = t.w; o32[k + 2] = t.h; o32[k + 3] = texAt[i];
    out.set(t.data, texAt[i]);
  });
  if (fogOff) out.set(new Uint8Array(buf, p + fogOff, 512), hdr + csz + vsz + usz);
  o32[9] = hdr; o32[11] = hdr + csz; o32[12] = used.length; o32[13] = hdr + csz + vsz; o32[14] = 0; o32[15] = 0;
  o32[17] = fogOff ? hdr + csz + vsz + usz : 0;
  const name = path.join(shotDir, 'f' + String(frame).padStart(5, '0') + '.ppdl');
  fs.writeFileSync(name, out);
  console.log('shot frame ' + frame + ' cmds ' + ncmd + ' verts ' + nvert + ' textures ' + used.length + ' bytes ' + size);
}
// S6: --input SCRIPT gives port A's pad through rtw_input / rtw_triggers before each rtw_frame, as the retrowser
// host does (same syntax as --pad: "F[+N]:BTN|BTN,..."); --vmu FILE is the A1 image (rtw_save_*), read at start
// and written back whenever rtw_save_generation changes; rtw_rumble calls are logged.
const BITS = { C: 1, B: 2, A: 4, START: 8, UP: 16, DOWN: 32, LEFT: 64, RIGHT: 128, Z: 256, Y: 512, X: 1024, D: 2048 };
const inputScript = (strOpt('--input') || '').split(',').filter(Boolean).map((ev) => {
  const m = ev.match(/^(\d+)(?:\+(\d+))?:(.*)$/);
  if (!m) throw new Error('bad --input event');
  return { from: +m[1], to: +m[1] + (m[2] ? +m[2] : 4), names: m[3].split('|').filter(Boolean) };
});
function hostPad(f) {
  const p = { buttons: 0, sx: 0, sy: 0, l: 0, r: 0 };
  for (const e of inputScript) {
    if (f < e.from || f >= e.to) continue;
    for (const n of e.names) {
      if (n in BITS) p.buttons |= BITS[n];
      else if (n === 'LT') p.l = 255; else if (n === 'RT') p.r = 255;
      else if (n === 'SL') p.sx = -127; else if (n === 'SR') p.sx = 127; else if (n === 'SU') p.sy = 127; else if (n === 'SD') p.sy = -127;
      else throw new Error('bad --input button');
    }
  }
  return p;
}
const vmuPath = strOpt('--vmu');
let rumbles = 0;
const raw = Buffer.alloc(2352);
const env = {
  rtw_rumble(port, on) { rumbles++; if (rumbles <= 16) console.log('boot host rumble port ' + port + ' ' + (on ? 'on' : 'off')); return 0; },
  penpen_gl_frame: (p, len) => glFrame(p, len),
  emscripten_notify_memory_growth() {}, // -sALLOW_MEMORY_GROWTH (decoded textures): views are re-made per call
  rtw_log(p, len) { process.stdout.write(Buffer.from(mem.buffer, p, len).toString('utf8') + '\n'); return 0; },
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
const extra = WebAssembly.Module.imports(mod).filter((i) => !(i.module === 'env' && (i.name.startsWith('rtw_') || i.name === 'penpen_gl_frame' || i.name === 'emscripten_notify_memory_growth')));
for (const i of WebAssembly.Module.imports(mod)) if (i.module === 'env' && !env[i.name]) console.log('boot import not provided by this host: env.' + i.name);
console.log('boot wasm imports outside env.rtw_* and env.penpen_gl_frame: ' + extra.length);
if (shotDir) fs.mkdirSync(shotDir, { recursive: true });
const inst = await WebAssembly.instantiate(mod, imports);
mem = inst.exports.memory;
inst.exports._initialize?.();
inst.exports.rt_set_frames?.(opt('--frames', 3));
inst.exports.rt_set_verbose?.(opt('--verbose', 600));
inst.exports.rt_set_digest?.(opt('--digest', 1));
inst.exports.rt_set_frame_log?.(opt('--frame-log', 1));
inst.exports.rt_set_audio?.(opt('--audio', 1));
{
  const i = rest.indexOf('--pad');
  if (i >= 0) {
    const b = Buffer.from(rest[i + 1] + '\0', 'latin1');
    new Uint8Array(mem.buffer, inst.exports.rt_arg_buf(), b.length).set(b);
    if (inst.exports.rt_set_pad() < 0) { console.log('boot bad pad script'); process.exit(2); }
  }
}
const ex = inst.exports;
let vmuGen = 0;
function vmuStore() {
  if (!vmuPath || ex.rtw_save_generation() === vmuGen) return;
  vmuGen = ex.rtw_save_generation();
  fs.writeFileSync(vmuPath, Buffer.from(new Uint8Array(mem.buffer, ex.rtw_save_ptr(), ex.rtw_save_size())));
  console.log('boot host vmu image written (generation ' + vmuGen + ')');
}
if (vmuPath && fs.existsSync(vmuPath)) {
  const img = fs.readFileSync(vmuPath);
  const n = Math.min(img.length, ex.rtw_save_size());
  new Uint8Array(mem.buffer, ex.rtw_save_ptr(), n).set(img.subarray(0, n));
  ex.rtw_save_load(ex.rtw_save_ptr(), n);
}
// S7: --wav FILE: drain rtw_audio_* after every frame as the host does and write a 44.1 kHz stereo WAV
const wavPath = strOpt('--wav');
const wavParts = [];
let wavFrames = 0;
function drainAudio() {
  const n = ex.rtw_audio_drain?.() ?? 0;
  if (!wavPath || n <= 0) return;
  wavParts.push(Buffer.from(new Uint8Array(mem.buffer, ex.rtw_audio_buffer(), n * 4)));
  wavFrames += n;
}
function wavWrite() {
  if (!wavPath) return;
  const rate = ex.rtw_audio_rate?.() ?? 0, h = Buffer.alloc(44);
  h.write('RIFF', 0); h.writeUInt32LE(36 + wavFrames * 4, 4); h.write('WAVEfmt ', 8); h.writeUInt32LE(16, 16);
  h.writeUInt16LE(1, 20); h.writeUInt16LE(2, 22); h.writeUInt32LE(rate, 24); h.writeUInt32LE(rate * 4, 28);
  h.writeUInt16LE(4, 32); h.writeUInt16LE(16, 34); h.write('data', 36); h.writeUInt32LE(wavFrames * 4, 40);
  fs.writeFileSync(wavPath, Buffer.concat([h, ...wavParts]));
  console.log('boot wav written: ' + wavFrames + ' frames at ' + rate + ' Hz');
}
const t0 = Date.now();
try {
  ex.rtw_start();
  vmuGen = ex.rtw_save_generation();
  for (let f = 0; ; f++) { // S6: stepped, one frame per call, until the frame limit traps
    if (inputScript.length) {
      const p = hostPad(f);
      ex.rtw_input(0, p.buttons, p.sx, p.sy);
      ex.rtw_triggers(0, p.l, p.r);
    }
    ex.rtw_frame();
    vmuStore();
    drainAudio();
  }
} catch (e) {
  try { vmuStore(); drainAudio(); } catch { /* memory gone */ }
  wavWrite();
  console.log('boot wasm stopped (' + (e instanceof WebAssembly.RuntimeError ? 'trap' : 'error') + ') after ' + (Date.now() - t0) + ' ms');
}
