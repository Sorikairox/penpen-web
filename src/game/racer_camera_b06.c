/* Pen Pen port, S18 b06: the racer's last two camera modes (func_8C03C99C for state 1, func_8C03D2C4 for state 5;
 * the racer's camera update func_8C03C69E picks them; the others are game/racer_camera.c's).
 *
 * As in racer_camera.c the matrix and the points live in a guest frame of the original's layout (the Ninja calls get
 * their addresses). */
#include <string.h>
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "game.h"     /* panel_get */
#include "actor_b05.h" /* race_scene_finish_position, race_scene_finish_value */
#include "racer.h"
#include "racer_camera_b06.h"

/* 0x8C0C4A80 (Ninja, named by port/symbols/sdk_calls.tsv): makes the camera's point from a vector and t (the same
 * call as racer_camera.c's) */
static inline void njProjectScreen(gaddr v, gaddr out, uint32_t flag, f32 t) {
  FARGS1(t);
  CALL3(0x8C0C4A80u, v, out, flag);
}

/* ---- state 1: the camera over the finish ---- */

#define FINISH_POINT_SOURCE_A 0x8C0831C8u /* step 1's vector */
#define FINISH_POINT_SOURCE_B 0x8C0831F8u /* step 2's */
#define FINISH_STEP1_FRAMES   720         /* step 1 runs this long, then step 2 */
#define FINISH_T_STEP1        F32_BITS(0x3C088888u) /* 0.008333334: 1/120 a frame, up to 1 */
#define FINISH_T_STEP2        F32_BITS(0x3A11A2B3u) /* 0.00055555557: 1/1800 a frame, back to 0 at 1 */
#define FINISH_EYE_DAMPING    F32_BITS(0x3F733333u) /* 0.95 */
#define FINISH_ENDS_A         3                     /* the two finish positions the focus is between */
#define FINISH_ENDS_B         4

/* the frame (0x78 bytes under PR) */
typedef struct FinishCameraFrame {
  uint32_t scratch;   /* 0x00 */
  Matrix m;           /* 0x04 */
  Vec3 point;         /* 0x44: the eye's place */
  Vec3 projected;     /* 0x50: the point the curve gives (left as it is past step 2) */
  Vec3 middle;        /* 0x5C: the focus: halfway between the two finish positions */
  Vec3 end_a;         /* 0x68 */
  gaddr racer;        /* 0x74 */
  uint32_t saved_pr;  /* 0x78 */
} FinishCameraFrame;
HW_OFFSET(FinishCameraFrame, m, 0x04);
HW_OFFSET(FinishCameraFrame, point, 0x44);
HW_OFFSET(FinishCameraFrame, projected, 0x50);
HW_OFFSET(FinishCameraFrame, middle, 0x5C);
HW_OFFSET(FinishCameraFrame, end_a, 0x68);
HW_SIZE(FinishCameraFrame, 0x7C);

/* was func_8C03C99C: the camera of racer state 1: the focus halfway between the finish positions 3 and 4 of the
 * panel's scene; step 0 sets the heading spring to the scene's finish value, t to 0 and the eye's damping, then
 * step 1 runs the first curve over 720 frames (t up by 1/120 a frame, held at 1) and step 2 the second one (t up by
 * 1/1800, back to 0 at 1). The eye is put at the curve's point turned by the heading about the focus. */
void racer_camera_finish(gaddr racer) {
  Racer *r = racer_at(racer);
  gaddr frame = hw_stack_alloc(sizeof(FinishCameraFrame));
  gaddr m = member_addr(frame, FinishCameraFrame, m);
  gaddr point = member_addr(frame, FinishCameraFrame, point);
  gaddr projected = member_addr(frame, FinishCameraFrame, projected);
  gaddr middle = member_addr(frame, FinishCameraFrame, middle);
  gaddr end_a = member_addr(frame, FinishCameraFrame, end_a);

  race_scene_finish_position(panel_get(), FINISH_ENDS_A, end_a);
  race_scene_finish_position(panel_get(), FINISH_ENDS_B, middle);
  vec3_sub(middle, end_a);
  vec3_scale(F32(0.5f), middle);
  vec3_add(middle, end_a);
  memmove(&r->camera_vec4e0, hw_ptr(middle), sizeof(Vec3));

  switch (r->camera_step) {
  case 0:
    r->camera_spring[1].value = race_scene_finish_value(panel_get());
    r->camera_offset.x = F32(0.0f); /* t */
    vec3_fill(member_addr(racer, Racer, camera_eye.damping), FINISH_EYE_DAMPING);
    r->camera_f51c = F32(0.0f);
    r->camera_step++;
    /* fall through */
  case 1:
    njProjectScreen(FINISH_POINT_SOURCE_A, projected, 0, r->camera_offset.x);
    r->camera_frames++;
    if (r->camera_frames > FINISH_STEP1_FRAMES) {
      r->camera_frames = 0;
      r->camera_step++;
      r->camera_offset.x = F32(0.0f);
    } else if (fgt(F32(1.0f), r->camera_offset.x)) {
      r->camera_offset.x = fadd(r->camera_offset.x, FINISH_T_STEP1);
    } else {
      r->camera_offset.x = F32(1.0f);
    }
    break;
  case 2:
    njProjectScreen(FINISH_POINT_SOURCE_B, projected, 0, r->camera_offset.x);
    r->camera_offset.x = fadd(r->camera_offset.x, FINISH_T_STEP2);
    if (!fgt(F32(1.0f), r->camera_offset.x)) r->camera_offset.x = F32(0.0f);
    break;
  default:
    break;
  }

  matrix_identity(m);
  njTranslateV(m, member_addr(racer, Racer, camera_vec4e0));
  njRotateY(m, f32_to_int(r->camera_spring[1].value));
  njCalcPoint(m, projected, point);
  vec3_copy_bytes(member_addr(racer, Racer, camera_eye), point);
  hw_stack_free(sizeof(FinishCameraFrame));
}
HW_EXPORT(8c03c99c, racer_camera_finish(ARG0))

/* ---- state 5: the swimming camera ---- */

/* the frame (0x5C bytes under fr12 and PR): racer_camera.c's SpringFrame */
typedef struct SwimCameraFrame {
  Vec3 rates;         /* 0x00: the eye's pull per axis */
  Matrix m;           /* 0x0C */
  Vec3 point;         /* 0x4C: the eye's target */
  gaddr racer;        /* 0x58 */
  uint8_t saved[8];   /* 0x5C: fr12, PR */
} SwimCameraFrame;
HW_OFFSET(SwimCameraFrame, m, 0x0C);
HW_OFFSET(SwimCameraFrame, point, 0x4C);
HW_SIZE(SwimCameraFrame, 0x64);

#define SWIM_UP          F32_BITS(0x3F7FFFFFu) /* 0.99999994: camera_vec498's y */
#define SWIM_SPRING_RATE F32_BITS(0x3D4CCCCCu) /* 0.05 */
#define SWIM_FRONT_PULL  F32_BITS(0x3ECCCCCCu) /* 0.4 */
#define SWIM_BEHIND_PULL F32_BITS(0x3DCCCCCCu) /* 0.1 */

/* was func_8C03D2C4: the camera of racer state 5: like the high camera (racer_camera.c) 25 behind (50 when word528
 * is 1, then seen from the front), the pitch spring set level and pulled to f578, a little up (0.99999994); the
 * heading spring 0.75 damped, the others 0.5; the springs pull at 0.05, the eye (damping 0.5) at 0.4 from the front,
 * else 0.1 */
void racer_camera_swim(gaddr racer) {
  Racer *r = racer_at(racer);
  gaddr frame = hw_stack_alloc(sizeof(SwimCameraFrame));
  gaddr m = member_addr(frame, SwimCameraFrame, m);
  gaddr point = member_addr(frame, SwimCameraFrame, point);
  gaddr rates = member_addr(frame, SwimCameraFrame, rates);
  gaddr eye = member_addr(racer, Racer, camera_eye);

  r->camera_distance = r->word528 == 1 ? F32(-50.0f) : F32(-25.0f);
  vec3_set(F32(0.0f), F32(0.0f), r->camera_distance, member_addr(racer, Racer, camera_offset));
  r->camera_spring[0].value = F32(0.0f);
  vec3_set(F32(0.0f), SWIM_UP, F32(0.0f), member_addr(racer, Racer, camera_vec498));
  r->camera_spring[0].damping = F32(0.5f);
  r->camera_spring[1].damping = F32(0.75f);
  r->camera_spring[2].damping = F32(0.5f);

  body1_pull_angle(fadd(r->camera_f578, F32(0.0f)), SWIM_SPRING_RATE, member_addr(racer, Racer, camera_spring[0]));
  body1_pull_angle(fadd(r->camera_f57c, r->heading.value), SWIM_SPRING_RATE,
                   member_addr(racer, Racer, camera_spring[1]));
  body1_pull_angle(fadd(r->camera_f580, r->value34c.value), SWIM_SPRING_RATE,
                   member_addr(racer, Racer, camera_spring[2]));
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
  if (r->word528 == 1) {
    r->camera_f57c = F32(32768.0f);
    vec3_set(F32(0.5f), F32(0.5f), F32(0.5f), member_addr(eye, Body, damping));
    vec3_set(SWIM_FRONT_PULL, SWIM_FRONT_PULL, SWIM_FRONT_PULL, rates);
  } else {
    r->camera_f57c = F32(0.0f);
    vec3_set(F32(0.5f), F32(0.5f), F32(0.5f), member_addr(eye, Body, damping));
    vec3_set(SWIM_BEHIND_PULL, SWIM_BEHIND_PULL, SWIM_BEHIND_PULL, rates);
  }
  body_pull_axes(eye, point, rates);
  body_step(eye);
  r->camera_f51c = F32(0.0f);
  hw_stack_free(sizeof(SwimCameraFrame));
}
HW_EXPORT(8c03d2c4, racer_camera_swim(ARG0))
