// Pen Pen port, S6: the retrowser glue (files.js, factory createPenpen). Our own code (MIT). tools/rtw.sh
// builds penpen.js = port/render/gl/render_gl.js (defines self.PenpenGL) + this file.
// The module (penpen.wasm, a standalone wasm32 module) imports the host's env.rtw_* functions plus two of
// ours: env.penpen_gl_frame(ptr, len), one frame's draw list (port/render/rd.c), drawn here with WebGL2 on
// the worker's OffscreenCanvas; env.emscripten_notify_memory_growth, a no-op (the memory is read through
// memory.buffer on every call). The host steps the game with rtw_frame (one frame per call: the game's
// njWaitVSync loop is unwound and rewound around it, port/recomp/rt_gen.h). S7: sound needs no glue: the host drains
// the module's rtw_audio_rate / rtw_audio_drain / rtw_audio_buffer exports (passed through below with every rtw_*).
// S8: penpen_set_rtc(seconds since 1950, local) is called once before rtw_start with the browser's clock.
var createPenpen = async function (opts) {
  if (!opts || !opts.wasmBinary) throw new Error('createPenpen: no wasmBinary');
  var gl = null, memory = null, glError = false;
  if (opts.canvas) gl = self.PenpenGL.create(opts.canvas);
  var env = {};
  for (var k in opts.rtw) env[k] = opts.rtw[k];
  env.penpen_gl_frame = function (ptr, len) {
    if (!gl || glError) return;
    try { gl.frame(memory.buffer, ptr >>> 0); } catch (e) {
      glError = true;
      opts.printErr('penpen: drawing failed: ' + (e && e.message));
    }
  };
  env.emscripten_notify_memory_growth = function () {};
  var result = await WebAssembly.instantiate(opts.wasmBinary, { env: env });
  var ex = result.instance.exports;
  memory = ex.memory;
  if (ex._initialize) ex._initialize();
  // S8: the Dreamcast's clock (seconds since 1950-01-01, local time) starts from the browser's clock; the module
  // advances it with the frames. retrowser has no time import (spec 4.9): the glue passes it once, here.
  if (ex.penpen_set_rtc) {
    var now = new Date();
    ex.penpen_set_rtc((Math.floor(now.getTime() / 1000) - now.getTimezoneOffset() * 60 + 631152000) | 0);
  }
  var api = { memory: memory };
  for (var n in ex) if (n.indexOf('rtw_') === 0) api[n] = ex[n];
  return api;
};
