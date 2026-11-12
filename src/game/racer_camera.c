/* Pen Pen port, S14 (batch b08): three of the racer's camera modes (func_8C03CBC2, func_8C03CD4C, func_8C03D02A;
 * the racer's camera update func_8C03C69E picks one).
 *
 * The camera's eye is a Body (Racer.camera_eye) pulled toward a point made with a Ninja matrix: from the racer's
 * position (or the focus camera_vec4e0) the matrix turns by three angle springs (Body1 camera_spring[0..2]: pitch,
 * heading, roll in Ninja angle units, each pulled toward a target angle) and the eye's offset (camera_offset, the
 * distance behind) is carried through it. The matrix and the points live in the original's stack frame (the Ninja
 * calls get their addresses), so each mode keeps a guest frame of the original's layout. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "race_scene.h"
#include "racer.h"
#include "scenery.h"

/* 0x8C0C4A80 (Ninja, named by port/symbols/sdk_calls.tsv): makes the intro's camera point from a vector and t */
static inline void njProjectScreen(gaddr v, gaddr out, uint32_t flag, f32 t) {
  FARGS1(t);
  CALL3(0x8C0C4A80u, v, out, flag);
}

#define INTRO_POINT_SOURCE 0x8C083228u /* the vector the intro camera's point is made from */
#define INTRO_FRAMES 180               /* the intro's t runs from 0 to 1 over these frames */

/* the intro camera's frame (0x68 bytes, under the five registers the original saves: 0x7C in all) */
typedef struct IntroFrame {
  gaddr scratch;      /* 0x00 */
  Vec3 delta;         /* 0x04: from the eye to the point */
  Matrix m;           /* 0x10 */
  Vec3 point;         /* 0x50: the point the eye is pulled to */
  Vec3 projected;     /* 0x5C: the point before the matrix */
  uint8_t saved[0x7C - 0x68]; /* 0x68: r14, r13, fr15, fr14, PR */
} IntroFrame;
HW_OFFSET(IntroFrame, delta, 0x04);
HW_OFFSET(IntroFrame, m, 0x10);
HW_OFFSET(IntroFrame, point, 0x50);
HW_OFFSET(IntroFrame, projected, 0x5C);
HW_SIZE(IntroFrame, 0x7C);

/* was func_8C03CBC2: the race's intro camera. Step 0 sets it up (t = 0, the eye's damping 0.9); while in step 1
 * the focus follows the racer, the heading spring takes the racer's heading and the camera point is made from
 * t, which grows by 1/180 a frame for 180 frames. Then the point is carried through the focus and the heading, and
 * the eye is pulled 1/100 of the way to it and stepped. */
void racer_camera_intro(gaddr racer) {
  gaddr frame = hw_stack_alloc(sizeof(IntroFrame));
  Racer *r = racer_at(racer);
  gaddr eye = member_addr(racer, Racer, camera_eye);
  gaddr m = member_addr(frame, IntroFrame, m);
  gaddr projected = member_addr(frame, IntroFrame, projected);
  gaddr point = member_addr(frame, IntroFrame, point);
  gaddr delta = member_addr(frame, IntroFrame, delta);
  int running = 0;
  if (r->camera_step == 0) {
    r->camera_offset.x = F32(0.0f); /* t */
    r->camera_eye.damping.z = F32_BITS(0x3F666666u); /* 0.9 */
    r->camera_eye.damping.y = F32_BITS(0x3F666666u);
    r->camera_eye.damping.x = F32_BITS(0x3F666666u);
    r->camera_f51c = F32(0.0f);
    r->camera_step++;
    running = 1;
  } else if (r->camera_step == 1) {
    running = 1;
  }
  if (running) {
    r->camera_vec4e0 = r->body.body.position;
    r->camera_spring[1].value = r->heading.value;
    njProjectScreen(INTRO_POINT_SOURCE, projected, 0, r->camera_offset.x);
    r->camera_frames++;
    if (r->camera_frames <= INTRO_FRAMES)
      r->camera_offset.x = fadd(r->camera_offset.x, F32_BITS(0x3BB60B60u)); /* 1/180 */
  }
  matrix_identity(m);
  njTranslateV(m, member_addr(racer, Racer, camera_vec4e0));
  njRotateY(m, f32_to_int(r->camera_spring[1].value));
  njCalcPoint(m, projected, point);
  vec3_copy(delta, point);
  vec3_sub(delta, eye);
  vec3_scale(F32_BITS(0x3C23D70Au) /* 0.01 */, delta);
  vec3_add(member_addr(eye, Body, force), delta);
  body_step(eye);
  hw_stack_free(sizeof(IntroFrame));
}
HW_EXPORT(8c03cbc2, racer_camera_intro(ARG0))

/* the frame of the two spring cameras (follow: 0x60 bytes, high: 0x5C; the racer is kept at the top) */
typedef struct SpringFrame {
  Vec3 rates;         /* 0x00: the eye's pull per axis */
  Matrix m;           /* 0x0C */
  Vec3 point;         /* 0x4C: the eye's target */
  uint8_t locals[8];  /* 0x58 */
} SpringFrame;
HW_OFFSET(SpringFrame, m, 0x0C);
HW_OFFSET(SpringFrame, point, 0x4C);

/* what the two spring cameras share once the targets are set: the springs pull toward (f578 + pitch, f57c +
 * heading, f580 + roll) at `rate` and step, the matrix goes to the racer, up by camera_vec498, (its place is the
 * focus), turns by heading, pitch and roll; the offset carried through it is the eye's target. A racer whose
 * word528 is 1 is seen from the front (f57c = 0x8000, half a turn), with the `front` pulls, else the `behind`
 * ones; the eye's damping is 0.75 */
static void spring_camera_step(gaddr racer, gaddr frame, f32 pitch, f32 rate, f32 front_xz, f32 front_y,
                               f32 behind_xz, f32 behind_y) {
  Racer *r = racer_at(racer);
  gaddr m = member_addr(frame, SpringFrame, m);
  gaddr point = member_addr(frame, SpringFrame, point);
  gaddr rates = member_addr(frame, SpringFrame, rates);
  gaddr eye = member_addr(racer, Racer, camera_eye);
  body1_pull_angle(fadd(r->camera_f578, pitch), rate, member_addr(racer, Racer, camera_spring[0]));
  body1_pull_angle(fadd(r->camera_f57c, r->heading.value), rate, member_addr(racer, Racer, camera_spring[1]));
  body1_pull_angle(fadd(r->camera_f580, r->value34c.value), rate, member_addr(racer, Racer, camera_spring[2]));
  body1_step(member_addr(racer, Racer, camera_spring[0]));
  body1_step(member_addr(racer, Racer, camera_spring[1]));
  body1_step(member_addr(racer, Racer, camera_spring[2]));
  matrix_identity(m);
  njTranslateV(m, member_addr(racer, Racer, body));
  njTranslateV(m, member_addr(racer, Racer, camera_vec498));
  vec3_copy(member_addr(racer, Racer, camera_vec4e0), member_addr(m, Matrix, m[12]));
  njRotateY(m, f32_to_int(r->camera_spring[1].value));
  njRotateX(m, f32_to_int(r->camera_spring[0].value));
  njRotateZ(m, f32_to_int(r->camera_spring[2].value));
  njCalcPoint(m, member_addr(racer, Racer, camera_offset), point);
  const f32 damping = F32(0.75f);
  if (r->word528 == 1) {
    r->camera_f57c = F32(32768.0f);
    vec3_set(damping, damping, damping, member_addr(eye, Body, damping));
    vec3_set(front_xz, front_y, front_xz, rates);
  } else {
    r->camera_f57c = F32(0.0f);
    vec3_set(damping, damping, damping, member_addr(eye, Body, damping));
    vec3_set(behind_xz, behind_y, behind_xz, rates);
  }
  body_pull_axes(eye, point, rates);
  body_step(eye);
  r->camera_f51c = F32(0.0f);
}

/* was func_8C03CD4C: the follow camera: 25 behind (35 unless the racer's action is 3, while word1ac is 0), 1.25
 * up, the heading spring 0.35 damped, the others 0.125; the pitch target is 0x10000/30 while word1ac is 0, else
 * 0x10000/24 on course 2 (on course 3 before cell 205), else 0 */
void racer_camera_follow(gaddr racer) {
  gaddr frame = hw_stack_alloc(0x60);
  Racer *r = racer_at(racer);
  f32 pitch;
  if (racer_word1ac(racer) != 0) {
    r->camera_distance = F32(-25.0f);
    int32_t course = race_setup_at(race_setup_get())->course;
    if (course == 2) {
      pitch = F32_BITS(0x452AAAAAu); /* 2730.6665: 0x10000 / 24 */
    } else if (course == 3) {
      int32_t cell = (int32_t)race_player_cell(r->scene, r->index);
      pitch = cell >= 205 ? F32(0.0f) : F32_BITS(0x452AAAAAu);
    } else {
      pitch = F32(0.0f);
    }
  } else {
    r->camera_distance = r->action == 3 ? F32(-25.0f) : F32(-35.0f);
    pitch = F32_BITS(0x45088888u); /* 2184.533: 0x10000 / 30 */
  }
  vec3_set(F32(0.0f), F32(0.0f), r->camera_distance, member_addr(racer, Racer, camera_offset));
  vec3_set(F32(0.0f), F32(1.25f), F32(0.0f), member_addr(racer, Racer, camera_vec498));
  r->camera_spring[0].damping = F32(0.125f);
  r->camera_spring[1].damping = F32_BITS(0x3EB33333u); /* 0.35 */
  r->camera_spring[2].damping = F32(0.125f);
  spring_camera_step(racer, frame, pitch, F32(0.0625f), F32_BITS(0x3F666665u) /* 0.9 */,
                     F32_BITS(0x3EE66665u) /* 0.45 */, F32_BITS(0x3E199999u) /* 0.15 */,
                     F32_BITS(0x3D999999u) /* 0.075 */);
  hw_stack_free(0x60);
}
HW_EXPORT(8c03cd4c, racer_camera_follow(ARG0))

/* was func_8C03D02A: the high camera: 25 behind (50 when word528 is 1), 1.25 up, pitched down 637 (0x10000 * 35 /
 * 3600) with the pitch spring set there; the heading spring 0.75 damped, the others 0.25; the springs pull at 0.05 */
void racer_camera_high(gaddr racer) {
  gaddr frame = hw_stack_alloc(0x5C);
  Racer *r = racer_at(racer);
  const f32 pitch = F32_BITS(0x441F49F4u); /* 637.1555 */
  r->camera_distance = r->word528 == 1 ? F32(-50.0f) : F32(-25.0f);
  vec3_set(F32(0.0f), F32(0.0f), r->camera_distance, member_addr(racer, Racer, camera_offset));
  r->camera_spring[0].value = pitch;
  vec3_set(F32(0.0f), F32(1.25f), F32(0.0f), member_addr(racer, Racer, camera_vec498));
  r->camera_spring[0].damping = F32(0.25f);
  r->camera_spring[1].damping = F32(0.75f);
  r->camera_spring[2].damping = F32(0.25f);
  spring_camera_step(racer, frame, pitch, F32_BITS(0x3D4CCCCCu) /* 0.05 */, F32_BITS(0x3E4CCCCCu) /* 0.2 */,
                     F32_BITS(0x3DCCCCCCu) /* 0.1 */, F32_BITS(0x3D4CCCCCu) /* 0.05 */,
                     F32_BITS(0x3CCCCCCCu) /* 0.025 */);
  hw_stack_free(0x5C);
}
HW_EXPORT(8c03d02a, racer_camera_high(ARG0))
