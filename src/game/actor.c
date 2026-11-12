/* Pen Pen port, S13 (batch b): small methods of the scene's actor and effect classes (0x8C0438E8-0x8C04C7A6):
 * constructors and destructors, record tables, per-view words, bodies at rest. S14: the classes are declared in
 * game/actor.h (the actors and their group), game/course.h (the course, a player's CourseEnv) and
 * game/race_scene.h. */
#include "hw.h"
#include "sdk.h"
#include "../core/task.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/view.h"
#include "../core/file.h"
#include "../core/ninja.h"
#include "game.h"
#include "actor.h"
#include "course.h"
#include "race_scene.h"

/* ---- a CourseEnv's cell keys (CellKey: {cell, a, b}, sorted by cell) ---- */

/* was func_8C0438E8: the cell keys (the list and the two words after it) = src */
void course_env_set_keys(gaddr env, gaddr src) {
  CourseEnv *e = course_env_at(env);
  const CellKeys *k = (const CellKeys *)hw_ptr(src);
  e->keys.list = k->list;
  e->keys.word4 = k->word4;
  e->keys.word8 = k->word8;
}
HW_EXPORT(8c0438e8, course_env_set_keys(ARG0, ARG1))

/* was func_8C0438FC: the values of the first cell key whose cell is not before the camera's cell */
void course_env_update_keys(gaddr env) {
  CourseEnv *e = course_env_at(env);
  const CellKey *k = (const CellKey *)hw_ptr(e->keys.list);
  while (e->cell > k->cell) k++;
  e->key_a = k->a;
  e->key_b = k->b;
}
HW_EXPORT(8c0438fc, course_env_update_keys(ARG0))

/* was func_8C043A90: the course's table, its five slots emptied (-1) and the x of the rotation at 0x1AC cleared */
void course_env_set_table(gaddr env, gaddr table) {
  CourseEnv *e = course_env_at(env);
  e->course_table = table;
  for (int32_t i = 0; i < 5; i++) e->slots[i] = -1;
  field_set_f32(member_addr(env, CourseEnv, rot1ac), 0, F32(0.0f));
}
HW_EXPORT(8c043a90, course_env_set_table(ARG0, ARG1))

/* was func_8C043D2E: the course's actor placements */
gaddr course_actor_placements(gaddr course) { return course_at(course)->actors; }
HW_EXPORT(8c043d2e, RET(course_actor_placements(ARG0)))

/* was func_8C044424: the Actor base's constructor: no flags; returns the actor */
gaddr actor_construct(gaddr actor) {
  actor_at(actor)->flags = 0;
  return actor;
}
HW_EXPORT(8c044424, RET(actor_construct(ARG0)))

/* ---- the group of actors ---- */

/* was func_8C0446B0: draw each actor of the group: the draw method of its vtable, on the actor moved by the
 * method's adjustment */
void actor_group_draw(gaddr group) {
  ActorGroup *g = actor_group_at(group);
  for (int32_t i = 0; i < g->count; i++) {
    gaddr actor = g->actors[i];
    const VMethod *m = &actor_vtbl(actor)->draw;
    hw_call(m->fn, 1, actor + (uint32_t)m->this_adjust, 0, 0, 0);
  }
}
HW_EXPORT(8c0446b0, actor_group_draw(ARG0))

/* was func_8C044EEA: add v to the `push` Vec3 of CourseEnv i of the scene's course. (S13 had read the other half of
 * the literal-pool word, 0xAC, for Course.envs and so added to the wrong place; the difftest missed it because the
 * routes only push by zero vectors: found reading the code in S14, fixed) */
void race_scene_env_push(gaddr scene, int32_t i, gaddr v) {
  gaddr env = member_addr(race_scene_at(scene)->course, Course, envs[i]);
  vec3_add(member_addr(env, CourseEnv, push), v);
}
HW_EXPORT(8c044eea, race_scene_env_push(ARG0, (int32_t)ARG1, ARG2))

/* was func_8C044F1E: this unit's copy of race_scene_view_cell (game/camera.c, func_8C04094E): the course cell under
 * view i's camera (-1 = the current view). S13 had read the other half of the literal-pool word (0xCC instead of
 * RaceScene.view_cell, 0xAC): its difftest failure; fixed in S14 */
HW_EXPORT(8c044f1e, RET(race_scene_view_cell(ARG0, (int32_t)ARG1)))

/* ---- destructors ----
 * A deleting destructor runs the class's destructor body, then frees the object when asked; it returns the
 * object. Some bodies are empty. */

static gaddr delete_after(gaddr obj, int32_t free_memory) {
  if (free_memory) syFree(obj);
  return obj;
}

/* func_8C04503C / func_8C047978: destructor bodies (still translated) */
static void destroy_body_8c04503c(gaddr obj) { CALL1(0x8C04503Cu, obj); }
static void destroy_body_8c047978(gaddr obj) { CALL1(0x8C047978u, obj); }

/* was func_8C045050 */
gaddr actor_a_delete(gaddr obj, int32_t free_memory) {
  destroy_body_8c04503c(obj);
  return delete_after(obj, free_memory);
}
HW_EXPORT(8c045050, RET(actor_a_delete(ARG0, (int32_t)ARG1)))

/* was func_8C04798C */
gaddr actor_b_delete(gaddr obj, int32_t free_memory) {
  destroy_body_8c047978(obj);
  return delete_after(obj, free_memory);
}
HW_EXPORT(8c04798c, RET(actor_b_delete(ARG0, (int32_t)ARG1)))

/* was func_8C046070, func_8C046714, func_8C047598, func_8C048498, func_8C049516: the deleting destructors of
 * classes whose body is empty (method_empty, core/inline.c) */
gaddr empty_delete(gaddr obj, int32_t free_memory) {
  method_empty();
  return delete_after(obj, free_memory);
}
HW_EXPORT(8c046070, RET(empty_delete(ARG0, (int32_t)ARG1)))
HW_EXPORT(8c046714, RET(empty_delete(ARG0, (int32_t)ARG1)))
HW_EXPORT(8c047598, RET(empty_delete(ARG0, (int32_t)ARG1)))
HW_EXPORT(8c048498, RET(empty_delete(ARG0, (int32_t)ARG1)))
HW_EXPORT(8c049516, RET(empty_delete(ARG0, (int32_t)ARG1)))

/* ---- constructors of actor classes with two bases (S14: S13 took them for destructor bodies; the factory
 * func_8C043D4C calls them on the new memory): both vtables, then the class's thunk to the Actor constructor (still
 * translated) ---- */

/* was func_8C0466F2: type 4, Strip (0x58 bytes; thunk func_8C0466DE) */
gaddr strip_construct(gaddr obj) {
  Strip *p = (Strip *)hw_ptr(obj);
  p->base.vtable = 0x8C23DC8Cu;
  p->second_vtable = 0x8C23DCACu;
  CALL1(0x8C0466DEu, obj);
  return obj;
}
HW_EXPORT(8c0466f2, RET(strip_construct(ARG0)))
/* was func_8C047576: type 0x0E (0x44 bytes; thunk func_8C047562) */
gaddr actor_type0e_construct(gaddr obj) {
  ActorType0E *p = (ActorType0E *)hw_ptr(obj);
  p->base.vtable = 0x8C23DD0Cu;
  p->second_vtable = 0x8C23DD2Cu;
  CALL1(0x8C047562u, obj);
  return obj;
}
HW_EXPORT(8c047576, RET(actor_type0e_construct(ARG0)))
/* was func_8C0494F6: type 0x1C (0x90 bytes; thunk func_8C0494E2) */
gaddr actor_type1c_construct(gaddr obj) {
  ActorType1C *p = (ActorType1C *)hw_ptr(obj);
  p->base.vtable = 0x8C23DE94u;
  p->second_vtable = 0x8C23DEB4u;
  CALL1(0x8C0494E2u, obj);
  return obj;
}
HW_EXPORT(8c0494f6, RET(actor_type1c_construct(ARG0)))

/* ---- small setters ---- */

/* was func_8C04763E: the angle += 364.09 (two degrees in Ninja units) */
HW_EXPORT(8c04763e, f32_add_to(F32_BITS(0x43B60B60u), member_addr(ARG0, Spinner, angle)))

/* was func_8C047EAA: a buffer holder: its size, its two blocks empty; returns it */
gaddr buffer_pair_init(gaddr obj, uint32_t size) {
  BufferPair *b = (BufferPair *)hw_ptr(obj);
  b->block_a = 0;
  b->block_b = 0;
  b->size = size;
  return obj;
}
HW_EXPORT(8c047eaa, RET(buffer_pair_init(ARG0, ARG1)))

/* was func_8C047ED0: free the holder's two blocks */
void buffer_pair_free(gaddr obj) {
  BufferPair *b = (BufferPair *)hw_ptr(obj);
  gaddr p = b->block_b;
  if (p != 0) syFree(p);
  p = b->block_a;
  if (p != 0) syFree(p);
  b->block_b = 0;
  b->block_a = 0;
}
HW_EXPORT(8c047ed0, buffer_pair_free(ARG0))

/* was func_8C04844A / func_8C048450: the course's start / finish segment */
int32_t course_start_segment(gaddr course) { return course_at(course)->start_segment; }
int32_t course_finish_segment(gaddr course) { return course_at(course)->finish_segment; }
HW_EXPORT(8c04844a, RET(course_start_segment(ARG0)))
HW_EXPORT(8c048450, RET(course_finish_segment(ARG0)))

/* was func_8C0499D4, func_8C04AD28: a Body at rest */
HW_EXPORT(8c0499d4, body_reset(ARG0))
HW_EXPORT(8c04ad28, body_reset(ARG0))

/* was func_8C04AEEE: a Body at rest, through the setters that return the vector */
gaddr body_reset_set3(gaddr b) {
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(b, Body, position));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(b, Body, velocity));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(b, Body, force));
  return vec3_set(F32(1.0f), F32(1.0f), F32(1.0f), member_addr(b, Body, damping));
}
HW_EXPORT(8c04aeee, RET(body_reset_set3(ARG0)))

/* was func_8C04B4DE: reset an effect: its counter cleared, word164 = word_ec, its table the static one, both scales
 * the default, and every particle's state off */
#define g_effect_default_scale GUEST_VAR(f32, 0x8C0881C0u)  /* 0.24999999 */
#define g_effect_slots        GUEST_VAR(int32_t, 0x8C0881B0u) /* 32 */
#define EFFECT_STATIC_TABLE   0x8C098F28u
void effect_reset(gaddr fx) {
  Effect *e = (Effect *)hw_ptr(fx);
  e->counter = 0;
  e->word164 = e->word_ec;
  e->table = EFFECT_STATIC_TABLE;
  e->scale_a = g_effect_default_scale;
  e->scale_b = g_effect_default_scale;
  gaddr state = e->states;
  for (int32_t n = g_effect_slots; n > 0; n--) {
    U32_AT(state) = 0;
    state += 4;
  }
}
HW_EXPORT(8c04b4de, effect_reset(ARG0))

/* was func_8C04C6F8: a Body at rest at the origin (vec.c's body_init) */
HW_EXPORT(8c04c6f8, body_init(ARG0))

/* func_8C06270C (SHC runtime): destroy an array from operator new[] with the destructor `dtor` */
static void array_delete(gaddr array, gaddr dtor) { CALL2(0x8C06270Cu, array, dtor); }

/* was func_8C04C780: free the object's array of members, their destructor being empty */
void member_array_free(gaddr obj) {
  gaddr array = ((MemberArray *)hw_ptr(obj))->array;
  if (array == 0) return;
  array_delete(array, 0x8C04C6F4u /* method_empty */);
  syFree(array);
}
HW_EXPORT(8c04c780, member_array_free(ARG0))

/* func_8C062790 (SHC runtime): construct `n` objects of `size` bytes in a block from operator new[] (returns it) */
static gaddr array_construct_new(int32_t n, uint32_t size, gaddr p, gaddr ctor) {
  return CALL4(0x8C062790u, n, size, p, ctor);
}

/* was func_8C04C708: an array holder: its rotation the identity, `count` members of 0x48 bytes built with
 * func_8C04C6F8 (a Body at rest); returns it */
gaddr member_array_init(gaddr obj, int32_t count) {
  MemberArray *a = (MemberArray *)hw_ptr(obj);
  a->rotation.x = F32(0.0f);
  a->rotation.y = F32(0.0f);
  a->rotation.z = F32(0.0f);
  a->rotation.w = F32(1.0f);
  a->count = count;
  gaddr block = syMalloc((uint32_t)(count ? count : 1) * 0x48);
  gaddr array = 0;
  if (block != 0) array = array_construct_new(count, 0x48, block, 0x8C04C6F8u);
  a->array = array;
  return obj;
}
HW_EXPORT(8c04c708, RET(member_array_init(ARG0, (int32_t)ARG1)))

/* ---- more group and actor methods ---- */


/* was func_8C044600: tear a group down: destroy its actors (their destructor, with "free" = 1), free its
 * 0-terminated block list, release and free its texture list, release the texture list of its data file (the
 * file's second word) and free the file */
void actor_group_release(gaddr group) {
  ActorGroup *g = actor_group_at(group);
  for (int32_t i = 0; i < g->count; i++) {
    gaddr actor = g->actors[i];
    if (actor == 0) continue;
    const VMethod *m = &actor_vtbl(actor)->destroy;
    hw_call(m->fn, 2, actor + (uint32_t)m->this_adjust, 1, 0, 0);
  }
  for (const gaddr *p = g->blocks; *p != 0; p++) syFree(*p);
  njReleaseTexture(g->texlist);
  syFree(g->texlist);
  njReleaseTexture(((const gaddr *)hw_ptr(g->file))[1]);
  file_free(g->file);
}
HW_EXPORT(8c044600, actor_group_release(ARG0))

/* was func_8C046564: draw the actor's two models: the first turned by its angle, the second by the value of its
 * Body1 */
void actor_pair_draw(gaddr actor) {
  ActorPair *a = (ActorPair *)hw_ptr(actor);
  njPushMatrix(0);
  njTranslateV(0, member_addr(actor, ActorPair, position_a));
  njRotateY(0, f32_to_int(a->angle_a));
  nj_ModelL1(a->model_a);
  njPopMatrix(1);
  njPushMatrix(0);
  njTranslateV(0, member_addr(actor, ActorPair, position_b));
  matrix_rotate_y(field_get_f32(member_addr(actor, ActorPair, angle_b), 0));
  nj_ModelL1(a->model_b);
  njPopMatrix(1);
}
HW_EXPORT(8c046564, actor_pair_draw(ARG0))

/* was func_8C046842: write a scrolling strip's texture coordinates: quad_count quads of (u0, u1) and v from the
 * scroll plus v0, each quad_height high; then the scroll moves by its speed and wraps to 0 past 255 */
void strip_scroll(gaddr strip) {
  Strip *s = (Strip *)hw_ptr(strip);
  int16_t *uv = (int16_t *)hw_ptr(s->uv);
  int32_t v = s->v0 + s->scroll;
  for (int32_t k = 0; k < s->quad_count; k++) {
    uv[0] = s->u0;
    uv[1] = (int16_t)v;
    uv[2] = s->u0;
    uv[3] = (int16_t)(s->quad_height + v);
    uv[4] = s->u1;
    uv[5] = (int16_t)v;
    uv[6] = s->u1;
    uv[7] = (int16_t)(s->quad_height + v);
    uv += 8;
    v += s->quad_height;
  }
  s->scroll = (int16_t)(s->scroll + s->scroll_speed);
  if (s->scroll > 0xFF) s->scroll = 0;
}
HW_EXPORT(8c046842, strip_scroll(ARG0))

/* was func_8C046FA0: kick piece i: down by 10, damping 0.98 on its Body and on its tip, whose value is reset (the
 * Body1 through the class-neutral float setters) */
void piece_kick(gaddr obj, int32_t i) {
  gaddr p = member_addr(obj, Pieces, pieces[i]);
  f32_add_to(F32(-10.0f), member_addr(p, Piece, body.position.y));
  vec3_fill(member_addr(p, Piece, body.damping), F32_BITS(0x3F7AE147u)); /* 0.98 */
  field_set_f32(member_addr(p, Piece, tip), offsetof(Body1, value), F32(0.0f));
  field_set_f32(member_addr(p, Piece, tip), offsetof(Body1, damping), F32_BITS(0x3F7AE147u));
}
HW_EXPORT(8c046fa0, piece_kick(ARG0, (int32_t)ARG1))

#define STRIP_MODELS ((const gaddr *)hw_ptr(0x8C098B2Cu)) /* the strip's model (NJS_OBJECT) per kind */

/* was func_8C046778: set a scrolling strip up from its placement {type, kind, position}: its position, its sound
 * words and scroll speed by kind, its model and, from the model's first mesh set, the quad count, the texture
 * coordinates to write and their base values; switched on */
void strip_init(gaddr strip, gaddr placement) {
  Strip *s = (Strip *)hw_ptr(strip);
  const ActorPlacement *pl = (const ActorPlacement *)hw_ptr(placement);
  Vec3 pos = pl->position;
  s->position = pos;
  switch ((int32_t)pl->param) {
  case 0:
    s->sound_a = 0x41;
    s->sound_b = 0x44;
    s->scroll_speed = 1;
    break;
  case 1:
    s->sound_a = 0x0F;
    s->sound_b = 0x13;
    s->scroll_speed = 1;
    break;
  case 2: case 3: case 4: case 5:
    s->sound_a = 0x23;
    s->sound_b = 0x24;
    s->scroll_speed = 4;
    break;
  default:
    break;
  }
  gaddr model = STRIP_MODELS[pl->param];
  s->model = model;
  const NjModel *m = (const NjModel *)hw_ptr(((const NjObject *)hw_ptr(model))->model);
  const NjMeshset *meshset = (const NjMeshset *)hw_ptr(m->meshsets); /* the first */
  s->quad_count = meshset->nb_meshes;
  gaddr uv = meshset->vertex_uvs;
  s->uv = uv;
  const int16_t *t = (const int16_t *)hw_ptr(uv);
  s->u0 = t[0];
  s->v0 = t[1];
  int32_t v1 = t[3];
  s->u1 = t[4];
  s->quad_height = (int16_t)(v1 - s->v0);
  s->scroll = 0;
  s->base.flags = 1;
}
HW_EXPORT(8c046778, strip_init(ARG0, ARG1))

/* was func_8C047F38: an effect's defaults: off, no velocity, gravity -10, scales (2, 2, 2) and (-2, 1, -2), damping
 * 0.98, a spin of -0.0545, the shared texture list and its texture, no blocks */
void effect_defaults(gaddr fx) {
  EffectParams *p = (EffectParams *)hw_ptr(fx);
  p->on = 0;
  p->word08 = 0;
  p->velocity = (Vec3){F32(0.0f), F32(0.0f), F32(0.0f)};
  p->gravity = F32(-10.0f);
  const f32 two = F32_BITS(0x3FFFFFFFu); /* 1.9999999 */
  p->scale_a = (Vec3){two, two, two};
  p->scale_b = (Vec3){F32_BITS(0xBFFFFFFFu), F32_BITS(0x3F7FFFFFu), F32_BITS(0xBFFFFFFFu)};
  p->damping = F32_BITS(0x3F7AE147u); /* 0.98 */
  p->spin = F32_BITS(0xBD5F0122u);    /* -0.05444 */
  p->texlist = 0x8C09703Cu;           /* a texture list (static) */
  p->texture = 0x8C097044u;
  p->block_a = 0;
  p->block_b = 0;
}
HW_EXPORT(8c047f38, effect_defaults(ARG0))

/* nj_ModelL3 (Ninja, S2 name): draw a chunk model (another light mode than nj_ModelL1) */
static inline void nj_ModelL3(gaddr model) { CALL1(0x8C0D1996u, model); }
/* func_8C046B84: whether something of this size is on the panel's view (still translated) */
static int32_t panel_sees(gaddr panel, uint32_t what) { return (int32_t)CALL2(0x8C046B84u, panel, what); }


/* was func_8C0473C2: draw the 15 pieces that are not gone (state < 2) and that the race scene sees: placed and
 * turned by their angles; a falling piece (state 0) also tips by its Body1 and hangs 10 below; model 8 or 9 of the
 * actor's list by its flag */
void pieces_draw(gaddr obj) {
  gaddr rec = member_addr(obj, Pieces, pieces);
  for (int32_t i = 0; i < PIECE_COUNT; i++, rec += sizeof(Piece)) {
    Piece *p = (Piece *)hw_ptr(rec);
    if (p->state >= 2) continue;
    uint32_t what = p->size;
    if (!panel_sees(panel_get(), what)) continue;
    matrix_push();
    matrix_translate_v(method_self(rec));
    matrix_rotate_x(p->angle.x);
    matrix_rotate_y(p->angle.y);
    matrix_rotate_z(p->angle.z);
    if (p->state == 0) {
      matrix_rotate_x(field_get_f32(member_addr(rec, Piece, tip), 0));
      matrix_translate(F32(0.0f), F32(-10.0f), F32(0.0f));
    }
    const Actor *a = actor_at(obj);
    gaddr model = p->alt_model != 0 ? actor_model(a, 8) : actor_model(a, 9);
    nj_ModelL1(model);
    matrix_pop();
  }
}
/* not exported (S13 difftest: differs from the original, to redo): HW_EXPORT(8c0473c2, pieces_draw(ARG0)) */

/* was func_8C0485D0: draw a three-part object: part 0 (model_a of the actor's list) at its place turned by its
 * angle, and again turned half a turn and moved 7 along x; parts 1 and 2 (model_b) at theirs */
void triple_draw(gaddr obj) {
  Triple *t = (Triple *)hw_ptr(obj);
  njPushMatrix(0);
  njTranslateV(0, member_addr(obj, Triple, position[0]));
  njRotateY(0, f32_to_int(t->angle[0]));
  nj_ModelL3(actor_model(&t->base, (int32_t)t->model_a));
  njRotateY(0, 0x8000);
  njTranslate(0, F32_BITS(0x40DFFFFFu), F32(0.0f), F32(0.0f)); /* 6.9999995 */
  nj_ModelL3(actor_model(&t->base, (int32_t)t->model_a));
  njPopMatrix(1);
  for (uint32_t k = 1; k <= 2; k++) {
    njPushMatrix(0);
    njTranslateV(0, member_addr(obj, Triple, position[k]));
    njRotateY(0, f32_to_int(t->angle[k]));
    nj_ModelL1(actor_model(&t->base, (int32_t)t->model_b));
    njPopMatrix(1);
  }
}
HW_EXPORT(8c0485d0, triple_draw(ARG0))

#define g_effect_limit GUEST_VAR(f32, 0x8C0881CCu) /* 20.0 */

/* was func_8C04B5AC: step the effect's particles by their state: 1 = wait out the countdown, then go on; 2 = count
 * the value up by 1 to the limit, then go on; 3 = off. A moving particle steps its Body and its animation frame
 * (0..6) */
void effect_update(gaddr fx) {
  const Effect *e = (const Effect *)hw_ptr(fx);
  gaddr frame = e->frames, countdown = e->countdowns, state = e->states;
  gaddr value = e->values, body = e->bodies;
  for (int32_t n = g_effect_slots; n > 0; n--) {
    int32_t *s = (int32_t *)hw_ptr(state);
    int32_t moving = 0;
    switch (*s) {
    case 1: {
      int32_t *c = (int32_t *)hw_ptr(countdown);
      int32_t left = (*c)--;
      if (left < 0) (*s)++;
      moving = 1;
      break;
    }
    case 2:
      if (fgt(g_effect_limit, F32_AT(value)))
        F32_AT(value) = fadd(F32_AT(value), F32(1.0f));
      else
        (*s)++;
      moving = 1;
      break;
    case 3:
      *s = 0;
      break;
    default:
      break;
    }
    if (moving) {
      body_step(body);
      int32_t *f = (int32_t *)hw_ptr(frame);
      if (++*f > 6) *f = 0;
    }
    frame += 4;
    countdown += 4;
    state += 4;
    value += 4;
    body += sizeof(Body);
  }
}
HW_EXPORT(8c04b5ac, effect_update(ARG0))

/* a Body1 at rest with no damping (the setters' copies, core/inline.c) */
static void body1_clear(gaddr b) {
  field_set_f32(b, offsetof(Body1, value), F32(0.0f));
  field_set(b, offsetof(Body1, velocity), 0);
  field_set(b, offsetof(Body1, force), 0);
  field_set_f32(b, offsetof(Body1, damping), F32(1.0f));
}

#define CHAIN_LINK_TABLE 0x8C098D60u /* 20 pairs of floats */

/* was func_8C0479EC: set a chain up from its placement {type, param, position}: its Body at the position, its
 * contacts (none in use, owned by the Body, radius 20, mass 1, no response: func_8C047838 is empty), its swing
 * (value 1.5) and 20
 * links (two floats each from the static table; Body1s at 20, damping 0.75 - 0.017 k) */
void chain_init(gaddr chain, gaddr placement) {
  Chain *c = (Chain *)hw_ptr(chain);
  const ActorPlacement *pl = (const ActorPlacement *)hw_ptr(placement);
  gaddr body = member_addr(chain, Chain, body);
  vec3_zero(member_addr(body, Body, position));
  vec3_zero(member_addr(body, Body, velocity));
  vec3_zero(member_addr(body, Body, force));
  vec3_fill(member_addr(body, Body, damping), F32(1.0f));
  c->body.position.x = pl->position.x;
  c->body.position.y = pl->position.y;
  c->body.position.z = pl->position.z;
  c->body.damping.z = F32(1.0f);
  c->body.damping.y = F32(1.0f);
  c->body.damping.x = F32(1.0f);
  gaddr contacts = member_addr(chain, Chain, contacts);
  field_reset_2c(contacts);
  c->contacts.cell = c->base.cell;
  c->contacts.owner = body;
  c->contacts.owner_velocity = member_addr(body, Body, velocity);
  c->contacts.radius = F32(20.0f);
  c->contacts.mass = F32(1.0f);
  c->contacts.unknown18 = F32(1.0f);
  c->contacts.unknown1c = 4;
  c->contacts.respond = 0x8C047838u; /* an empty method */
  c->param = pl->param;
  c->word_b4 = F32(0.0f);
  gaddr swing = member_addr(chain, Chain, swing);
  body1_clear(swing);
  c->swing.value = F32_BITS(0x3FBFFFFFu); /* 1.4999999 */
  c->swing.damping = F32(1.0f);
  c->swing.velocity = F32(0.0f);
  const f32 *table = (const f32 *)hw_ptr(CHAIN_LINK_TABLE);
  for (int32_t k = 0; k < CHAIN_LINKS; k++) {
    c->link_a[k] = *table++;
    c->link_b[k] = *table++;
    gaddr link = member_addr(chain, Chain, links[k]);
    body1_clear(link);
    c->links[k].value = F32(20.0f);
    c->links[k].damping = fsub(F32(0.75f), fmul(f32_from_int(k), F32_BITS(0x3C8B4395u))); /* 0.017 */
  }
}
HW_EXPORT(8c0479ec, chain_init(ARG0, ARG1))
