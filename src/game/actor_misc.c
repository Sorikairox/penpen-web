/* Pen Pen port, S14 b10: methods of several scenery actor classes (game/actor.h): the pair of turning parts
 * (type 3), the scrolling strip's draw (type 4), the chain (type 0x1D), the three-part gate (type 0x1F), the turner
 * (type 0x23), the animated model's draw (type 0x1C), type 0x2B's construction; and three small race-scene
 * accessors of this unit. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/view.h"
#include "../core/ninja.h"
#include "game.h"
#include "actor.h"
#include "course.h"
#include "race_scene.h"
#include "scenery.h"

/* nj_ModelL4 (Ninja, S2 name): draw a chunk model in another light mode */
static inline void nj_ModelL4(gaddr model) { CALL1(0x8C0AB8C8u, model); }

/* ---- small accessors ---- */

/* was func_8C04AD62: v = (x, y, z) (a copy of the setter that returns nothing) */
HW_EXPORT(8c04ad62, vec3_set(FARG0, FARG1, FARG2, ARG0))

/* was func_8C04D7F4: the course cell a position is on, searched from `hint` in the scene's course */
int32_t race_scene_find_cell(gaddr scene, gaddr pos, int32_t hint) {
  return course_find_cell(race_scene_at(scene)->course, pos, hint);
}
HW_EXPORT(8c04d7f4, RET(race_scene_find_cell(ARG0, ARG1, (int32_t)ARG2)))

/* was func_8C05045A: the player view i's camera follows (the writer had read the other half of the literal-pool
 * word, 0xAC = view_cell; the difftest caught it on cup2) */
int32_t race_scene_view_focus(gaddr scene, int32_t i) { return race_scene_at(scene)->focus[i]; }
HW_EXPORT(8c05045a, RET(race_scene_view_focus(ARG0, (int32_t)ARG1)))

/* ---- type 3: two parts that turn toward their targets while a player is near ---- */

/* the update's locals that go to guest code (its frame: the distance at 4, the player's position at 0x10) */
typedef struct PairFrame {
  uint8_t pad_00[4];
  Vec3 diff;                /* 0x04 */
  Vec3 player;              /* 0x10 */
} PairFrame;
HW_OFFSET(PairFrame, player, 0x10);
HW_SIZE(PairFrame, 0x1C);

/* was func_8C046374: when a player is nearer than 60 to the centre, both parts turn toward their targets (rate
 * 0.007) and, the first time, every view whose camera is within a cell of the actor plays sound 0x24; with no
 * player near, both turn back to the rest angle (rate 0.002). Then both Body1s step */
void actor_pair_update(gaddr obj) {
  ActorPair *a = (ActorPair *)hw_ptr(obj);
  int32_t players = race_setup_at(g_race_setup)->player_count;
  int32_t near = 0;
  gaddr frame = hw_stack_alloc(sizeof(PairFrame));
  PairFrame *f = (PairFrame *)hw_ptr(frame);
  for (int32_t i = 0; i < players; i++) {
    f->player = *vec3_at(race_player_body(g_panel, i));
    vec3_diff(member_addr(frame, PairFrame, diff), member_addr(frame, PairFrame, player),
              member_addr(obj, ActorPair, center));
    if (fgt(F32(60.0f), njScalor(member_addr(frame, PairFrame, diff)))) {
      const f32 rate = F32_BITS(0x3BE56041u); /* 0.007 */
      body1_pull_angle(a->target_a, rate, member_addr(obj, ActorPair, turn_a));
      body1_pull_angle(a->target_b, rate, member_addr(obj, ActorPair, angle_b));
      near = 1;
      break;
    }
  }
  hw_stack_free(sizeof(PairFrame));
  if (!near) {
    const f32 rate = F32_BITS(0x3B03126Eu); /* 0.002 */
    body1_pull_angle(a->rest_angle, rate, member_addr(obj, ActorPair, turn_a));
    body1_pull_angle(a->rest_angle, rate, member_addr(obj, ActorPair, angle_b));
    a->sounded = 0;
  } else if (!a->sounded) {
    for (int32_t v = 0; v < (int32_t)a->base.word0c; v++) {
      int32_t cell = race_scene_view_cell(g_panel, v);
      if (!fgt(fabs_(f32_from_int(a->base.cell - cell)), F32(1.0f))) race_scene_sound(g_panel, v, 0, 8, 0x24);
    }
    a->sounded = 1;
  }
  body1_step(member_addr(obj, ActorPair, turn_a));
  body1_step(member_addr(obj, ActorPair, angle_b));
}
HW_EXPORT(8c046374, actor_pair_update(ARG0))

/* ---- type 4: the scrolling strip ---- */

/* was func_8C0468C4: draw the strip when the current view's course row has its bit or the race scene's view has
 * its bit */
void strip_draw(gaddr obj) {
  const Strip *s = (const Strip *)hw_ptr(obj);
  gaddr scene = g_panel;
  int32_t row_bit = (int32_t)s->sound_a;
  if (!course_row_bit(race_scene_at(scene)->course, view_current_index(), row_bit) &&
      !race_scene_view_bit(panel_get(), (int32_t)s->sound_b))
    return;
  njPushMatrix(0);
  nj_ModelL4(s->model);
  njPopMatrix(1);
}
HW_EXPORT(8c0468c4, strip_draw(ARG0))

/* ---- type 0x1D: the chain ---- */

#define CHAIN_LINK_CONSTRUCTOR 0x8C0478C0u /* a Body1's constructor (0, 0, 0, 1) */

/* was func_8C0478D4: the chain's base construction: the Actor, its Body at rest at the origin, its contacts owned
 * by the Body (radius 20, room for 8), the swing at rest and the 20 links */
gaddr chain_base_construct(gaddr obj) {
  Chain *c = (Chain *)hw_ptr(obj);
  actor_construct(obj);
  gaddr body = member_addr(obj, Chain, body);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, position));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, velocity));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, force));
  vec3_set(F32(1.0f), F32(1.0f), F32(1.0f), member_addr(body, Body, damping));
  contacts_construct(member_addr(obj, Chain, contacts), body, F32(20.0f), 8);
  c->swing.value = F32(0.0f);
  c->swing.velocity = F32(0.0f);
  c->swing.force = F32(0.0f);
  c->swing.damping = F32(1.0f);
  array_construct(CHAIN_LINKS, sizeof(Body1), member_addr(obj, Chain, links), CHAIN_LINK_CONSTRUCTOR);
  return obj;
}
HW_EXPORT(8c0478d4, RET(chain_base_construct(ARG0)))

/* was func_8C04795C: type 0x1D's constructor: both vtables, then the base */
gaddr chain_construct(gaddr obj) {
  Chain *c = (Chain *)hw_ptr(obj);
  c->base.vtable = 0x8C23DD4Cu;
  c->second_vtable = 0x8C23DD6Cu;
  chain_base_construct(obj);
  return obj;
}
HW_EXPORT(8c04795c, RET(chain_construct(ARG0)))

/* was func_8C047B88: step the chain: its Body, its contacts into the race scene's slot 8, the swing pulled toward
 * 1.25 (rate 0.03), the angle 5 degrees back; each link is pulled toward 0 (rate 0.03) and starts again at 20 once
 * below 2.5 */
void chain_update(gaddr obj) {
  Chain *c = (Chain *)hw_ptr(obj);
  body_step(member_addr(obj, Chain, body));
  race_scene_slot_insert(g_panel, member_addr(obj, Chain, contacts), 8);
  gaddr swing = member_addr(obj, Chain, swing);
  body1_pull(F32(1.25f), F32_BITS(0x3CF5C28Fu) /* 0.03 */, swing);
  body1_step(swing);
  c->word_b4 = fsub(c->word_b4, F32_BITS(0x44638E38u)); /* 910.2222: 5 degrees */
  for (int32_t k = 0; k < CHAIN_LINKS; k++) {
    Body1 *l = &c->links[k];
    l->force = fadd(l->force, fmul(l->value, F32_BITS(0xBCF5C28Fu))); /* -0.03 */
    body1_step(member_addr(obj, Chain, links[k]));
    if (fgt(F32(2.5f), l->value)) l->value = F32(20.0f);
  }
}
HW_EXPORT(8c047b88, chain_update(ARG0))

/* ---- type 0x1F: the three-part gate at the start or the finish ---- */

/* per course: the gate's models (indices into the actor's model list) */
typedef struct TripleModels {
  int32_t start_a;          /* 0x00: part 0 at the start */
  int32_t parts_b;          /* 0x04: parts 1 and 2 */
  int32_t finish_a;         /* 0x08: part 0 at the finish */
} TripleModels;
HW_SIZE(TripleModels, 12);
/* per course: where the parts stand, three per segment */
typedef struct TriplePlace {
  Vec3 position;            /* 0x00 */
  f32 angle;                /* 0x0C */
} TriplePlace;
HW_SIZE(TriplePlace, 16);
#define TRIPLE_MODELS ((const TripleModels *)hw_ptr(0x8C087B5Cu)) /* by course */
#define TRIPLE_PLACES ((const gaddr *)hw_ptr(0x8C098E50u))        /* by course: TriplePlace array */

/* was func_8C048456: the gate's base construction: the Actor and its three (empty) positions */
gaddr triple_base_construct(gaddr obj) {
  actor_construct(obj);
  array_construct(3, sizeof(Vec3), member_addr(obj, Triple, position), CTOR_EMPTY_VEC3_TRIPLE);
  return obj;
}
HW_EXPORT(8c048456, RET(triple_base_construct(ARG0)))

/* was func_8C048476: type 0x1F's constructor: both vtables, then the base */
gaddr triple_construct(gaddr obj) {
  Triple *t = (Triple *)hw_ptr(obj);
  t->base.vtable = 0x8C23DD94u;
  t->second_vtable = 0x8C23DDB4u;
  triple_base_construct(obj);
  return obj;
}
HW_EXPORT(8c048476, RET(triple_construct(ARG0)))

/* was func_8C048500: set the gate up from its placement: param 0 puts it at the course's start segment, else at
 * the finish; its models and its parts' places come from the course's tables; then its cell is found */
void triple_init(gaddr obj, gaddr placement) {
  Triple *t = (Triple *)hw_ptr(obj);
  const ActorPlacement *pl = (const ActorPlacement *)hw_ptr(placement);
  t->param = pl->param;
  int32_t course = race_setup_at(g_race_setup)->course;
  int32_t start = course_start_segment(race_scene_at(g_panel)->course);
  int32_t finish = course_finish_segment(race_scene_at(g_panel)->course);
  const TripleModels *m = &TRIPLE_MODELS[course];
  const TriplePlace *place = (const TriplePlace *)hw_ptr(TRIPLE_PLACES[course]);
  if (t->param == 0) {
    t->model_a = (uint32_t)m->start_a;
    t->model_b = (uint32_t)m->parts_b;
    place += start * 3;
  } else {
    t->model_a = (uint32_t)m->finish_a;
    t->model_b = (uint32_t)m->parts_b;
    place += (finish - 1) * 3 + 12;
  }
  for (int32_t k = 0; k < 3; k++, place++) {
    t->position[k] = place->position;
    t->angle[k] = place->angle;
  }
  actor_locate(obj, member_addr(obj, Triple, position[0]), 0);
}
HW_EXPORT(8c048500, triple_init(ARG0, ARG1))

/* ---- type 0x23: the turner ---- */

/* was func_8C0487C4: type 0x23's constructor: both vtables, then the Actor */
gaddr turner_construct(gaddr obj) {
  Turner *t = (Turner *)hw_ptr(obj);
  t->base.vtable = 0x8C23DDD4u;
  t->second_vtable = 0x8C23DDF4u;
  actor_base_construct(obj);
  return obj;
}
HW_EXPORT(8c0487c4, RET(turner_construct(ARG0)))

/* was func_8C048844: set it up from its placement: position, kind (the param), angle about y; switched on */
void turner_init(gaddr obj, gaddr placement) {
  Turner *t = (Turner *)hw_ptr(obj);
  const ActorPlacement *pl = (const ActorPlacement *)hw_ptr(placement);
  t->position = pl->position;
  t->kind = (int32_t)pl->param;
  t->angle = pl->angle.y;
  t->base.flags = 1;
}
HW_EXPORT(8c048844, turner_init(ARG0, ARG1))

/* was func_8C048882: kind 2 counts its angle up by 1; past 180 it starts again at 0 and, when a view's camera is on
 * cells 125-129, that view plays sound 0x28 */
void turner_update(gaddr obj) {
  Turner *t = (Turner *)hw_ptr(obj);
  if (t->kind != 2) return;
  t->angle = fadd(t->angle, F32(1.0f));
  if (!fgt(t->angle, F32(180.0f))) return;
  t->angle = F32(0.0f);
  int32_t found = 0, v;
  for (v = 0; v < (int32_t)t->base.word0c; v++) {
    int32_t cell = race_scene_view_cell(g_panel, v);
    if (cell >= 125 && cell <= 129) {
      found = 1;
      break;
    }
  }
  if (found) race_scene_sound(g_panel, v, 0, 8, 0x28);
}
HW_EXPORT(8c048882, turner_update(ARG0))

/* was func_8C04891E: draw kinds 0 and 1 (models 6 and 7 of the list) turned by the angle, when the current view's
 * course row has bit 0x86 or the race scene's view has bit 0x8C or 0x91 */
void turner_draw(gaddr obj) {
  const Turner *t = (const Turner *)hw_ptr(obj);
  njPushMatrix(0);
  njTranslateV(0, member_addr(obj, Turner, position));
  njRotateY(0, f32_to_int(t->angle));
  if (t->kind == 0 || t->kind == 1) {
    gaddr scene = g_panel;
    if (course_row_bit(race_scene_at(scene)->course, view_current_index(), 0x86) ||
        race_scene_view_bit(scene, 0x8C) || race_scene_view_bit(scene, 0x91))
      nj_ModelL1(actor_model(&t->base, 6 + t->kind));
  }
  njPopMatrix(1);
}
HW_EXPORT(8c04891e, turner_draw(ARG0))

/* ---- type 0x1C: a model with a motion ---- */

/* was func_8C049640: draw the model at its position, turned about z, x, y: with the shape motion of its motion
 * entry, else with the motion, else still */
void type1c_draw(gaddr obj) {
  const ActorType1C *a = (const ActorType1C *)hw_ptr(obj);
  njSetTexture(a->texlist);
  njPushMatrix(0);
  njTranslateV(0, member_addr(obj, ActorType1C, position));
  njRotateZ(0, f32_to_int(a->angle.z));
  njRotateX(0, f32_to_int(a->angle.x));
  njRotateY(0, f32_to_int(a->angle.y));
  const MotionEntry *e = &((const MotionEntry *)hw_ptr(a->motions))[a->motion];
  if (e->shape != 0)
    shape_motion_draw_l1(a->object, e->motion, e->shape, member_addr(obj, ActorType1C, frame), 1);
  else if (e->motion != 0)
    motion_draw_l1(a->object, e->motion, member_addr(obj, ActorType1C, frame), 1);
  else
    nj_ModelL1(a->object);
  njPopMatrix(1);
}
HW_EXPORT(8c049640, type1c_draw(ARG0))

/* ---- type 0x2B ---- */

static void body1_rest(Body1 *b) {
  b->value = F32(0.0f);
  b->velocity = F32(0.0f);
  b->force = F32(0.0f);
  b->damping = F32(1.0f);
}

/* was func_8C049B86: type 0x2B's base construction: the Actor, six Body1s at rest, the Body at rest at the origin,
 * two pairs of (empty) points and the contacts owned by the Body (radius 5, room for 2) */
gaddr type2b_base_construct(gaddr obj) {
  ActorType2B *a = (ActorType2B *)hw_ptr(obj);
  actor_construct(obj);
  for (int32_t k = 0; k < 6; k++) body1_rest(&a->values[k]);
  gaddr body = member_addr(obj, ActorType2B, body);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, position));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, velocity));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, force));
  vec3_set(F32(1.0f), F32(1.0f), F32(1.0f), member_addr(body, Body, damping));
  array_construct(2, sizeof(Vec3), member_addr(obj, ActorType2B, points_a), CTOR_EMPTY_VEC3_2B);
  array_construct(2, sizeof(Vec3), member_addr(obj, ActorType2B, points_b), CTOR_EMPTY_VEC3_2B);
  contacts_construct(member_addr(obj, ActorType2B, contacts), body, GUEST_VAR(f32, 0x8C08818Cu) /* 5.0 */, 2);
  return obj;
}
HW_EXPORT(8c049b86, RET(type2b_base_construct(ARG0)))

/* was func_8C049CB0: type 0x2B's constructor: both vtables, then the base */
gaddr type2b_construct(gaddr obj) {
  ActorType2B *a = (ActorType2B *)hw_ptr(obj);
  a->base.vtable = 0x8C23DED4u;
  a->second_vtable = 0x8C23DEF4u;
  type2b_base_construct(obj);
  return obj;
}
HW_EXPORT(8c049cb0, RET(type2b_construct(ARG0)))
