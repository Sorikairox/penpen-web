/* Pen Pen port, S17 b06: the drawing and the destructors of the rising things (game/actor_motes.c): the bubbles
 * (type 7), the mote set (MemberArray) and the spring (type 8). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/ninja.h"
#include "../core/inline.h"
#include "actor.h"
#include "actor_b05.h"
#include "scenery.h"
#include "actor_b06.h"

/* ---- type 7: the bubbles ---- */

/* was func_8C04C208: this unit's copy of empty_delete (game/actor.c): the bubbles' deleting destructor */
HW_EXPORT(8c04c208, RET(empty_delete(ARG0, (int32_t)ARG1)))

/* was func_8C04C22C: this unit's copy of empty_destroy_self (game/actor_lifetimes.c) */
HW_EXPORT(8c04c22c, RET(empty_destroy_self(ARG0)))

#define BUBBLE_TEXLIST    0x8C09703Cu /* the bubbles' NJS_TEXLIST (a static of the binary) */
#define BUBBLE_CONTROL3D  0x14u       /* the njControl3D flags they are drawn with */
#define BUBBLE_DRAW_ATTR  0x32u

/* the draw's locals that go to Ninja (its frame, 0x34 bytes: the sprite at 4, the colour at 0x24) */
typedef struct BubblesDrawFrame {
  uint8_t pad_00[4];
  NjSprite sprite;          /* 0x04 */
  NjArgb color;             /* 0x24 */
} BubblesDrawFrame;
HW_OFFSET(BubblesDrawFrame, sprite, 0x04);
HW_OFFSET(BubblesDrawFrame, color, 0x24);
HW_SIZE(BubblesDrawFrame, 0x34);

/* was func_8C04C46E: draw the shown bubbles as 3D sprites (scale 2) with njControl3D flags 0x14 (the flags pushed
 * and popped around): each fades out as its timer runs (alpha 1 - timer / 200) and grows (8 + timer / 5 pixels
 * wide and high, centred) */
void bubbles_draw(gaddr obj) {
  Bubbles *b = (Bubbles *)hw_ptr(obj);
  control3d_push();
  control3d_set(BUBBLE_CONTROL3D);
  gaddr frame = hw_stack_alloc(sizeof(BubblesDrawFrame));
  BubblesDrawFrame *f = (BubblesDrawFrame *)hw_ptr(frame);
  f->color.r = F32(1.0f);
  f->color.g = F32(1.0f);
  f->color.b = F32(1.0f);
  f->sprite.tlist = BUBBLE_TEXLIST;
  f->sprite.ang = 0;
  f->sprite.sx = F32(2.0f);
  f->sprite.sy = F32(2.0f);
  for (int32_t i = 0; i < BUBBLE_COUNT; i++) {
    if (b->shown[i] == 0) continue;
    f->color.a = fadd(fneg(fmul(f32_from_int(b->timer[i]), F32_BITS(0x3BA3D70Au) /* 0.005 */)), F32(1.0f));
    f->sprite.tanim = member_addr(obj, Bubbles, sprites[i]);
    f->sprite.p.x = b->bodies[i].position.x;
    f->sprite.p.y = b->bodies[i].position.y;
    f->sprite.p.z = b->bodies[i].position.z;
    BubbleSprite *s = &b->sprites[i];
    s->w = (int16_t)(f32_to_int(fmul(f32_from_int(b->timer[i]), F32_BITS(0x3E4CCCCCu) /* 0.2 */)) + 8);
    s->h = s->w;
    s->cx = (int16_t)(s->w / 2);
    s->cy = (int16_t)(s->h / 2);
    njSetConstantMaterial(member_addr(frame, BubblesDrawFrame, color));
    njDrawSprite3D(member_addr(frame, BubblesDrawFrame, sprite), 0, BUBBLE_DRAW_ATTR);
  }
  hw_stack_free(sizeof(BubblesDrawFrame));
  control3d_pop();
}
HW_EXPORT(8c04c46e, bubbles_draw(ARG0))

/* ---- the mote set (MemberArray) ---- */

/* was func_8C04C7A6: the mote set's deleting destructor: the motes freed, then the set when asked */
gaddr member_array_delete(gaddr obj, int32_t free_memory) {
  member_array_free(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c04c7a6, RET(member_array_delete(ARG0, (int32_t)ARG1)))

#define MOTE_DRAW_ATTR 0x30u

/* was func_8C04CA86: draw the rising motes as 3D sprites of the set's texture: each its kind's frame at its
 * position, scale 2; the first mote is stretched by the fade (x 2 + fade, y 2 - fade) */
void motes_draw(gaddr obj) {
  const MemberArray *m = (const MemberArray *)hw_ptr(obj);
  gaddr frame = hw_stack_alloc(sizeof(NjSprite)); /* the original's frame: the sprite at its bottom */
  NjSprite *s = (NjSprite *)hw_ptr(frame);
  s->tlist = m->texlist;
  s->tanim = m->texanim;
  s->ang = 0;
  gaddr mote = m->array;
  for (int32_t i = 0; i < m->count; i++, mote += sizeof(Mote)) {
    const Mote *t = (const Mote *)hw_ptr(mote);
    if (t->state != 1) continue;
    s->p.x = t->body.position.x;
    s->p.y = t->body.position.y;
    s->p.z = t->body.position.z;
    f32 two = F32(2.0f);
    if (i == 0) {
      s->sx = fadd(m->fade.value, two);
      two = fsub(two, m->fade.value);
    } else {
      s->sx = two;
    }
    s->sy = two;
    njDrawSprite3D(frame, t->kind, MOTE_DRAW_ATTR);
  }
  hw_stack_free(sizeof(NjSprite));
}
HW_EXPORT(8c04ca86, motes_draw(ARG0))

/* ---- type 8: the spring ---- */

/* was func_8C04CB44: the spring's destructor body: its motes destroyed */
void spring_destroy(gaddr obj) { motes_destroy(member_addr(obj, Spring, motes)); }
HW_EXPORT(8c04cb44, spring_destroy(ARG0))

/* was func_8C04CB90: the deleting destructor */
gaddr spring_delete(gaddr obj, int32_t free_memory) {
  spring_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c04cb90, RET(spring_delete(ARG0, (int32_t)ARG1)))

/* was func_8C04CBB4: the destructor body, returning the object */
gaddr spring_destroy_self(gaddr obj) {
  spring_destroy(obj);
  return obj;
}
HW_EXPORT(8c04cbb4, RET(spring_destroy_self(ARG0)))

/* was func_8C04CC5E: draw the spring's motes while they rise (state not 0) */
void spring_draw(gaddr obj) {
  if (((const Spring *)hw_ptr(obj))->state != 0) motes_draw(member_addr(obj, Spring, motes));
}
HW_EXPORT(8c04cc5e, spring_draw(ARG0))
