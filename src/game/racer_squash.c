/* Pen Pen port, S15 (b06): the racer's squash and stretch (func_8C03709C).
 *
 * The racer is drawn scaled by its three springs value35c[0..2] along the axis vec39c (racer_draw). A new mark
 * (mark_a, mark_b, mark_d: set this update, not the one before) kicks the springs (two up, one down) and sets the
 * axis; while mark_c is set they are kicked the other way. The springs are then pulled back to 1 and stepped, and
 * the five marks are moved to marks_prev and cleared. */
#include "hw.h"
#include "../core/vec.h"
#include "../core/ninja.h"
#include "racer.h"

#define RACER_MARKS 5 /* marks[] and marks_prev[] (0x148, 0x15C) */

/* the original's frame (0x1C bytes under PR): the normalized direction at its bottom */
typedef struct SquashFrame {
  Vec3 dir;                   /* 0x00 */
  uint8_t rest[0x20 - 0x0C];  /* 0x0C: (unused words, PR) */
} SquashFrame;
HW_SIZE(SquashFrame, 0x20);

/* a kick of the three springs (added to their force) and their damping */
static void squash_kick(gaddr racer, f32 kick0, f32 kick1, f32 kick2, f32 damping) {
  Racer *r = racer_at(racer);
  f32_add_to(kick0, member_addr(racer, Racer, value35c[0].force));
  f32_add_to(kick1, member_addr(racer, Racer, value35c[1].force));
  f32_add_to(kick2, member_addr(racer, Racer, value35c[2].force));
  r->value35c[0].damping = damping;
  r->value35c[1].damping = damping;
  r->value35c[2].damping = damping;
}

#define KICK_UP     F32_BITS(0x3E999999u) /* 0.3 */
#define KICK_DOWN   F32_BITS(0xBE999999u) /* -0.3 */
#define KICK_SMALL  F32_BITS(0xBC23D70Au) /* -0.01 */
#define DAMPING_NEW F32_BITS(0x3F4CCCCCu) /* 0.8 */
#define DAMPING_C   F32_BITS(0x3ECCCCCCu) /* 0.4 */
#define PULL_RATE   F32_BITS(0x3E999999u) /* 0.3 */

/* was func_8C03709C: the squash update (see above). mark_a squashes along z, mark_b along the normalized vec260,
 * mark_d along vec0bc, mark_c along z */
void racer_squash_update(gaddr racer) {
  Racer *r = racer_at(racer);
  gaddr axis = member_addr(racer, Racer, vec39c);
  if (r->marks_prev[0] == 0 && r->mark_a != 0) {
    squash_kick(racer, KICK_UP, KICK_UP, KICK_DOWN, DAMPING_NEW);
    vec3_set(F32(0.0f), F32(0.0f), F32(1.0f), axis);
  }
  if (r->marks_prev[1] == 0 && r->mark_b != 0) {
    squash_kick(racer, KICK_UP, KICK_UP, KICK_DOWN, DAMPING_NEW);
    gaddr frame = hw_stack_alloc(sizeof(SquashFrame));
    gaddr dir = member_addr(frame, SquashFrame, dir);
    vec3_normalized(member_addr(racer, Racer, vec260), dir);
    r->vec39c = *(const Vec3 *)hw_ptr(dir);
    hw_stack_free(sizeof(SquashFrame));
  }
  if (r->marks_prev[3] == 0 && r->mark_d != 0) {
    squash_kick(racer, KICK_UP, KICK_UP, KICK_DOWN, DAMPING_NEW);
    r->vec39c = r->vec0bc;
  }
  if (r->mark_c != 0) {
    squash_kick(racer, KICK_SMALL, KICK_SMALL, KICK_UP, DAMPING_C);
    vec3_set(F32(0.0f), F32(0.0f), F32(1.0f), axis);
  }
  for (int32_t i = 0; i < 3; i++)
    body1_pull(F32(1.0f), PULL_RATE, member_addr(racer, Racer, value35c[i]));
  for (int32_t i = 0; i < 3; i++)
    body1_step(member_addr(racer, Racer, value35c[i]));
  for (int32_t i = 0; i < RACER_MARKS; i++) {
    r->marks_prev[i] = r->marks[i];
    r->marks[i] = 0;
  }
}
HW_EXPORT(8c03709c, racer_squash_update(ARG0))
