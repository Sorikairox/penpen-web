// Pen Pen port, S5: the WebGL2 backend of the draw list (port/render/rd.c; format in docs/port/RENDER.md 2).
// A classic script (importScripts in the retrowser worker, <script> in the capture page) that defines
// self.PenpenGL. Our own code. It owns a WebGL2 context on a canvas (the host's OffscreenCanvas in the
// worker) and draws one frame per call:
//   const r = PenpenGL.create(canvas);   r.frame(memoryBuffer, ptr);   (r.stats: the last frame's counts)
// A frame is the module's draw list at ptr in memoryBuffer (an ArrayBuffer: the wasm memory, or a dump
// file whose header is at 0). Textures are uploaded once (upload records) and kept until a free record.
(function (root) {
  'use strict';
  const VS = `#version 300 es
in vec2 a_pos; in float a_iw; in vec2 a_uv; in vec4 a_col; in vec4 a_ofs;
uniform vec2 u_size;
out vec2 v_uv; out vec4 v_col; out vec4 v_ofs;
void main() {
  float w = 1.0 / max(a_iw, 1e-8);
  vec2 ndc = vec2(a_pos.x / u_size.x * 2.0 - 1.0, 1.0 - a_pos.y / u_size.y * 2.0);
  gl_Position = vec4(ndc * w, 0.0, w);
  v_uv = a_uv; v_col = a_col.bgra; v_ofs = a_ofs.bgra;
}`;
  // PVR texture shading instructions: 0 decal, 1 modulate, 2 decal alpha, 3 modulate alpha
  const FS = `#version 300 es
precision highp float;
in vec2 v_uv; in vec4 v_col; in vec4 v_ofs;
uniform sampler2D u_tex; uniform int u_hastex, u_shade, u_alpha, u_notexa, u_offset, u_punch;
out vec4 o;
void main() {
  vec4 c = v_col;
  if (u_alpha == 0) c.a = 1.0;
  vec4 p;
  if (u_hastex == 1) {
    vec4 t = texture(u_tex, v_uv);
    if (u_notexa == 1) t.a = 1.0;
    if (u_shade == 0) p = vec4(t.rgb, t.a);
    else if (u_shade == 1) p = vec4(t.rgb * c.rgb, t.a);
    else if (u_shade == 2) p = vec4(mix(c.rgb, t.rgb, t.a), c.a);
    else p = t * c;
  } else p = c;
  if (u_offset == 1) p.rgb += v_ofs.rgb;
  if (u_punch == 1 && p.a < 0.5) discard;
  o = clamp(p, 0.0, 1.0);
  gl_FragDepth = 1.0 / (1.0 + max(gl_FragCoord.w, 0.0));
}`;

  function create(canvas, opts) {
    const gl = canvas.getContext('webgl2', Object.assign({ alpha: false, depth: true, stencil: true, antialias: false,
      premultipliedAlpha: false, preserveDrawingBuffer: true }, opts || {}));
    if (!gl) throw new Error('no WebGL2');
    const sh = (type, src) => {
      const s = gl.createShader(type); gl.shaderSource(s, src); gl.compileShader(s);
      if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) throw new Error('shader: ' + gl.getShaderInfoLog(s));
      return s;
    };
    const prog = gl.createProgram();
    gl.attachShader(prog, sh(gl.VERTEX_SHADER, VS)); gl.attachShader(prog, sh(gl.FRAGMENT_SHADER, FS));
    gl.linkProgram(prog);
    if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) throw new Error('link: ' + gl.getProgramInfoLog(prog));
    const U = {};
    for (const n of ['u_size', 'u_tex', 'u_hastex', 'u_shade', 'u_alpha', 'u_notexa', 'u_offset', 'u_punch']) U[n] = gl.getUniformLocation(prog, n);
    const vao = gl.createVertexArray(), vbo = gl.createBuffer();
    gl.bindVertexArray(vao); gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
    const A = (n) => gl.getAttribLocation(prog, n);
    gl.enableVertexAttribArray(A('a_pos')); gl.vertexAttribPointer(A('a_pos'), 2, gl.FLOAT, false, 28, 0);
    gl.enableVertexAttribArray(A('a_iw')); gl.vertexAttribPointer(A('a_iw'), 1, gl.FLOAT, false, 28, 8);
    gl.enableVertexAttribArray(A('a_uv')); gl.vertexAttribPointer(A('a_uv'), 2, gl.FLOAT, false, 28, 12);
    gl.enableVertexAttribArray(A('a_col')); gl.vertexAttribPointer(A('a_col'), 4, gl.UNSIGNED_BYTE, true, 28, 20);
    gl.enableVertexAttribArray(A('a_ofs')); gl.vertexAttribPointer(A('a_ofs'), 4, gl.UNSIGNED_BYTE, true, 28, 24);
    // the background plane: a quad of its own (three colours, the fourth extrapolated as on the PVR)
    const bgvbo = gl.createBuffer(), bgvao = gl.createVertexArray();
    gl.bindVertexArray(bgvao); gl.bindBuffer(gl.ARRAY_BUFFER, bgvbo);
    gl.enableVertexAttribArray(A('a_pos')); gl.vertexAttribPointer(A('a_pos'), 2, gl.FLOAT, false, 28, 0);
    gl.enableVertexAttribArray(A('a_iw')); gl.vertexAttribPointer(A('a_iw'), 1, gl.FLOAT, false, 28, 8);
    gl.enableVertexAttribArray(A('a_uv')); gl.vertexAttribPointer(A('a_uv'), 2, gl.FLOAT, false, 28, 12);
    gl.enableVertexAttribArray(A('a_col')); gl.vertexAttribPointer(A('a_col'), 4, gl.UNSIGNED_BYTE, true, 28, 20);
    gl.enableVertexAttribArray(A('a_ofs')); gl.vertexAttribPointer(A('a_ofs'), 4, gl.UNSIGNED_BYTE, true, 28, 24);
    gl.bindVertexArray(null);
    const textures = new Map(); // id -> {t, w, h, pot}
    const SRC = [gl.ZERO, gl.ONE, gl.DST_COLOR, gl.ONE_MINUS_DST_COLOR, gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA, gl.DST_ALPHA, gl.ONE_MINUS_DST_ALPHA];
    const DST = [gl.ZERO, gl.ONE, gl.SRC_COLOR, gl.ONE_MINUS_SRC_COLOR, gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA, gl.DST_ALPHA, gl.ONE_MINUS_DST_ALPHA];
    // PVR compares 1/w (greater = nearer); our depth is 1 / (1 + 1/w) (smaller = nearer)
    const ZF = [gl.NEVER, gl.GREATER, gl.EQUAL, gl.GEQUAL, gl.LESS, gl.NOTEQUAL, gl.LEQUAL, gl.ALWAYS];
    const r = { gl, stats: {} };

    function argbClamp(c0, c1, c2) { // c1 + c2 - c0 per channel
      let o = 0;
      for (let s = 0; s < 32; s += 8) {
        const v = ((c1 >>> s) & 255) + ((c2 >>> s) & 255) - ((c0 >>> s) & 255);
        o |= Math.max(0, Math.min(255, v)) << s;
      }
      return o >>> 0;
    }

    r.frame = function (buffer, ptr, opt) {
      const listMask = opt && opt.lists !== undefined ? opt.lists : 15; // debugging: draw only these lists (bit per list)
      const u32 = new Uint32Array(buffer, ptr, 24);
      if (u32[0] !== 0x4C445050) throw new Error('not a draw list');
      const W = u32[3], H = u32[4];
      if (canvas.width !== W || canvas.height !== H) { canvas.width = W; canvas.height = H; }
      const ncmd = u32[8], cmdOff = u32[9], nvert = u32[10], vertOff = u32[11], nup = u32[12], upOff = u32[13];
      const nfree = u32[14], freeOff = u32[15];
      // frees, then uploads
      const fr = new Uint32Array(buffer, ptr + freeOff, nfree);
      for (let i = 0; i < nfree; i++) { const t = textures.get(fr[i]); if (t) { gl.deleteTexture(t.t); textures.delete(fr[i]); } }
      const up = new Uint32Array(buffer, ptr + upOff, nup * 4);
      for (let i = 0; i < nup; i++) {
        const id = up[4 * i], w = up[4 * i + 1], h = up[4 * i + 2], p = up[4 * i + 3];
        const old = textures.get(id); if (old) gl.deleteTexture(old.t);
        const t = gl.createTexture();
        gl.bindTexture(gl.TEXTURE_2D, t);
        gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, w, h, 0, gl.RGBA, gl.UNSIGNED_BYTE, new Uint8Array(buffer, p, w * h * 4));
        const pot = (w & (w - 1)) === 0 && (h & (h - 1)) === 0;
        if (pot) gl.generateMipmap(gl.TEXTURE_2D);
        textures.set(id, { t, w, h, pot });
      }
      gl.viewport(0, 0, W, H);
      gl.disable(gl.SCISSOR_TEST);
      gl.depthMask(true); gl.clearDepth(1.0);
      gl.clearColor(0, 0, 0, 1);
      gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT | gl.STENCIL_BUFFER_BIT);
      gl.useProgram(prog);
      gl.uniform2f(U.u_size, W, H);
      gl.uniform1i(U.u_tex, 0);
      gl.disable(gl.CULL_FACE);
      // background plane (depth far, no depth write)
      {
        const c0 = u32[5], c1 = u32[6], c2 = u32[7], c3 = argbClamp(c0, c1, c2);
        const b = new ArrayBuffer(28 * 6), f = new Float32Array(b), w = new Uint32Array(b);
        const P = [[0, 0, c0], [W, 0, c1], [0, H, c2], [W, 0, c1], [W, H, c3], [0, H, c2]];
        P.forEach((q, i) => { f[7 * i] = q[0]; f[7 * i + 1] = q[1]; f[7 * i + 2] = 1e-6; w[7 * i + 5] = q[2]; });
        gl.bindVertexArray(bgvao); gl.bindBuffer(gl.ARRAY_BUFFER, bgvbo); gl.bufferData(gl.ARRAY_BUFFER, b, gl.STREAM_DRAW);
        gl.disable(gl.DEPTH_TEST); gl.disable(gl.BLEND);
        gl.uniform1i(U.u_hastex, 0); gl.uniform1i(U.u_alpha, 0); gl.uniform1i(U.u_offset, 0); gl.uniform1i(U.u_punch, 0);
        gl.drawArrays(gl.TRIANGLES, 0, 6);
      }
      gl.bindVertexArray(vao); gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
      gl.bufferData(gl.ARRAY_BUFFER, new Uint8Array(buffer, ptr + vertOff, nvert * 28), gl.STREAM_DRAW);
      gl.enable(gl.DEPTH_TEST); gl.enable(gl.SCISSOR_TEST);
      const C = new Uint32Array(buffer, ptr + cmdOff, ncmd * 8);
      let draws = 0;
      const shadow = u32[22] ? (u32[22] & 255) / 256 : 160 / 256; // cheap shadow intensity (njSetCheapShadowMode)
      // pass 0: opaque + punch-through; pass 1: opaque modifier volumes (stencil parity) and the darkening;
      // pass 2: translucent (already sorted back to front)
      for (let pass = 0; pass < 3; pass++) {
        let vols = 0;
        if (pass === 1) {
          gl.enable(gl.STENCIL_TEST); gl.colorMask(false, false, false, false); gl.depthMask(false);
          gl.stencilFunc(gl.ALWAYS, 0, 1); gl.stencilOp(gl.KEEP, gl.KEEP, gl.INVERT); gl.depthFunc(gl.LESS);
          gl.disable(gl.BLEND); gl.uniform1i(U.u_hastex, 0); gl.uniform1i(U.u_punch, 0);
        }
        for (let i = 0; i < ncmd; i++) {
          const list = C[8 * i], first = C[8 * i + 1], count = C[8 * i + 2], tid = C[8 * i + 3], m = C[8 * i + 4];
          if ((pass === 0 && list !== 0 && list !== 3) || (pass === 1 && list !== 1) || (pass === 2 && list !== 2)) continue;
          const x0 = C[8 * i + 5] & 0xFFFF, y0 = C[8 * i + 5] >>> 16, x1 = C[8 * i + 6] & 0xFFFF, y1 = C[8 * i + 6] >>> 16;
          if (x1 <= x0 || y1 <= y0 || !count || !((listMask >> list) & 1)) continue;
          gl.scissor(x0, H - y1, x1 - x0, y1 - y0);
          if (pass === 1) { gl.drawArrays(gl.TRIANGLES, first, count); vols++; draws++; continue; }
          gl.depthFunc(ZF[(m >>> 7) & 7]);
          gl.depthMask(list === 2 ? ((m >>> 6) & 1) === 1 : true);
          if (list === 2) { gl.enable(gl.BLEND); gl.blendFunc(SRC[m & 7], DST[(m >>> 3) & 7]); } else gl.disable(gl.BLEND);
          const t = tid ? textures.get(tid) : null;
          gl.uniform1i(U.u_hastex, t ? 1 : 0);
          if (t) {
            gl.activeTexture(gl.TEXTURE0); gl.bindTexture(gl.TEXTURE_2D, t.t);
            const bil = (m >>> 10) & 1;
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, bil ? gl.LINEAR : gl.NEAREST);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, t.pot ? (bil ? gl.LINEAR_MIPMAP_NEAREST : gl.NEAREST_MIPMAP_NEAREST) : (bil ? gl.LINEAR : gl.NEAREST));
            const wrap = (clamp, flip) => clamp || !t.pot ? gl.CLAMP_TO_EDGE : flip ? gl.MIRRORED_REPEAT : gl.REPEAT;
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, wrap((m >>> 11) & 1, (m >>> 13) & 1));
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, wrap((m >>> 12) & 1, (m >>> 14) & 1));
          }
          gl.uniform1i(U.u_shade, (m >>> 15) & 3);
          gl.uniform1i(U.u_alpha, (m >>> 17) & 1);
          gl.uniform1i(U.u_notexa, (m >>> 18) & 1);
          gl.uniform1i(U.u_offset, (m >>> 19) & 1);
          gl.uniform1i(U.u_punch, list === 3 ? 1 : 0);
          gl.drawArrays(gl.TRIANGLES, first, count);
          draws++;
        }
        if (pass === 1) {
          gl.colorMask(true, true, true, true);
          if (vols) { // darken where the parity is odd: the opaque surface is inside a volume
            gl.disable(gl.SCISSOR_TEST); gl.disable(gl.DEPTH_TEST);
            gl.stencilFunc(gl.EQUAL, 1, 1); gl.stencilOp(gl.KEEP, gl.KEEP, gl.KEEP);
            gl.enable(gl.BLEND); gl.blendFunc(gl.ZERO, gl.SRC_COLOR);
            const b = new ArrayBuffer(28 * 6), f = new Float32Array(b), w = new Uint32Array(b);
            const k = Math.round(shadow * 255), col = (0xFF000000 | (k << 16) | (k << 8) | k) >>> 0;
            [[0, 0], [W, 0], [0, H], [W, 0], [W, H], [0, H]].forEach((q, j) => { f[7 * j] = q[0]; f[7 * j + 1] = q[1]; f[7 * j + 2] = 1; w[7 * j + 5] = col; });
            gl.bindVertexArray(bgvao); gl.bindBuffer(gl.ARRAY_BUFFER, bgvbo); gl.bufferData(gl.ARRAY_BUFFER, b, gl.STREAM_DRAW);
            gl.uniform1i(U.u_hastex, 0); gl.uniform1i(U.u_alpha, 0); gl.uniform1i(U.u_offset, 0);
            gl.drawArrays(gl.TRIANGLES, 0, 6);
            gl.bindVertexArray(vao); gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
            gl.enable(gl.SCISSOR_TEST); gl.enable(gl.DEPTH_TEST);
          }
          gl.disable(gl.STENCIL_TEST);
        }
      }
      gl.disable(gl.SCISSOR_TEST);
      r.stats = { frame: u32[2], cmds: ncmd, verts: nvert, uploads: nup, textures: textures.size, draws, op: u32[18], trTris: u32[19], omTris: u32[20], culled: u32[21] };
      return r.stats;
    };
    return r;
  }
  root.PenpenGL = { create };
})(typeof self !== 'undefined' ? self : globalThis);
