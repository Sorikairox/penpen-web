/* Pen Pen port, S14 b11: the geyser (type 0x18) (game/actor.h): it waits, swells, settles, and may send motes up. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "game.h"
#include "actor.h"
#include "race_scene.h"
#include "scenery.h"

/* the course cells a still geyser (kind 2) stands on, by its cell index - 30 (static data) */
#define g_geyser_cells ((const int32_t *)hw_ptr(0x8C0884CCu))
#define GEYSER_MOTES_TEXANIM 0x8C099360u /* the motes' texture animation (static) */

/* was func_8C056086: the geyser's base construction: the Actor, its scale at rest and its 10 motes */
gaddr geyser_base_construct(gaddr obj) {
  actor_construct(obj);
  body_reset(member_addr(obj, Geyser, scale));
  motes_construct(member_addr(obj, Geyser, motes), 10);
  return obj;
}
HW_EXPORT(8c056086, RET(geyser_base_construct(ARG0)))

/* was func_8C0560E2: type 0x18's constructor: both vtables, then the base */
gaddr geyser_construct(gaddr obj) {
  Geyser *g = (Geyser *)hw_ptr(obj);
  g->base.vtable = 0x8C23E3DCu;
  g->second_vtable = 0x8C23E3FCu;
  geyser_base_construct(obj);
  return obj;
}
HW_EXPORT(8c0560e2, RET(geyser_construct(ARG0)))

/* was func_8C05616C: set the geyser up from its placement's param (cell index, kind, first delay, bit 12; bit 13
 * keeps it still) and position; the motes start 87.5 above it (top 65) with the actor's textures. Kinds 0 and 1
 * swell from (1, 1, 1) with damping 0.92 (kind 1 with motes); kind 2 stands on its cell from the table, no damping */
void geyser_init(gaddr obj, gaddr placement) {
  Geyser *g = (Geyser *)hw_ptr(obj);
  const ActorPlacement *pl = (const ActorPlacement *)hw_ptr(placement);
  g->state = 0;
  g->param = pl->param;
  g->cell_index = (int32_t)(g->param & 0x1F) + 17;
  g->kind = (int32_t)(g->param & 0xE0) >> 5;
  g->delay = ((int32_t)(g->param & 0xF00) >> 8) * 40 + 20;
  g->flag12 = (int32_t)(g->param & 0x1000) >> 12;
  body_reset(member_addr(obj, Geyser, scale));
  g->position = pl->position;
  g->angle_y = pl->angle.y;
  if (g->param & 0x2000) g->state = 4;
  gaddr origin = hw_stack_alloc(sizeof(Vec3));
  Vec3 *o = vec3_at(origin);
  *o = g->position;
  o->y = fadd(o->y, F32(87.5f));
  motes_setup(member_addr(obj, Geyser, motes), origin, 1, F32(65.0f));
  hw_stack_free(sizeof(Vec3));
  g->motes.texlist = g->base.texlist;
  g->motes.texanim = GEYSER_MOTES_TEXANIM;
  g->motes_on = 0;
  f32 damping;
  switch (g->kind) {
  case 1:
    g->motes_on = 1;
    /* fall through */
  case 0:
    g->scale.position.x = F32(1.0f);
    g->scale.position.y = F32(1.0f);
    g->scale.position.z = F32(1.0f);
    damping = F32_BITS(0x3F6B851Eu); /* 0.92 */
    break;
  case 2:
    g->base.cell = g_geyser_cells[g->cell_index - 30];
    g->scale.position.x = F32(1.0f);
    g->scale.position.y = F32(1.0f);
    g->scale.position.z = F32(1.0f);
    damping = F32(0.0f);
    break;
  default:
    return;
  }
  g->scale.damping.z = damping;
  g->scale.damping.y = damping;
  g->scale.damping.x = damping;
}
HW_EXPORT(8c05616c, geyser_init(ARG0, ARG1))

/* was func_8C056304: step the geyser (kinds 0 and 1): it waits its delay, swells toward (1.2, 0.8, 1.2) (rate 0.05)
 * for 20 steps, settles back toward 1 (rate 0.025) for 100 steps (kind 1: at 90 left the motes start again and every
 * view within a cell of it plays sound 0x2E), pauses 10 steps and starts again; the scale steps. Then the motes step
 * while on */
void geyser_update(gaddr obj) {
  Geyser *g = (Geyser *)hw_ptr(obj);
  if (g->kind <= 1) {
    gaddr target = hw_stack_alloc(sizeof(Vec3));
    gaddr scale = member_addr(obj, Geyser, scale);
    switch (g->state) {
    case 0:
      if (--g->delay < 0) {
        g->state++;
        g->delay = 20;
      }
      break;
    case 1:
      vec3_set(F32_BITS(0x3F999999u) /* 1.2 */, F32_BITS(0x3F4CCCCCu) /* 0.8 */, F32_BITS(0x3F999999u), target);
      body_pull(F32_BITS(0x3D4CCCCCu) /* 0.05 */, scale, target);
      if (--g->delay < 0) {
        g->state++;
        g->delay = 100;
      }
      break;
    case 2:
      vec3_set(F32(1.0f), F32(1.0f), F32(1.0f), target);
      body_pull(F32_BITS(0x3CCCCCCCu) /* 0.025 */, scale, target);
      g->delay--;
      if (g->kind != 0 && g->delay == 90) {
        motes_restart(member_addr(obj, Geyser, motes));
        for (int32_t v = 0; v < (int32_t)g->base.word0c; v++) {
          int32_t cell = race_scene_view_cell(g_panel, v);
          if (!fgt(f32_abs(f32_from_int(g->base.cell - cell)), F32(1.0f))) race_scene_sound(g_panel, v, 0, 8, 0x2E);
        }
      }
      if (g->delay < 0) {
        g->state++;
        g->delay = 10;
      }
      break;
    case 3:
      g->state = 0;
      break;
    default:
      break;
    }
    hw_stack_free(sizeof(Vec3));
    body_step(scale);
  }
  if (g->motes_on) motes_update(member_addr(obj, Geyser, motes));
}
HW_EXPORT(8c056304, geyser_update(ARG0))
