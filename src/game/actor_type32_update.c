/* Pen Pen port, S18 b05: the update of the scenery actor of placement type 0x32 (game/actor_b08.h: ActorType32;
 * its other methods are in game/actor_type32.c).
 *
 * The actor has two states. At rest (state 0) it bobs in place: its frame runs up fast while its Body is pushed up,
 * then slowly while it sinks back; after three bobs it drifts back to its home position for 240 steps. A player
 * that hits it hard enough (or its wait running out) wakes it (state 1, the second model): it sets off a ring,
 * squashes its scale Body1s, widens its contacts, and while awake throws a splash every four steps and plays a
 * sound to the players near it at frames 10, 20 and 30; when the wait runs out it goes back to rest. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/inline.h"
#include "actor.h"
#include "actor_b08.h"
#include "race_scene.h" /* race_scene_view_cell, race_scene_slot_insert */
#include "game.h"       /* panel_get */
#include "scenery.h"    /* random_scaled, race_scene_sound */
#include "../core/file.h" /* NjMotion */
#include "../core/motion_frame.h"

/* statics of the binary the update reads */
#define g_type32_radius       GUEST_VAR(f32, 0x8C08A310u) /* 5: the contacts' radius at rest */
#define g_type32_awake_radius GUEST_VAR(f32, 0x8C08A314u) /* 25: the contacts' radius while awake */
#define g_type32_hit_recoil   GUEST_VAR(f32, 0x8C08A32Cu) /* -0.0925926: a hit's normal, scaled into the velocity */
#define g_type32_squash       GUEST_VAR(f32, 0x8C08A330u) /* 0.1: a hit's push on the scale Body1s, and their pull */

#define TYPE32_AWAKE_PUSH   0x8C060190u /* the awake contacts' handler: an empty method */
#define TYPE32_REST_PUSH    0x8C0615F8u /* actor_type32_contact_push, as the guest calls it */
#define TYPE32_HIT_SPEED    F32_BITS(0x3EED097Au) /* 0.46296293: the speed of a hit that wakes it */
#define TYPE32_HIT_PLAYERS  7                     /* contacts whose other word is above this are not players */
#define TYPE32_HIT_WAIT     120                   /* a hit's wait */
#define TYPE32_RING_LEAD    60                    /* the ring waits that much less than the actor */
#define TYPE32_WAKE_RANGE   F32(120.0f)           /* the wait running out: awake for random 0..120 + 120 */
#define TYPE32_WAKE_BASE    F32(120.0f)
#define TYPE32_REST_RANGE   F32(240.0f)           /* then at rest for random 0..240 + 180 */
#define TYPE32_REST_BASE    F32(180.0f)
#define TYPE32_SHRINK_WAIT  5                     /* the last steps awake: scale 0.3 */
#define TYPE32_SHRINK_SCALE F32_BITS(0x3E999999u) /* 0.3 */
#define TYPE32_SPLASH_EVERY 4
#define TYPE32_SOUND_RANGE  F32(1.0f)             /* players within one course cell hear it */
#define TYPE32_SOUND_BANK   8
#define TYPE32_SOUND_ID     0x26
#define TYPE32_TURN_DAMPING F32_BITS(0x3F4CCCCCu) /* 0.8 */
#define TYPE32_HOME_STEPS   240
#define TYPE32_HOME_TIME    F32(240.0f)
#define TYPE32_BOBS         3                     /* idle bobs before it goes home */
#define TYPE32_RISE_END     F32(30.0f)            /* the bob's frame rises fast below this */
#define TYPE32_BOB_DAMPING  F32_BITS(0x3F6E147Au) /* 0.93 */
#define TYPE32_RISE_FORCE   F32_BITS(0x3D23D70Au) /* 0.04 */
#define TYPE32_SINK_FORCE   F32_BITS(0xBB83126Eu) /* -0.004 */
#define TYPE32_HOME_TOP     F32(10.0f)            /* on the way home the frame swings between 0 and this */

/* the original's frame (0x1C bytes under r8, fr12 and PR): the hit's recoil Vec3 goes to the vector functions */
typedef struct UpdateFrame {
  Vec3 recoil;       /* 0x00 */
  gaddr normal;      /* 0x0C: the contact's normal */
  int32_t i;         /* 0x10: the contact */
  int32_t other;     /* 0x14: the contact's other word (then the player of the sound loop) */
  gaddr self;        /* 0x18 */
  uint32_t saved[3]; /* 0x1C: PR, fr12, r8 */
} UpdateFrame;
HW_OFFSET(UpdateFrame, normal, 0x0C);
HW_OFFSET(UpdateFrame, self, 0x18);
HW_SIZE(UpdateFrame, 0x28);

static Contacts *type32_contacts(ActorType32 *a) { return &a->contacts; }

/* the contacts set for a state: the actor's radius, then the handler (the unit's inline setters 0x8C060154,
 * 0x8C060180) */
static void type32_contacts_set(ActorType32 *a, f32 radius, gaddr handler) {
  a->radius = radius;
  type32_contacts(a)->radius = a->radius;
  type32_contacts(a)->respond = handler;
}

/* a random wait: random 0..range made whole, plus base */
static int32_t type32_random_wait(f32 range, f32 base) {
  return f32_to_int(fadd(f32_from_int(f32_to_int(random_scaled(range))), base));
}

/* the actor woken: its ring set off TYPE32_RING_LEAD steps before its wait ends, its contacts widened */
static void type32_wake_ripple(gaddr obj) {
  ActorType32 *a = (ActorType32 *)hw_ptr(obj);
  actor_type32_spawn_ripple(obj, a->wait - TYPE32_RING_LEAD);
  type32_contacts_set(a, g_type32_awake_radius, TYPE32_AWAKE_PUSH);
}

/* state 0: the wait counted down (at its end: awake for a random time); then every contact with a player (other
 * word up to 7) recoils the Body along its normal, and a hit faster than TYPE32_HIT_SPEED squashes the scale Body1s
 * and wakes the actor for TYPE32_HIT_WAIT steps */
static void type32_rest(gaddr obj, UpdateFrame *f, gaddr frame) {
  ActorType32 *a = (ActorType32 *)hw_ptr(obj);
  a->wait -= 1;
  if (a->wait < 0) {
    a->state = 1;
    a->alternate = 1;
    a->wait = type32_random_wait(TYPE32_WAKE_RANGE, TYPE32_WAKE_BASE);
    type32_wake_ripple(obj);
  }
  gaddr body = member_addr(obj, ActorType32, body);
  gaddr recoil = member_addr(frame, UpdateFrame, recoil);
  f->other = -1;
  for (f->i = 0; f->i < type32_contacts(a)->count; f->i++) {
    const Contacts *c = type32_contacts(a);
    if (((const int32_t *)hw_ptr(c->other))[f->i] > TYPE32_HIT_PLAYERS) continue;
    f->other = ((const int32_t *)hw_ptr(c->other))[f->i];
    f->normal = c->normal + (uint32_t)f->i * sizeof(Vec3);
    method_self(recoil); /* func_8C05FD2C: the Vec3's (empty) constructor */
    vec3_zero(recoil);
    vec3_scaled(g_type32_hit_recoil, recoil, f->normal);
    vec3_add(member_addr(body, Body, velocity), recoil); /* func_8C060106 */
    if (!fgt(njScalor(c->velocity + (uint32_t)f->i * sizeof(Vec3)), TYPE32_HIT_SPEED)) continue;
    for (int32_t k = 0; k < 3; k++) f32_add_to(g_type32_squash, member_addr(obj, ActorType32, turns[k].force));
    a->state = 1;
    a->alternate = 1;
    a->wait = TYPE32_HIT_WAIT;
    vec3_fill(member_addr(body, Body, damping), TYPE32_TURN_DAMPING); /* func_8C0600C6 */
    type32_wake_ripple(obj);
  }
}

/* state 1: the wait counted down (at its end: at rest for a random time, the narrow contacts, the frame 0); the
 * scale 0.3 for the last steps; a splash every TYPE32_SPLASH_EVERY steps (C's signed remainder) */
static void type32_awake(gaddr obj) {
  ActorType32 *a = (ActorType32 *)hw_ptr(obj);
  a->wait -= 1;
  if (a->wait < 0) {
    a->state = 0;
    a->alternate = 0;
    a->wait = type32_random_wait(TYPE32_REST_RANGE, TYPE32_REST_BASE);
    type32_contacts_set(a, g_type32_radius, TYPE32_REST_PUSH);
    a->frame = F32(0.0f);
  }
  if (a->wait < TYPE32_SHRINK_WAIT) a->scale = TYPE32_SHRINK_SCALE;
  if (a->wait % TYPE32_SPLASH_EVERY == 0) actor_type32_spawn_splash(obj);
}

/* awake: the frame advanced; at frames 10, 20 and 30 every player within a course cell of the actor hears it */
static void type32_awake_frame(gaddr obj, UpdateFrame *f) {
  ActorType32 *a = (ActorType32 *)hw_ptr(obj);
  a->frame = fadd(a->frame, F32(1.0f));
  if (!feq(a->frame, F32(10.0f)) && !feq(a->frame, F32(20.0f)) && !feq(a->frame, F32(30.0f))) return;
  for (f->other = 0; f->other < (int32_t)a->base.word0c; f->other++) {
    int32_t cell = a->base.cell;
    int32_t near = race_scene_view_cell(panel_get(), f->other);
    if (fgt(f32_abs(f32_from_int(cell - near)), TYPE32_SOUND_RANGE)) continue;
    race_scene_sound(panel_get(), f->other, 0, TYPE32_SOUND_BANK, TYPE32_SOUND_ID);
  }
}

/* at rest, after TYPE32_BOBS bobs: the trip home starts, the velocity that covers the way in 240 steps, no damping */
static void type32_start_home(gaddr obj) {
  ActorType32 *a = (ActorType32 *)hw_ptr(obj);
  gaddr body = member_addr(obj, ActorType32, body);
  const Body *b = body_at(body);
  a->word44 = TYPE32_HOME_STEPS;
  a->sinking = 0;
  a->v64.y = fsub(a->home.y, b->position.y); /* the unit's inline getters (func_8C060042, ...) */
  a->v64.y = fmul(a->v64.y, f32_reciprocal(TYPE32_HOME_TIME));
  a->v64.x = fsub(a->home.x, b->position.x);
  a->v64.x = fmul(a->v64.x, f32_reciprocal(TYPE32_HOME_TIME));
  a->v64.z = fsub(a->home.z, b->position.z);
  a->v64.z = fmul(a->v64.z, f32_reciprocal(TYPE32_HOME_TIME));
  vec3_fill(member_addr(body, Body, damping), F32(1.0f)); /* func_8C0600C6 */
}

/* at rest, not going home: one step of the bob (fast and pushed up below frame 30, then slow and sinking to the
 * motion's last frame, which counts a bob and starts the next) */
static void type32_bob(gaddr obj) {
  ActorType32 *a = (ActorType32 *)hw_ptr(obj);
  gaddr body = member_addr(obj, ActorType32, body);
  if (a->word40 >= TYPE32_BOBS) type32_start_home(obj);
  if (fgt(TYPE32_RISE_END, a->frame)) {
    a->frame = fadd(a->frame, fadd(F32(1.0f), F32(1.0f)));
    vec3_fill(member_addr(body, Body, damping), TYPE32_BOB_DAMPING);
    f32_add_to(TYPE32_RISE_FORCE, member_addr(body, Body, force.y)); /* func_8C06014A */
    return;
  }
  a->frame = fadd(a->frame, F32(0.5f));
  vec3_fill(member_addr(body, Body, damping), TYPE32_BOB_DAMPING);
  f32_add_to(TYPE32_SINK_FORCE, member_addr(body, Body, force.y));
  const MotionEntry *e = &((const MotionEntry *)hw_ptr(a->motions))[a->motion];
  a->frames = f32_from_uint((uint32_t)(((const NjMotion *)hw_ptr(e->motion))->nb_frames - 1));
  if (fgt(a->frames, a->frame)) return;
  a->frame = F32(0.0f);
  a->word40 += 1;
}

/* at rest, going home: the Body moved by the home velocity, the frame swinging between 0 and 10; at the end the
 * bobs start again from frame 0, the Body stopped */
static void type32_go_home(gaddr obj) {
  ActorType32 *a = (ActorType32 *)hw_ptr(obj);
  gaddr body = member_addr(obj, ActorType32, body);
  Body *b = body_at(body);
  b->velocity.x = a->v64.x; /* the unit's inline setters (func_8C0600D4, ...) */
  b->velocity.y = a->v64.y;
  b->velocity.z = a->v64.z;
  if (a->sinking != 0) {
    if (fgt(a->frame, F32(0.0f)))
      a->frame = fsub(a->frame, F32(0.5f));
    else
      a->sinking = 0;
  } else {
    if (fgt(TYPE32_HOME_TOP, a->frame))
      a->frame = fadd(a->frame, F32(0.5f));
    else
      a->sinking = 1;
  }
  a->word44 -= 1;
  if (a->word44 > 0) return;
  a->word44 = 0;
  a->word40 = 0;
  a->frame = F32(0.0f);
  vec3_zero(member_addr(body, Body, velocity)); /* func_8C060074 */
}

/* was func_8C060706: the update method: the state's step (0 at rest, 1 awake, others nothing); the scale Body1s
 * pulled to the scale and stepped, the scale back to 1; the frame (awake) or the bob / the trip home (at rest); the
 * Body stepped, the contacts put in the race scene's slot 8, the ripples stepped */
void actor_type32_update(gaddr obj) {
  ActorType32 *a = (ActorType32 *)hw_ptr(obj);
  gaddr frame = hw_stack_alloc(sizeof(UpdateFrame));
  UpdateFrame *f = (UpdateFrame *)hw_ptr(frame);
  f->self = obj;
  switch (a->state) {
  case 0:
    type32_rest(obj, f, frame);
    break;
  case 1:
    type32_awake(obj);
    break;
  default:
    break;
  }
  for (int32_t k = 0; k < 3; k++) body1_pull(a->scale, g_type32_squash, member_addr(obj, ActorType32, turns[k]));
  for (int32_t k = 0; k < 3; k++) body1_step(member_addr(obj, ActorType32, turns[k]));
  a->scale = F32(1.0f);
  if (a->alternate != 0)
    type32_awake_frame(obj, f);
  else if (a->word44 == 0)
    type32_bob(obj);
  else
    type32_go_home(obj);
  body_step(member_addr(obj, ActorType32, body));
  gaddr contacts = member_addr(obj, ActorType32, contacts);
  gaddr spill = hw_stack_alloc(4); /* the original keeps the contacts on the stack across panel_get */
  *(gaddr *)hw_ptr(spill) = contacts;
  gaddr scene = panel_get();
  hw_stack_free(4);
  race_scene_slot_insert(scene, contacts, 8);
  actor_type32_update_ripples(obj);
  hw_stack_free(sizeof(UpdateFrame));
}
HW_EXPORT(8c060706, actor_type32_update(ARG0))
