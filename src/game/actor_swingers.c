/* Pen Pen port, S14 b11: the part that turns to an angle (type 0x15), the field of 40 sprites (type 0x16) and the
 * shrinking rings (type 0x2C) (game/actor.h). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/ninja.h"
#include "game.h"
#include "actor.h"
#include "race_scene.h"
#include "scenery.h"

/* ---- type 0x15: the part that turns ---- */

/* was func_8C05B91A: the base construction: the Actor and the swing at rest (0, 0, 0, 1) */
gaddr swinger_base_construct(gaddr obj) {
  Swinger *s = (Swinger *)hw_ptr(obj);
  actor_construct(obj);
  s->swing.value = F32(0.0f);
  s->swing.velocity = F32(0.0f);
  s->swing.force = F32(0.0f);
  s->swing.damping = F32(1.0f);
  return obj;
}
HW_EXPORT(8c05b91a, RET(swinger_base_construct(ARG0)))

/* was func_8C05B942: type 0x15's constructor: both vtables, then the base */
gaddr swinger_construct(gaddr obj) {
  Swinger *s = (Swinger *)hw_ptr(obj);
  s->base.vtable = 0x8C23E6BCu;
  s->second_vtable = 0x8C23E6DCu;
  swinger_base_construct(obj);
  return obj;
}
HW_EXPORT(8c05b942, RET(swinger_construct(ARG0)))

/* was func_8C05B9C0: set the part up from its placement: position, angles, kind; the swing still at 0 with damping
 * by kind (1, 1, 0.6, 0.6, 0.4, 0.4; kind 6 starts at 25 degrees, damping 1) */
void swinger_init(gaddr obj, gaddr placement) {
  Swinger *s = (Swinger *)hw_ptr(obj);
  const ActorPlacement *pl = (const ActorPlacement *)hw_ptr(placement);
  s->position = pl->position;
  s->angle.x = pl->angle.x;
  s->angle.y = pl->angle.y;
  s->angle.z = pl->angle.z;
  s->kind = pl->param;
  s->word2c = 0;
  s->swing.value = F32(0.0f);
  s->swing.velocity = F32(0.0f);
  switch (s->kind) {
  case 0:
  case 1:
    s->swing.damping = F32(1.0f);
    s->style = 1;
    break;
  case 2:
  case 3:
    s->swing.damping = F32_BITS(0x3F199999u); /* 0.6 */
    s->style = 2;
    break;
  case 4:
  case 5:
    s->swing.damping = F32_BITS(0x3ECCCCCCu); /* 0.4 */
    s->style = 2;
    break;
  case 6:
    s->swing.value = F32_BITS(0x458E38E3u); /* 25 degrees */
    s->swing.damping = F32(1.0f);
    s->style = 4;
    break;
  default:
    break;
  }
}
HW_EXPORT(8c05b9c0, swinger_init(ARG0, ARG1))

/* was func_8C05BA7E: pull the swing toward its kind's angle the short way round (wrapped), then step it: 15 degrees
 * (rate 0.008), 90 (0.003), 60 (0.01), -80 (0.008), -90 (0.01), 90 (0.01), 0 (0.02). A kind above 6 uses whatever
 * the caller left in fr4 as the target, at rate 16384 (the original never sets them): entry_fr4 */
void swinger_update(gaddr obj, f32 entry_fr4) {
  Swinger *s = (Swinger *)hw_ptr(obj);
  f32 target = entry_fr4;
  f32 rate = F32(16384.0f);
  switch (s->kind) {
  case 0:
    target = F32_BITS(0x452AAAAAu); /* 15 degrees */
    rate = F32_BITS(0x3C03126Eu);   /* 0.008 */
    break;
  case 1:
    target = F32(16384.0f);         /* 90 degrees */
    rate = F32_BITS(0x3B449BA5u);   /* 0.003 */
    break;
  case 2:
    target = F32_BITS(0x462AAAAAu); /* 60 degrees */
    rate = F32_BITS(0x3C23D70Au);   /* 0.01 */
    break;
  case 3:
    target = F32_BITS(0xC6638E38u); /* -80 degrees */
    rate = F32_BITS(0x3C03126Eu);   /* 0.008 */
    break;
  case 4:
    target = F32(-16384.0f);        /* -90 degrees */
    rate = F32_BITS(0x3C23D70Au);
    break;
  case 5:
    target = F32(16384.0f);
    rate = F32_BITS(0x3C23D70Au);
    break;
  case 6:
    target = F32(0.0f);
    rate = F32_BITS(0x3CA3D70Au);   /* 0.02 */
    break;
  default:
    break;
  }
  s->swing.force = fmac(rate, angle_wrap(fsub(target, s->swing.value)), s->swing.force);
  body1_step(member_addr(obj, Swinger, swing));
}
HW_EXPORT(8c05ba7e, swinger_update(ARG0, FARG0))

/* ---- type 0x16: the sprite field ---- */

/* a sprite of the field (static table, 20 bytes) */
typedef struct SparkleSpot {
  int32_t frame;            /* 0x00: its texture animation entry */
  Vec3 pos;                 /* 0x04 */
  f32 scale;                /* 0x10 */
} SparkleSpot;
HW_SIZE(SparkleSpot, 20);
#define g_sparkle_spots   ((const SparkleSpot *)hw_ptr(0x8C089E44u))
#define SPARKLE_TEXANIMS  0x8C0995B8u /* NJS_TEXANIM[]: 20 bytes each (static) */
#define NJD_SPRITE_ALPHA_SCALE 0x30u   /* the 3D sprite's attributes */

/* was func_8C05BC60: type 0x16's constructor: both vtables, then the Actor */
gaddr sparkles_construct(gaddr obj) {
  Sparkles *s = (Sparkles *)hw_ptr(obj);
  s->base.vtable = 0x8C23E6FCu;
  s->second_vtable = 0x8C23E71Cu;
  actor_base_construct(obj);
  return obj;
}
HW_EXPORT(8c05bc60, RET(sparkles_construct(ARG0)))

/* was func_8C05BCB6: set the field up: flags 1, each sprite's course cell (locating the actor at it), the sprite
 * not turned, with the actor's texture list */
void sparkles_init(gaddr obj) {
  Sparkles *s = (Sparkles *)hw_ptr(obj);
  s->base.flags = 1;
  gaddr pos = hw_stack_alloc(sizeof(Vec3));
  for (int32_t i = 0; i < SPARKLES; i++) {
    const SparkleSpot *t = &g_sparkle_spots[i];
    Vec3 *p = vec3_at(pos);
    p->z = t->pos.z;
    p->y = t->pos.y;
    p->x = t->pos.x;
    actor_locate(obj, pos, 0);
    s->cells[i] = s->base.cell;
  }
  hw_stack_free(sizeof(Vec3));
  s->sprite.ang = 0;
  s->sprite.tlist = s->base.texlist;
}
HW_EXPORT(8c05bcb6, sparkles_init(ARG0))

/* was func_8C05BD44: draw the 40 sprites: each at its place, its scale both ways, its animation frame */
void sparkles_draw(gaddr obj) {
  Sparkles *s = (Sparkles *)hw_ptr(obj);
  for (int32_t i = 0; i < SPARKLES; i++) {
    const SparkleSpot *t = &g_sparkle_spots[i];
    s->sprite.tanim = SPARKLE_TEXANIMS + (gaddr)(t->frame * 20);
    s->sprite.p.x = t->pos.x;
    s->sprite.p.y = t->pos.y;
    s->sprite.p.z = t->pos.z;
    s->sprite.sx = t->scale;
    s->sprite.sy = t->scale;
    njDrawSprite3D(member_addr(obj, Sparkles, sprite), 0, NJD_SPRITE_ALPHA_SCALE);
  }
}
HW_EXPORT(8c05bd44, sparkles_draw(ARG0))

/* ---- type 0x2C: the rings ---- */

#define RING_CONSTRUCTOR 0x8C05BF22u /* a Body1's constructor (0, 0, 0, 1) */

/* was func_8C05BF54: the base construction: the Actor, the body at rest, the contacts (owned by the body, radius
 * 40, room for 8) and the 40 rings */
gaddr rings_base_construct(gaddr obj) {
  actor_construct(obj);
  body_reset(member_addr(obj, Rings, body));
  contacts_construct(member_addr(obj, Rings, contacts), member_addr(obj, Rings, body), F32(40.0f), 8);
  array_construct(RINGS, sizeof(Body1), member_addr(obj, Rings, sizes), RING_CONSTRUCTOR);
  return obj;
}
HW_EXPORT(8c05bf54, RET(rings_base_construct(ARG0)))

/* was func_8C05BFC6: type 0x2C's constructor: both vtables, then the base */
gaddr rings_construct(gaddr obj) {
  Rings *r = (Rings *)hw_ptr(obj);
  r->base.vtable = 0x8C23E73Cu;
  r->second_vtable = 0x8C23E75Cu;
  rings_base_construct(obj);
  return obj;
}
HW_EXPORT(8c05bfc6, RET(rings_construct(ARG0)))

/* was func_8C05C1E8: step the body and put its contacts into the race scene's slot 8; each ring shrinks (pulled by
 * -0.02 of its size), steps, starts again at 40 below 2.5, and turns 6 degrees */
void rings_update(gaddr obj) {
  Rings *r = (Rings *)hw_ptr(obj);
  body_step(member_addr(obj, Rings, body));
  race_scene_slot_insert(g_panel, member_addr(obj, Rings, contacts), 8);
  for (int32_t k = 0; k < RINGS; k++) {
    Body1 *s = &r->sizes[k];
    s->force = fadd(s->force, fmul(s->value, F32_BITS(0xBCA3D70Au) /* -0.02 */));
    body1_step(member_addr(obj, Rings, sizes[k]));
    if (fgt(F32(2.5f), s->value)) s->value = F32(40.0f);
    r->turns[k] = fadd(r->turns[k], F32_BITS(0x44888888u) /* 6 degrees */);
  }
}
HW_EXPORT(8c05c1e8, rings_update(ARG0))
