/* Pen Pen port, S14 b11: the sign of seven modes (type 0x22) (game/actor.h): its construction, destruction and
 * step. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "game.h"
#include "actor.h"
#include "race_scene.h"
#include "scenery.h"

#define DEG_60 F32_BITS(0x462AAAAAu) /* 10922.67 */
#define DEG_80 F32_BITS(0x46638E38u) /* 14563.55 */

/* was func_8C057656: the course cell under view i's camera (-1 = the current view) (a copy of
 * race_scene_view_cell) */
HW_EXPORT(8c057656, RET(race_scene_view_cell(ARG0, (int32_t)ARG1)))

/* was func_8C0576BE: the sign's base construction: the Actor, its body and swing at rest, 5 motes and the contacts
 * (owned by the position, radius 10, room for 2) */
gaddr sign_base_construct(gaddr obj) {
  Sign *s = (Sign *)hw_ptr(obj);
  actor_construct(obj);
  body_reset(member_addr(obj, Sign, body));
  s->swing.value = F32(0.0f);
  s->swing.velocity = F32(0.0f);
  s->swing.force = F32(0.0f);
  s->swing.damping = F32(1.0f);
  motes_construct(member_addr(obj, Sign, motes), 5);
  contacts_construct(member_addr(obj, Sign, contacts), member_addr(obj, Sign, position), F32(10.0f), 2);
  return obj;
}
HW_EXPORT(8c0576be, RET(sign_base_construct(ARG0)))

/* was func_8C057784: type 0x22's constructor: both vtables, then the base */
gaddr sign_construct(gaddr obj) {
  Sign *s = (Sign *)hw_ptr(obj);
  s->base.vtable = 0x8C23E45Cu;
  s->second_vtable = 0x8C23E47Cu;
  sign_base_construct(obj);
  return obj;
}
HW_EXPORT(8c057784, RET(sign_construct(ARG0)))

/* was func_8C0577A0: the sign's destructor body: its contacts and motes freed */
void sign_destroy(gaddr obj) {
  contacts_free(member_addr(obj, Sign, contacts));
  motes_destroy(member_addr(obj, Sign, motes));
}
HW_EXPORT(8c0577a0, sign_destroy(ARG0))

/* was func_8C0577C0: the sign's virtual destructor: the body, then the memory when asked */
gaddr sign_delete(gaddr obj, int32_t free_memory) {
  sign_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c0577c0, RET(sign_delete(ARG0, (int32_t)ARG1)))

/* pull the swing toward `target` (the short way round) */
static void sign_swing(gaddr obj, f32 target, f32 rate) {
  body1_pull_angle(target, rate, member_addr(obj, Sign, swing));
}

/* was func_8C057AF2: step the sign by its mode:
 * 0: the swing settles to 0 (rate 0.03); with state 0 its contacts are live; every 67 steps the first view on the
 *    actor's cell plays sound 0x29;
 * 2: swings to 80 degrees for 180 steps, back to 0 from 300 to 480, again at 660 (rate 0.01);
 * 3: to 60 degrees for 60 steps, back from 180 to 240, again at 420 (rate 0.1);
 * 4: counts down from 30 again and again;
 * 5: to -40 degrees for 180 steps, back from 300 to 480, at 730 the motes start again; the motes step;
 * 6: the board slides toward (32, 16) (rate 0.025) for 46 steps, then back to (16, 32) (rate 0.09) for 21 steps and
 *    jumps to (32, 8), while the first player is in state 4 or 5 (else it hides); the scene's word 0xCA8 = 2 ends
 *    it; its body steps.
 * Then the swing steps and live contacts go into the race scene's slot 8 */
void sign_update(gaddr obj) {
  Sign *s = (Sign *)hw_ptr(obj);
  int32_t live = 0;
  gaddr target = hw_stack_alloc(sizeof(Vec3));
  switch (s->mode) {
  case 0:
    sign_swing(obj, F32(0.0f), F32_BITS(0x3CF5C28Fu) /* 0.03 */);
    if (s->state == 0) live = 1;
    if (++s->timer > 66) {
      s->timer = 0;
      for (int32_t v = 0; v < (int32_t)s->base.word0c; v++) {
        int32_t cell = race_scene_view_cell(g_panel, v);
        if (feq(f32_abs(f32_from_int(s->base.cell - cell)), F32(0.0f))) {
          race_scene_sound(g_panel, v, 0, 8, 0x29);
          break;
        }
      }
    }
    break;
  case 2:
    s->timer++;
    if (s->timer < 180) {
      sign_swing(obj, DEG_80, F32_BITS(0x3C23D70Au) /* 0.01 */);
    } else if (s->timer >= 300) {
      if (s->timer < 480)
        sign_swing(obj, F32(0.0f), F32_BITS(0x3C23D70Au));
      else if (s->timer >= 660)
        s->timer = 0;
    }
    break;
  case 3:
    s->timer++;
    if (s->timer < 60) {
      sign_swing(obj, DEG_60, F32_BITS(0x3DCCCCCCu) /* 0.1 */);
    } else if (s->timer >= 180) {
      if (s->timer < 240)
        sign_swing(obj, F32(0.0f), F32_BITS(0x3DCCCCCCu));
      else if (s->timer >= 420)
        s->timer = 0;
    }
    break;
  case 4:
    if (--s->timer == 0) s->timer = 30;
    break;
  case 5:
    s->timer++;
    if (s->timer < 180) {
      sign_swing(obj, F32_BITS(0xC5E38E38u) /* -40 degrees */, F32_BITS(0x3C23D70Au));
    } else if (s->timer >= 300) {
      if (s->timer < 480) {
        sign_swing(obj, F32(0.0f), F32_BITS(0x3C23D70Au));
      } else if (s->timer >= 730) {
        s->timer = 0;
        motes_restart(member_addr(obj, Sign, motes));
      }
    }
    motes_update(member_addr(obj, Sign, motes));
    break;
  case 6: {
    gaddr body = member_addr(obj, Sign, body);
    switch (s->state) {
    case 0: {
      vec3_set(F32(32.0f), F32(16.0f), F32(0.0f), target);
      body_pull(F32_BITS(0x3CCCCCCCu) /* 0.025 */, body, target);
      vec3_fill(member_addr(obj, Sign, body.damping), F32_BITS(0x3F733333u) /* 0.95 */);
      if (++s->timer > 45) {
        s->timer = 0;
        s->state++;
      }
      int32_t st = race_player_state(g_panel, 0);
      if (st != 4 && st != 5) s->state = 2;
      if (race_scene_at(g_panel)->word_ca8 == 2) s->state = 3;
      break;
    }
    case 1:
      vec3_set(F32(16.0f), F32(32.0f), F32(0.0f), target);
      body_pull(F32_BITS(0x3DB851EBu) /* 0.09 */, body, target);
      vec3_fill(member_addr(obj, Sign, body.damping), F32_BITS(0x3F733333u) /* 0.95 */);
      if (++s->timer > 20) {
        s->timer = 0;
        vec3_set(F32(32.0f), F32(8.0f), F32(0.0f), body);
        s->state = 0;
      }
      break;
    case 2: {
      int32_t st = race_player_state(g_panel, 0);
      if (st == 4 || st == 5) {
        s->state = 0;
        s->timer = 0;
      }
      if (race_scene_at(g_panel)->word_ca8 == 2) s->state = 3;
      break;
    }
    default:
      break;
    }
    body_step(body);
    break;
  }
  default:
    break;
  }
  hw_stack_free(sizeof(Vec3));
  body1_step(member_addr(obj, Sign, swing));
  if (live) race_scene_slot_insert(g_panel, member_addr(obj, Sign, contacts), 8);
}
HW_EXPORT(8c057af2, sign_update(ARG0))
