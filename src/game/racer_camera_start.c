/* Pen Pen port, S15 (c05): the racer camera of state 0, the race's intro (func_8C03C804), written fresh from the
 * original for the version in game/racer_camera_update.c whose export is commented out (it failed the difftest
 * through racer_camera_update). This one also keeps the original's frame words: the racer at 0x5C and the
 * scratch word at 0x00 (the start value in step 0, then the chosen vector), and calls the locals' empty
 * constructors. It replaces that racer_camera_start (same name, racer.h declares it). S16: exported (the far point's
 * test was reversed: found by the replay of racer_camera_update's samples, DECOMP.md 17).
 *
 * Step 0 sets the camera up (the eye and the three angle springs reset, the focus at the racer's position, the
 * heading spring at the scene's start value, t = 0) and goes on as step 1: the point is made by njProjectScreen
 * from one of three vectors and t, which grows by 1/360 a frame for 360 frames. In every step the point is then
 * carried through the focus and the heading spring, and the eye is put there. */
#include <string.h>
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h" /* method_self */
#include "game.h"           /* panel_get */
#include "course.h"         /* race_scene_start_value */
#include "race_scene.h"
#include "racer.h"

int32_t race_scene_start_segment(gaddr scene); /* game/stage.c (func_8C03C690), not in a header */

/* 0x8C0C4A80 (Ninja, named by port/symbols/sdk_calls.tsv): makes the camera's point from a vector and t (the
 * same call as racer_camera.c's) */
static inline void njProjectScreen(gaddr v, gaddr out, uint32_t flag, f32 t) {
  FARGS1(t);
  CALL3(0x8C0C4A80u, v, out, flag);
}

/* the vectors the intro camera's point is made from (static data) */
#define START_POINT_FAR     0x8C083198u /* a count of at most 4, or a course that starts past segment 0 */
#define START_POINT_COURSE3 0x8C083168u /* course 3 */
#define START_POINT_OTHER   0x8C083138u /* the others */
#define START_FRAMES 360                /* t runs from 0 to 1 over these frames */
#define START_T_STEP F32_BITS(0x3B360B60u) /* 1/360 */
#define START_FAR_PLAYERS 4
#define START_COURSE3 3

/* the intro camera's frame (0x60 bytes under r8 and PR: 0x68 in all) */
typedef struct StartFrame {
  uint32_t scratch;   /* 0x00: the start value (f32 bits) in step 0, then the chosen vector */
  Matrix m;           /* 0x04 */
  Vec3 point;         /* 0x44: where the eye is put */
  Vec3 projected;     /* 0x50: the point before the matrix */
  gaddr racer;        /* 0x5C */
  uint8_t saved[0x68 - 0x60]; /* 0x60: r8, PR */
} StartFrame;
HW_OFFSET(StartFrame, m, 0x04);
HW_OFFSET(StartFrame, point, 0x44);
HW_OFFSET(StartFrame, projected, 0x50);
HW_OFFSET(StartFrame, racer, 0x5C);
HW_SIZE(StartFrame, 0x68);

/* was func_8C03C804: the camera of state 0 (see above) */
void racer_camera_start(gaddr racer) {
  gaddr frame = hw_stack_alloc(sizeof(StartFrame));
  StartFrame *fr = (StartFrame *)hw_ptr(frame);
  Racer *r = racer_at(racer);
  gaddr m = member_addr(frame, StartFrame, m);
  gaddr projected = member_addr(frame, StartFrame, projected);
  gaddr point = member_addr(frame, StartFrame, point);
  fr->racer = racer;
  method_self(projected); /* the local's (empty) constructor */
  int running = 0;
  if (r->camera_step == 0) {
    body_reset(member_addr(racer, Racer, camera_eye));
    quat_identity(member_addr(racer, Racer, camera_spring[0]));
    quat_identity(member_addr(racer, Racer, camera_spring[1]));
    quat_identity(member_addr(racer, Racer, camera_spring[2]));
    gaddr position = method_self(member_addr(racer, Racer, body));
    memmove(&r->camera_vec4e0, hw_ptr(position), sizeof(Vec3));
    f32 start = race_scene_start_value(panel_get());
    fr->scratch = f32_bits(start);
    r->camera_spring[1].value = start;
    r->camera_offset.x = F32(0.0f); /* t */
    r->camera_f51c = F32(0.0f);
    r->camera_step++;
    running = 1;
  } else if (r->camera_step == 1) {
    running = 1;
  }
  if (running) {
    gaddr source;
    /* S16: the far point unless the set-up's count is above 4 (the S15 versions had this test reversed: the
     * difftest replay of racer_camera_update's samples found it in the time trial's intro) */
    if (race_setup_at(race_setup_get())->player_count <= START_FAR_PLAYERS)
      source = START_POINT_FAR;
    else if (race_scene_start_segment(panel_get()) != 0)
      source = START_POINT_FAR;
    else if (race_setup_at(race_setup_get())->course == START_COURSE3)
      source = START_POINT_COURSE3;
    else
      source = START_POINT_OTHER;
    fr->scratch = source;
    njProjectScreen(source, projected, 0, r->camera_offset.x);
    r->camera_frames++;
    if (r->camera_frames <= START_FRAMES)
      r->camera_offset.x = fadd(r->camera_offset.x, START_T_STEP);
  }
  method_self(point);
  matrix_identity(m);
  njTranslateV(m, member_addr(racer, Racer, camera_vec4e0));
  njRotateY(m, f32_to_int(r->camera_spring[1].value));
  njCalcPoint(m, projected, point);
  vec3_copy_bytes(member_addr(racer, Racer, camera_eye), point);
  hw_stack_free(sizeof(StartFrame));
}
HW_EXPORT(8c03c804, racer_camera_start(ARG0))
