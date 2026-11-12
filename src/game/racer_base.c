/* Pen Pen port, S15 (b04): the racer's base construction (func_8C030BF4), shared by the Racer (racer_construct,
 * game/racer_control.c) and the LaneRacer (lane_racer_construct, game/lane_racer.c).
 *
 * It builds the Task, the character (its model and motion player, by character and costume), the collider (at the
 * moving body's position, with the character's radius, anchored at vec2ac), the moving bodies and their values,
 * the second Contacts (4, no owner) and the camera's bodies, picks the character's four looks, and adds the
 * racer's effects as a child task. (The original also runs the empty constructors of its plain Vec3 members:
 * nothing to do.) */
#include "hw.h"
#include "sdk.h"
#include "../core/task.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "racer.h"
#include "motion.h"
#include "effects.h"

/* the looks of each save_byte44 value (4 signed bytes per row) */
#define LOOK_TABLE ((const int8_t (*)[4])hw_ptr(0x8C085514u))
#define RACER_EFFECTS_SIZE 0x3C /* racer_effects_construct's object */
enum { CHARACTER_WITH_SHORT_LOOKS = 2 };

/* was func_8C030BF4: the base construction (index: the racer's slot; b and the float are not read; costume from
 * the stack); returns the racer */
gaddr racer_base_init(gaddr racer, uint32_t index, gaddr unused_b, int32_t character, f32 unused_f, int32_t costume) {
  Racer *r = racer_at(racer);
  const CharacterParams *params = (const CharacterParams *)hw_ptr(CHARACTER_PARAMS) + character;
  task_construct(racer, 0);
  motion_player_build(member_addr(racer, Racer, second_base), character, costume, 0, 0);
  collider_construct(member_addr(racer, Racer, collider_data), member_addr(racer, Racer, body),
                     params->radius, member_addr(racer, Racer, body.body.velocity), member_addr(racer, Racer, vec2ac));
  frame_body_init(member_addr(racer, Racer, body));
  body_init(member_addr(racer, Racer, body2fc));
  quat_identity(member_addr(racer, Racer, heading));
  quat_identity(member_addr(racer, Racer, value33c));
  quat_identity(member_addr(racer, Racer, value34c));
  for (int k = 0; k < 3; k++)
    quat_identity(member_addr(racer, Racer, value35c[k]));
  quat_identity(member_addr(racer, Racer, value38c));
  matrix_identity(member_addr(racer, Racer, matrix3a8));
  matrix_identity(member_addr(racer, Racer, matrix3e8));
  r->index = (int32_t)index;
  r->params = hw_addr(params);
  contacts_construct(member_addr(racer, Racer, contacts2), 0, F32(0.0f), 4);
  for (int k = 0; k < 3; k++)
    quat_identity(member_addr(racer, Racer, camera_spring[k]));
  body_init(member_addr(racer, Racer, camera_eye));
  r->child = 0;
  if (r->costume == 0) {
    r->looks[0] = LOOK_TABLE[save_byte44(r->character)][0];
    r->looks[1] = LOOK_TABLE[save_byte44(r->character)][1];
    r->looks[2] = LOOK_TABLE[save_byte44(r->character)][2];
    r->looks[3] = LOOK_TABLE[save_byte44(r->character)][3];
    if (r->character == CHARACTER_WITH_SHORT_LOOKS) {
      if (r->looks[2] == 1)
        r->looks[2] = 2;
      if (r->looks[3] == 1)
        r->looks[3] = 2;
    }
  } else {
    r->looks[0] = 0;
    r->looks[1] = 0;
    r->looks[2] = 1;
    r->looks[3] = 1;
  }
  r->word530 = 0; /* stored as the float 0 */
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(racer, Racer, vec534));
  vec3_set(F32(0.0f), F32(1.0f), F32(0.0f), member_addr(racer, Racer, vec540));
  gaddr mem = syMalloc(RACER_EFFECTS_SIZE);
  gaddr effects = mem != 0 ? racer_effects_construct(mem, racer) : 0;
  r->child = effects;
  task_add_children(&r->task, task_at(effects));
  return racer;
}
HW_EXPORT(8c030bf4, RET(racer_base_init(ARG0, ARG1, ARG2, (int32_t)ARG3, FARG0, (int32_t)ARG4)))
