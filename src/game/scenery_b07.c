/* Pen Pen port, S17 b07: the destructors and draws of the scenery actors written in S14 b11 (the bumper, the flap,
 * the flock, the turning gate, the geyser, the swimmer, the sign, the buoy, types 0x25 and 0x11), and the copies of
 * shared inline methods their units keep (game/actor.h).
 *
 * SHC emits, per class, a destructor body, a deleting destructor (the body, then syFree when asked: the vtables
 * point at it) and a destructor that runs the body and returns the object. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/inline.h"
#include "../core/ninja.h"
#include "game.h"
#include "actor.h"
#include "actor_b05.h"
#include "race_scene.h"
#include "scenery.h"
#include "actor_b07.h"

/* func_8C06270C (SHC runtime): run `dtor` on every element of an array from operator new[] */
static void array_delete(gaddr array, gaddr dtor) { CALL2(0x8C06270Cu, array, dtor); }
#define CONTACTS_FREE 0x8C013D38u /* contacts_free (core/contact.c): the swimmer's spheres' destructor */

/* SDK njColorBlendingMode (0x8C0A23A8) */
static void njColorBlendingMode(int32_t target, int32_t mode) { CALL2(0x8C0A23A8u, target, mode); }

#define FLOCK_SPRITE_ATTR  0x30
#define FLOCK_SPRITE_RISE  F32(7.5f)  /* the second sprite above the first */
#define TURNING_GATE_MODEL 48 /* in the actor's model list */
#define FLAP_MODEL         12

/* ---- this batch's copies of race_scene_view_cell (game/camera.c): the course cell under view i's camera
 * (-1 = the current view) ---- */
/* was func_8C052906, func_8C05351E, func_8C054826, func_8C056038, func_8C059BAA */
HW_EXPORT(8c052906, RET(race_scene_view_cell(ARG0, (int32_t)ARG1)))
HW_EXPORT(8c05351e, RET(race_scene_view_cell(ARG0, (int32_t)ARG1)))
HW_EXPORT(8c054826, RET(race_scene_view_cell(ARG0, (int32_t)ARG1)))
HW_EXPORT(8c056038, RET(race_scene_view_cell(ARG0, (int32_t)ARG1)))
HW_EXPORT(8c059baa, RET(race_scene_view_cell(ARG0, (int32_t)ARG1)))

/* was func_8C052852: a copy of body_damping_none (game/actor_types.c): a Body's damping (1, 1, 1) */
HW_EXPORT(8c052852, body_damping_none(ARG0))

/* was func_8C058B44 and func_8C059F88: copies of body_reset_set3 (game/actor.c): a Body at rest (the constructor
 * of the Body arrays of types 0x26 and 0x27) */
HW_EXPORT(8c058b44, RET(body_reset_set3(ARG0)))
HW_EXPORT(8c059f88, RET(body_reset_set3(ARG0)))

/* ---- the classes whose destructor body is empty (types 0x0A, 0x25, 0x11): copies of empty_delete and
 * empty_destroy_self (game/actor.c, game/actor_lifetimes.c) ---- */
/* was func_8C0556D8, func_8C059614, func_8C059C52 */
HW_EXPORT(8c0556d8, RET(empty_delete(ARG0, (int32_t)ARG1)))
HW_EXPORT(8c059614, RET(empty_delete(ARG0, (int32_t)ARG1)))
HW_EXPORT(8c059c52, RET(empty_delete(ARG0, (int32_t)ARG1)))
/* was func_8C0556FC, func_8C059638, func_8C059C76 */
HW_EXPORT(8c0556fc, RET(empty_destroy_self(ARG0)))
HW_EXPORT(8c059638, RET(empty_destroy_self(ARG0)))
HW_EXPORT(8c059c76, RET(empty_destroy_self(ARG0)))

/* ---- type 0x0C: the bumper (its destructor body is actor_b_destroy's copy func_8C053050) ---- */

/* was func_8C053064: a copy of actor_b_delete (game/actor.c) */
HW_EXPORT(8c053064, RET(actor_b_delete(ARG0, (int32_t)ARG1)))
/* was func_8C053088: a copy of actor_b_destroy_self (game/actor_lifetimes.c) */
HW_EXPORT(8c053088, RET(actor_b_destroy_self(ARG0)))

/* was func_8C053216: draw the bumper: model `size` of its list at its position, swollen in x and z by its pulse */
void bumper_draw(gaddr obj) {
  Bumper *b = (Bumper *)hw_ptr(obj);
  njPushMatrix(0);
  njTranslateV(0, member_addr(obj, Bumper, position));
  f32 swell = b->pulse.value;
  njScale(0, swell, F32(1.0f), swell);
  nj_ModelL1(actor_model(&b->base, b->size));
  njPopMatrix(1);
}
HW_EXPORT(8c053216, bumper_draw(ARG0))

/* ---- type 0x0D: the flap ---- */

/* was func_8C053648: the flap's destructor body: its contacts freed */
void flap_destroy(gaddr obj) { contacts_free(member_addr(obj, Flap, contacts)); }
HW_EXPORT(8c053648, flap_destroy(ARG0))

/* was func_8C05365E: the flap's deleting destructor */
gaddr flap_delete(gaddr obj, int32_t free_memory) {
  flap_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c05365e, RET(flap_delete(ARG0, (int32_t)ARG1)))

/* was func_8C053682 */
gaddr flap_destroy_self(gaddr obj) {
  flap_destroy(obj);
  return obj;
}
HW_EXPORT(8c053682, RET(flap_destroy_self(ARG0)))

/* was func_8C0539B4: draw the flap: model 12 of its list at its position, turned about z by its swing */
void flap_draw(gaddr obj) {
  Flap *f = (Flap *)hw_ptr(obj);
  njPushMatrix(0);
  njTranslateV(0, member_addr(obj, Flap, position));
  njRotateZ(0, f32_to_int(f->swing.value));
  nj_ModelL1(actor_model(&f->base, FLAP_MODEL));
  njPopMatrix(1);
}
HW_EXPORT(8c0539b4, flap_draw(ARG0))

/* ---- type 0x29: the flock ---- */

/* was func_8C055150: the flock's destructor body: its Body array freed (when there is one) */
void flock_destroy(gaddr obj) {
  gaddr bodies = ((Flock *)hw_ptr(obj))->bodies;
  if (bodies != 0) syFree(bodies);
}
HW_EXPORT(8c055150, flock_destroy(ARG0))

/* was func_8C055198 */
gaddr flock_delete(gaddr obj, int32_t free_memory) {
  flock_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c055198, RET(flock_delete(ARG0, (int32_t)ARG1)))

/* was func_8C0551BC */
gaddr flock_destroy_self(gaddr obj) {
  flock_destroy(obj);
  return obj;
}
HW_EXPORT(8c0551bc, RET(flock_destroy_self(ARG0)))

/* was func_8C0554AE: draw the flock: blending (source 8, destination 6); at each body its sprite with frame 0, and
 * 7.5 above it with frame 1 + (i & 3) */
void flock_draw(gaddr obj) {
  Flock *f = (Flock *)hw_ptr(obj);
  njColorBlendingMode(0, 8);
  njColorBlendingMode(1, 6);
  gaddr body = f->bodies;
  gaddr sprite = member_addr(obj, Flock, sprite);
  for (int32_t i = 0; i < f->count; i++, body += sizeof(Body)) {
    const Body *b = body_at(body);
    f->sprite.p.x = b->position.x;
    f->sprite.p.y = b->position.y;
    f->sprite.p.z = b->position.z;
    njDrawSprite3D(sprite, 0, FLOCK_SPRITE_ATTR);
    f->sprite.p.y = fadd(f->sprite.p.y, FLOCK_SPRITE_RISE);
    njDrawSprite3D(sprite, (i & 3) + 1, FLOCK_SPRITE_ATTR);
  }
}
HW_EXPORT(8c0554ae, flock_draw(ARG0))

/* ---- type 0x17: the turning gate ---- */

/* was func_8C055AFE: the gate's destructor body: its contacts freed */
void turning_gate_destroy(gaddr obj) { contacts_free(member_addr(obj, TurningGate, contacts)); }
HW_EXPORT(8c055afe, turning_gate_destroy(ARG0))

/* was func_8C055B12 */
gaddr turning_gate_delete(gaddr obj, int32_t free_memory) {
  turning_gate_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c055b12, RET(turning_gate_delete(ARG0, (int32_t)ARG1)))

/* was func_8C055B36 */
gaddr turning_gate_destroy_self(gaddr obj) {
  turning_gate_destroy(obj);
  return obj;
}
HW_EXPORT(8c055b36, RET(turning_gate_destroy_self(ARG0)))

/* was func_8C055D38: draw the gate: model 48 of its list at its position, turned about y */
void turning_gate_draw(gaddr obj) {
  TurningGate *g = (TurningGate *)hw_ptr(obj);
  njPushMatrix(0);
  njTranslateV(0, member_addr(obj, TurningGate, position));
  njRotateY(0, f32_to_int(g->turn.value));
  nj_ModelL1(actor_model(&g->base, TURNING_GATE_MODEL));
  njPopMatrix(1);
}
HW_EXPORT(8c055d38, turning_gate_draw(ARG0))

/* ---- type 0x18: the geyser ---- */

/* was func_8C0560FE: the geyser's destructor body: its motes destroyed */
void geyser_destroy(gaddr obj) { motes_destroy(member_addr(obj, Geyser, motes)); }
HW_EXPORT(8c0560fe, geyser_destroy(ARG0))

/* was func_8C056114 */
gaddr geyser_delete(gaddr obj, int32_t free_memory) {
  geyser_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c056114, RET(geyser_delete(ARG0, (int32_t)ARG1)))

/* was func_8C056138 */
gaddr geyser_destroy_self(gaddr obj) {
  geyser_destroy(obj);
  return obj;
}
HW_EXPORT(8c056138, RET(geyser_destroy_self(ARG0)))

/* ---- type 0x21: the swimmer ---- */

/* was func_8C0568D0: the swimmer's destructor body: its contact spheres destroyed (contacts_free on each) and
 * freed, then their points freed */
void swimmer_destroy(gaddr obj) {
  gaddr spheres = ((Swimmer *)hw_ptr(obj))->spheres;
  if (spheres != 0) {
    array_delete(spheres, CONTACTS_FREE);
    syFree(spheres);
  }
  gaddr points = ((Swimmer *)hw_ptr(obj))->points;
  if (points != 0) syFree(points);
}
HW_EXPORT(8c0568d0, swimmer_destroy(ARG0))

/* was func_8C05690A */
gaddr swimmer_delete(gaddr obj, int32_t free_memory) {
  swimmer_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c05690a, RET(swimmer_delete(ARG0, (int32_t)ARG1)))

/* was func_8C05692E */
gaddr swimmer_destroy_self(gaddr obj) {
  swimmer_destroy(obj);
  return obj;
}
HW_EXPORT(8c05692e, RET(swimmer_destroy_self(ARG0)))

/* ---- type 0x22: the sign ---- */

/* was func_8C0577E4: the sign's destructor body (sign_destroy, game/actor_sign.c), returning the object */
gaddr sign_destroy_self(gaddr obj) {
  sign_destroy(obj);
  return obj;
}
HW_EXPORT(8c0577e4, RET(sign_destroy_self(ARG0)))

/* ---- type 0x2A: the buoy ---- */

/* was func_8C058554: the buoy's destructor body: its contacts freed */
void buoy_destroy(gaddr obj) { contacts_free(member_addr(obj, Buoy, contacts)); }
HW_EXPORT(8c058554, buoy_destroy(ARG0))

/* was func_8C05856A */
gaddr buoy_delete(gaddr obj, int32_t free_memory) {
  buoy_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c05856a, RET(buoy_delete(ARG0, (int32_t)ARG1)))

/* was func_8C05858E */
gaddr buoy_destroy_self(gaddr obj) {
  buoy_destroy(obj);
  return obj;
}
HW_EXPORT(8c05858e, RET(buoy_destroy_self(ARG0)))
