/* Pen Pen port, S17 b07: the construction and destruction of the scenery actors of placement types 0x0A, 0x0F,
 * 0x10, 0x26 and 0x27 (game/actor_b07.h), and the material setter of type 0x26's unit. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/inline.h"
#include "../core/view.h"
#include "../core/file.h"
#include "game.h"
#include "course.h"
#include "race_scene.h"
#include "actor.h"
#include "actor_b05.h"
#include "scenery.h"
#include "actor_b07.h"

#define TYPE0A_VTABLE        0x8C23E35Cu
#define TYPE0A_SECOND_VTABLE 0x8C23E37Cu
#define TYPE0A_MODEL         11          /* in the actor's model list */
#define TYPE0A_ROW_BIT       12          /* the course row bit the draw tests */
#define TYPE0A_VIEW_BIT      15          /* the race scene's view bit the draw tests */
#define TYPE0A_GROUP         5           /* vertices moved together */
#define TYPE0F_MODEL         9
#define TYPE0F_VTABLE        0x8C23E29Cu
#define TYPE0F_SECOND_VTABLE 0x8C23E2BCu
#define TYPE0F_RADIUS        F32_BITS(0x407FFFFFu) /* 3.9999998 */
#define TYPE10_VTABLE        0x8C23E2DCu
#define TYPE10_SECOND_VTABLE 0x8C23E2FCu
#define TYPE10_RADIUS        F32(2.5f)
#define TYPE26_VTABLE        0x8C23E4DCu
#define TYPE26_SECOND_VTABLE 0x8C23E4FCu
#define TYPE26_BLOCK         0x20u
#define TYPE26_BODY_CONSTRUCTOR 0x8C058B44u /* body_reset_set3's copy (game/scenery_b07.c) */
#define TYPE27_VTABLE        0x8C23E59Cu
#define TYPE27_SECOND_VTABLE 0x8C23E5BCu
#define TYPE27_THIRD_VTABLE  0x8C23E5DCu
#define TYPE27_BODY_CONSTRUCTOR 0x8C059F88u /* body_reset_set3's copy (game/scenery_b07.c) */
#define TYPE27_QUAT_CONSTRUCTOR 0x8C059FC8u /* a Quat (0, 0, 0, 1) (still translated) */

/* statics of the binary the unit reads (not folded into the code) */
#define g_type26_radius      GUEST_VAR(f32, 0x8C0885F8u) /* 5: type 0x26's contacts' radius */
#define MATERIAL_DEFAULT_89B8 0x8C099418u                /* the unit's default NJS_ARGB */
#define g_type26_particles   GUEST_VAR(int32_t, 0x8C0885FCu) /* 8: words of blocks[0] the reset clears */
#define g_type26_reset_word  GUEST_VAR(uint32_t, 0x8C096D10u)
#define TYPE26_TABLE_A       0x8C099428u
#define TYPE26_TABLE_B       0x8C09943Cu

/* a Body1 at rest: (0, 0, 0, 1) (the inline Quat constructor) */
static void body1_rest(Body1 *v) {
  v->value = F32(0.0f);
  v->velocity = F32(0.0f);
  v->force = F32(0.0f);
  v->damping = F32(1.0f);
}

/* a Body at rest at the origin, through the Vec3 setter */
static void body_rest_set(gaddr body) {
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, position));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, velocity));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, force));
  vec3_set(F32(1.0f), F32(1.0f), F32(1.0f), member_addr(body, Body, damping));
}

/* ---- type 0x0A ---- */

/* was func_8C055650: the base construction: the Actor and both Body1s at rest */
gaddr actor_type0a_base_construct(gaddr obj) {
  ActorType0A *a = (ActorType0A *)hw_ptr(obj);
  actor_construct(obj);
  body1_rest(&a->a);
  body1_rest(&a->b);
  return obj;
}
HW_EXPORT(8c055650, RET(actor_type0a_base_construct(ARG0)))

/* was func_8C05568A: type 0x0A's constructor: both vtables, then the base */
gaddr actor_type0a_construct(gaddr obj) {
  ActorType0A *a = (ActorType0A *)hw_ptr(obj);
  a->base.vtable = TYPE0A_VTABLE;
  a->second_vtable = TYPE0A_SECOND_VTABLE;
  actor_type0a_base_construct(obj);
  return obj;
}
HW_EXPORT(8c05568a, RET(actor_type0a_construct(ARG0)))

/* was func_8C05570E: set it up (once: not when the group's re-init has set flags bit 1): model 11 of the list, the
 * address of its vertex list's first vertex's y and that y, the vertex count / 5 groups; flags bit 0; ripple a at 1 (just
 * under), ripple b at 0.6 */
void actor_type0a_init(gaddr obj) {
  ActorType0A *a = (ActorType0A *)hw_ptr(obj);
  a->word28 = 0;
  if ((a->base.flags & 2) == 0) {
    gaddr object = actor_model(&a->base, TYPE0A_MODEL);
    a->model = object;
    gaddr vlist = ((const NjCnkModel *)hw_ptr(((const NjObject *)hw_ptr(object))->model))->vlist;
    int32_t sizes = ((const CnkVertexHeader *)hw_ptr(vlist))->sizes;
    /* the count is the upper half, as a signed 16-bit value (rounded toward zero) */
    a->groups = (int16_t)((uint32_t)(sizes < 0 ? sizes + 0xFFFF : sizes) >> 16);
    a->groups = a->groups / TYPE0A_GROUP;
    CnkVertexN *first = (CnkVertexN *)hw_ptr(vlist + sizeof(CnkVertexHeader));
    a->heights = hw_addr(&first->pos.y);
    a->rest_y = first->pos.y;
  }
  a->base.flags |= 1;
  a->a.value = F32(0.0f);
  a->a.velocity = F32(0.0f);
  a->a.force = F32(0.0f);
  a->a.damping = F32(1.0f);
  a->a.value = F32_BITS(0x3F7FFFFFu); /* 0.99999994 */
  a->a.damping = F32(1.0f);
  a->b.value = F32(0.0f);
  a->b.velocity = F32(0.0f);
  a->b.force = F32(0.0f);
  a->b.damping = F32(1.0f);
  a->b.value = F32_BITS(0x3F199999u); /* 0.6 */
  a->b.damping = F32(1.0f);
}
HW_EXPORT(8c05570e, actor_type0a_init(ARG0))

/* was func_8C0557C4: step it: both ripples are pulled back toward 0 (rates 0.007 and 0.009) and stepped; then in
 * every group of five vertices the y of each is set from the rest y: + a, - a, + b, + a, - b */
void actor_type0a_update(gaddr obj) {
  ActorType0A *a = (ActorType0A *)hw_ptr(obj);
  a->a.force = fmac(F32_BITS(0x3BE56041u) /* 0.007 */, fsub(F32(0.0f), a->a.value), a->a.force);
  body1_step(member_addr(obj, ActorType0A, a));
  a->b.force = fmac(F32_BITS(0x3C1374BCu) /* 0.009 */, fsub(F32(0.0f), a->b.value), a->b.force);
  body1_step(member_addr(obj, ActorType0A, b));
  gaddr v = a->heights;
  f32 plus_a = fadd(a->rest_y, a->a.value);
  f32 minus_a = fsub(a->rest_y, a->a.value);
  f32 minus_b = fsub(a->rest_y, a->b.value);
  f32 plus_b = fadd(a->rest_y, a->b.value);
  for (int32_t i = 0; i < a->groups; i++) {
    /* the group's first vertex (heights points at a vertex's y) */
    CnkVertexN *g = (CnkVertexN *)((uint8_t *)hw_ptr(v) - offsetof(CnkVertexN, pos.y));
    g[0].pos.y = plus_a;
    g[1].pos.y = minus_a;
    g[2].pos.y = plus_b;
    g[3].pos.y = plus_a;
    g[4].pos.y = minus_b;
    v += TYPE0A_GROUP * sizeof(CnkVertexN);
  }
}
HW_EXPORT(8c0557c4, actor_type0a_update(ARG0))

/* was func_8C055864: draw its model when the current view's course row has bit 12 or the race scene's view has
 * bit 15 */
void actor_type0a_draw(gaddr obj) {
  gaddr scene = g_panel;
  if (!course_row_bit(race_scene_at(scene)->course, view_current_index(), TYPE0A_ROW_BIT) &&
      !race_scene_view_bit(panel_get(), TYPE0A_VIEW_BIT))
    return;
  njPushMatrix(0);
  nj_ModelL1(((ActorType0A *)hw_ptr(obj))->model);
  njPopMatrix(1);
}
HW_EXPORT(8c055864, actor_type0a_draw(ARG0))

/* ---- type 0x0F ---- */

/* was func_8C0545B8: draw it when shown: model 9 of the list, by its matrix */
void actor_type0f_draw(gaddr obj) {
  ActorType0F *a = (ActorType0F *)hw_ptr(obj);
  if (a->shown == 0) return;
  njPushMatrix(0);
  njMultiMatrix(0, member_addr(obj, ActorType0F, matrix));
  nj_ModelL1(actor_model(&a->base, TYPE0F_MODEL));
  njPopMatrix(1);
}
HW_EXPORT(8c0545b8, actor_type0f_draw(ARG0))


/* was func_8C053D62: the base construction: the Actor, its Body at rest, the three Body1s at rest, the matrix the
 * identity, the contacts owned by the Body (radius 4, room for 3) */
gaddr actor_type0f_base_construct(gaddr obj) {
  ActorType0F *a = (ActorType0F *)hw_ptr(obj);
  actor_construct(obj);
  body_rest_set(member_addr(obj, ActorType0F, body));
  for (int32_t i = 0; i < 3; i++) body1_rest(&a->values[i]);
  matrix_identity(member_addr(obj, ActorType0F, matrix));
  contacts_construct(member_addr(obj, ActorType0F, contacts), member_addr(obj, ActorType0F, body), TYPE0F_RADIUS, 3);
  return obj;
}
HW_EXPORT(8c053d62, RET(actor_type0f_base_construct(ARG0)))

/* was func_8C053E26: type 0x0F's constructor: both vtables, then the base */
gaddr actor_type0f_construct(gaddr obj) {
  ActorType0F *a = (ActorType0F *)hw_ptr(obj);
  a->base.vtable = TYPE0F_VTABLE;
  a->second_vtable = TYPE0F_SECOND_VTABLE;
  actor_type0f_base_construct(obj);
  return obj;
}
HW_EXPORT(8c053e26, RET(actor_type0f_construct(ARG0)))

/* was func_8C053E42: the destructor body: the contacts freed */
void actor_type0f_destroy(gaddr obj) { contacts_free(member_addr(obj, ActorType0F, contacts)); }
HW_EXPORT(8c053e42, actor_type0f_destroy(ARG0))

/* was func_8C053E8C */
gaddr actor_type0f_delete(gaddr obj, int32_t free_memory) {
  actor_type0f_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c053e8c, RET(actor_type0f_delete(ARG0, (int32_t)ARG1)))

/* was func_8C053EB0 */
gaddr actor_type0f_destroy_self(gaddr obj) {
  actor_type0f_destroy(obj);
  return obj;
}
HW_EXPORT(8c053eb0, RET(actor_type0f_destroy_self(ARG0)))

/* ---- type 0x10 ---- */

/* was func_8C05485C: the base construction: the Actor, its three Body1s at rest, the matrix the identity, the
 * contacts owned by the point (radius 2.5, room for 3) */
gaddr actor_type10_base_construct(gaddr obj) {
  ActorType10 *a = (ActorType10 *)hw_ptr(obj);
  actor_construct(obj);
  body1_rest(&a->value30);
  body1_rest(&a->value58);
  body1_rest(&a->value6c);
  matrix_identity(member_addr(obj, ActorType10, matrix));
  contacts_construct(member_addr(obj, ActorType10, contacts), member_addr(obj, ActorType10, point), TYPE10_RADIUS, 3);
  return obj;
}
HW_EXPORT(8c05485c, RET(actor_type10_base_construct(ARG0)))

/* was func_8C0548E0: type 0x10's constructor: both vtables, then the base */
gaddr actor_type10_construct(gaddr obj) {
  ActorType10 *a = (ActorType10 *)hw_ptr(obj);
  a->base.vtable = TYPE10_VTABLE;
  a->second_vtable = TYPE10_SECOND_VTABLE;
  actor_type10_base_construct(obj);
  return obj;
}
HW_EXPORT(8c0548e0, RET(actor_type10_construct(ARG0)))

/* was func_8C0548FC: the destructor body: the contacts freed */
void actor_type10_destroy(gaddr obj) { contacts_free(member_addr(obj, ActorType10, contacts)); }
HW_EXPORT(8c0548fc, actor_type10_destroy(ARG0))

/* was func_8C054912 */
gaddr actor_type10_delete(gaddr obj, int32_t free_memory) {
  actor_type10_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c054912, RET(actor_type10_delete(ARG0, (int32_t)ARG1)))

/* was func_8C054964 */
gaddr actor_type10_destroy_self(gaddr obj) {
  actor_type10_destroy(obj);
  return obj;
}
HW_EXPORT(8c054964, RET(actor_type10_destroy_self(ARG0)))

/* ---- type 0x26 ---- */

/* was func_8C0589B8: njSetConstantMaterial of {a, r, g, b} = the four floats (r, g, b, a): a local NJS_ARGB first
 * copied from the unit's default, then filled (its code runs on through func_8C0589C6, a fragment) */
void material_set_89b8(f32 r, f32 g, f32 b, f32 a) {
  gaddr argb = hw_stack_alloc(20); /* the original's local, 16 bytes below its saved PR */
  memmove(hw_ptr(argb), hw_ptr(MATERIAL_DEFAULT_89B8), sizeof(NjArgb));
  NjArgb *c = (NjArgb *)hw_ptr(argb);
  c->a = a;
  c->r = r;
  c->g = g;
  c->b = b;
  njSetConstantMaterial(argb);
  hw_stack_free(20);
}
HW_EXPORT(8c0589b8, material_set_89b8(FARG0, FARG1, FARG2, FARG3))

/* was func_8C058B84: the base construction: the Actor, its Body at rest, the contacts owned by the point (radius
 * g_type26_radius, room for 2), three 0x20-byte blocks and eight Bodies at rest from operator new[] (0 when out of
 * memory) */
gaddr actor_type26_base_construct(gaddr obj) {
  ActorType26 *a = (ActorType26 *)hw_ptr(obj);
  actor_construct(obj);
  body_rest_set(member_addr(obj, ActorType26, body));
  contacts_construct(member_addr(obj, ActorType26, contacts), member_addr(obj, ActorType26, point), g_type26_radius,
                     2);
  for (int32_t i = 0; i < 3; i++) a->blocks[i] = syMalloc(TYPE26_BLOCK);
  gaddr block = syMalloc(TYPE26_BODIES * sizeof(Body));
  gaddr bodies = 0;
  if (block != 0) bodies = array_construct_new(TYPE26_BODIES, sizeof(Body), block, TYPE26_BODY_CONSTRUCTOR);
  a->bodies = bodies;
  return obj;
}
HW_EXPORT(8c058b84, RET(actor_type26_base_construct(ARG0)))

/* was func_8C058C2A: type 0x26's constructor: both vtables, then the base */
gaddr actor_type26_construct(gaddr obj) {
  ActorType26 *a = (ActorType26 *)hw_ptr(obj);
  a->base.vtable = TYPE26_VTABLE;
  a->second_vtable = TYPE26_SECOND_VTABLE;
  actor_type26_base_construct(obj);
  return obj;
}
HW_EXPORT(8c058c2a, RET(actor_type26_construct(ARG0)))

/* was func_8C05905C: the blocks freed (each that is not 0), the Body array's block last */
static void actor_type26_free_blocks(gaddr obj) {
  for (int32_t i = 0; i < 3; i++) {
    gaddr p = ((ActorType26 *)hw_ptr(obj))->blocks[i];
    if (p != 0) syFree(p);
  }
  gaddr bodies = ((ActorType26 *)hw_ptr(obj))->bodies;
  if (bodies != 0) syFree(bodies);
}
HW_EXPORT(8c05905c, actor_type26_free_blocks(ARG0))

/* was func_8C05900E: reset it: the counter 0; word100 and the table by word30 (0: word88 and table A; else the
 * global word and table B); the counter from f3c; the first block's g_type26_particles words cleared */
void actor_type26_reset(gaddr obj) {
  ActorType26 *a = (ActorType26 *)hw_ptr(obj);
  gaddr table;
  a->counter = 0;
  if (a->word30 == 0) {
    a->word100 = a->word88;
    table = TYPE26_TABLE_A;
  } else {
    a->word100 = g_type26_reset_word;
    table = TYPE26_TABLE_B;
  }
  a->table = table;
  a->counter = f32_to_int(a->f3c);
  uint32_t *w = (uint32_t *)hw_ptr(a->blocks[0]);
  for (int32_t n = g_type26_particles; n > 0; n--) *w++ = 0;
}
HW_EXPORT(8c05900e, actor_type26_reset(ARG0))

/* was func_8C058C80: the destructor body: the blocks freed, then the contacts */
void actor_type26_destroy(gaddr obj) {
  actor_type26_free_blocks(obj);
  contacts_free(member_addr(obj, ActorType26, contacts));
}
HW_EXPORT(8c058c80, actor_type26_destroy(ARG0))

/* was func_8C058C9A */
gaddr actor_type26_delete(gaddr obj, int32_t free_memory) {
  actor_type26_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c058c9a, RET(actor_type26_delete(ARG0, (int32_t)ARG1)))

/* was func_8C058CBE */
gaddr actor_type26_destroy_self(gaddr obj) {
  actor_type26_destroy(obj);
  return obj;
}
HW_EXPORT(8c058cbe, RET(actor_type26_destroy_self(ARG0)))

/* ---- type 0x27: a Flyer with more parts ---- */

/* was func_8C05A018: the base construction: the Flyer's, 21 Bodies and 21 Quats at rest, both matrices the identity,
 * the contacts owned by the Flyer's Body (radius 10, room for 2) */
gaddr actor_type27_base_construct(gaddr obj) {
  flyer_base_construct(obj);
  array_construct(TYPE27_PARTS, sizeof(Body), member_addr(obj, ActorType27, parts), TYPE27_BODY_CONSTRUCTOR);
  array_construct(TYPE27_PARTS, sizeof(Quat), member_addr(obj, ActorType27, turns), TYPE27_QUAT_CONSTRUCTOR);
  matrix_identity(member_addr(obj, ActorType27, matrix_a));
  matrix_identity(member_addr(obj, ActorType27, matrix_b));
  contacts_construct(member_addr(obj, ActorType27, contacts), member_addr(obj, Flyer, body), F32(10.0f), 2);
  return obj;
}
HW_EXPORT(8c05a018, RET(actor_type27_base_construct(ARG0)))

/* was func_8C05A0A8: type 0x27's constructor: its three vtables, then the base */
gaddr actor_type27_construct(gaddr obj) {
  ActorType27 *a = (ActorType27 *)hw_ptr(obj);
  a->flyer.base.vtable = TYPE27_VTABLE;
  a->flyer.second_vtable = TYPE27_SECOND_VTABLE;
  a->third_vtable = TYPE27_THIRD_VTABLE;
  actor_type27_base_construct(obj);
  return obj;
}
HW_EXPORT(8c05a0a8, RET(actor_type27_construct(ARG0)))

/* was func_8C05A0CC: the destructor body: the contacts freed, then the Flyer's (empty) destructor body */
void actor_type27_destroy(gaddr obj) {
  contacts_free(member_addr(obj, ActorType27, contacts));
  method_empty();
}
HW_EXPORT(8c05a0cc, actor_type27_destroy(ARG0))

/* was func_8C05A0E0 */
gaddr actor_type27_delete(gaddr obj, int32_t free_memory) {
  actor_type27_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c05a0e0, RET(actor_type27_delete(ARG0, (int32_t)ARG1)))

/* was func_8C05A104 */
gaddr actor_type27_destroy_self(gaddr obj) {
  actor_type27_destroy(obj);
  return obj;
}
HW_EXPORT(8c05a104, RET(actor_type27_destroy_self(ARG0)))
