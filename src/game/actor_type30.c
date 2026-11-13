/* Pen Pen port, S18 b05: the scenery actor of placement type 0x30 (game/actor_b08.h: ActorType30; its
 * construction, reset and puff spawns are in game/actor_types_b08.c): something that hides flat in the ground. When a
 * player comes within 50 it rises (its scale y pulled to 1.5), puffs, and turns to follow the player; one coming
 * within 30 sets off its motion (and a sound); after its time (or its motion) it sinks back and waits again. Its
 * puffs (up to four) rise and fade by themselves. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/inline.h"
#include "../core/file.h" /* ContainerEntry, NjMotion */
#include "actor.h"
#include "actor_b08.h"
#include "actor_b07.h"            /* matrix_load, matrix_assign */
#include "../core/motion_frame.h" /* shape_motion_draw_l3b */
#include "race_scene.h" /* race_scene_view_cell, race_scene_slot_insert */
#include "game.h"       /* panel_get */
#include "scenery.h"    /* random_timer_scaled, race_scene_sound, race_player_body */

/* func_8C0628E8 (C runtime, float): atan2f(y, x) (as guest code) */
static f32 rt_atan2f(f32 y, f32 x) {
  FARGS2(y, x);
  CALL0(0x8C0628E8u);
  return hw_fret();
}

/* statics of the binary the unit reads */
#define g_type30_radius_a    GUEST_VAR(f32, 0x8C08A178u)     /* 1.9999999 */
#define g_type30_radius_b    GUEST_VAR(f32, 0x8C08A17Cu)     /* 0.99999994 */
#define g_type30_scale_rate  GUEST_VAR(f32, 0x8C08A180u)     /* 0.1: the scale Body1s' pull */
#define g_type30_scale_damp  GUEST_VAR(f32, 0x8C08A184u)     /* 0.95 */
#define g_type30_sink_damp   GUEST_VAR(f32, 0x8C08A188u)     /* 0.7: scale y's damping while it sinks */
#define g_type30_wake_range  GUEST_VAR(f32, 0x8C08A18Cu)     /* 50: a player this near wakes it */
#define g_type30_act_range   GUEST_VAR(f32, 0x8C08A190u)     /* 30: a player this near sets off its motion */
#define g_type30_puff_object GUEST_VAR(int32_t, 0x8C08A194u) /* 6: the puffs' object in the shared table */
#define g_type30_puff_motion GUEST_VAR(int32_t, 0x8C08A198u) /* 2: their motion entry in the shared table */
#define g_type30_puffs       GUEST_VAR(int32_t, 0x8C08A19Cu) /* 4: the puffs in use */
#define g_type30_puff_fade   GUEST_VAR(f32, 0x8C08A1A4u)     /* 20: a puff's fade runs up to it */
#define g_type30_grow_b      GUEST_VAR(f32, 0x8C08A1ACu)     /* 0.025: a kind-1 puff's growth */
#define g_type30_grow_a      GUEST_VAR(f32, 0x8C08A1B0u)     /* 0.05: a kind-0 puff's growth */
#define g_shared_objects     GUEST_VAR(gaddr, 0x8C096D14u)   /* gaddr[]: the shared effect objects */
#define g_shared_motions     GUEST_VAR(gaddr, 0x8C096D18u)   /* MotionEntry[]: the shared effect motions */
#define g_effects_texlist    GUEST_VAR(gaddr, 0x8C096D10u)   /* the shared effects' texlist (the reset's word384) */

#define TYPE30_START_STATE   4
#define TYPE30_START_TIMER   180
#define TYPE30_TURN_DAMPING  F32_BITS(0x3F4CCCCCu) /* 0.8 */
#define TYPE30_WIDE          F32(1.5f)             /* its scale x and z, and y when risen */
#define TYPE30_POINT_RISE    F32(5.0f)             /* point_a above the Body */
#define TYPE30_CONTACTS_1C_A 2
#define TYPE30_CONTACTS_1C_B 1
#define TYPE30_RESPOND_A     0x8C05E920u           /* the contacts' responses (guest code) */
#define TYPE30_RESPOND_B     0x8C05E986u
#define TYPE30_CONTACT_WORD  4                     /* the Contacts' word after it (field_reset_2c) */
#define TYPE30_RISE_TIME     60
#define TYPE30_WAIT_TIME     300
#define TYPE30_ACT_TIME      120                   /* added to the timer when its motion starts */
#define TYPE30_PUFF_EVERY    30
#define TYPE30_SOUND_RANGE   F32(1.0f)
#define TYPE30_SOUND_BANK    8
#define TYPE30_SOUND_WAKE    0x29
#define TYPE30_SOUND_ACT     0x3E
#define TYPE30_FLAT          F32_BITS(0x3D4CCCCCu) /* 0.05: a scale y below this is flat */
#define TYPE30_YAW_RATE      F32_BITS(0x3CF5C28Fu) /* 0.03 */
#define TYPE30_ANGLE_UNITS   F32(65536.0f)
#define TYPE30_TWO_PI        F32_BITS(0x40C90FD7u) /* 6.2831836 */
#define TYPE30_SLOW_FROM     F32(60.0f)            /* motions 4-6: frames 60..61 crawl by 0.01 */
#define TYPE30_SLOW_TO       F32(61.0f)
#define TYPE30_SLOW_STEP     F32_BITS(0x3C23D70Au) /* 0.01 */
#define TYPE30_WAVE_HOLD     F32(30.0f)            /* motions 4-6: the wave 2 to frame 30, down to 0 at 60, then up */
#define TYPE30_WAVE_SPAN     F32(15.0f)

static ActorType30 *type30_at(gaddr obj) { return (ActorType30 *)hw_ptr(obj); }
static gaddr entry_data(gaddr entry) { return (gaddr)((const ContainerEntry *)hw_ptr(entry))->offset; }
static Body1 *type30_scale(gaddr obj, int32_t k) { return &type30_at(obj)->quats_a[k]; }
static Body1 *type30_angle(gaddr obj, int32_t k) { return &type30_at(obj)->quats_b[k]; }

/* the frame count - 1 of the actor's motion */
static f32 type30_motion_frames(const ActorType30 *a) {
  const MotionEntry *e = &((const MotionEntry *)hw_ptr(a->motions))[a->motion];
  return f32_from_uint((uint32_t)(((const NjMotion *)hw_ptr(e->motion))->nb_frames - 1));
}

/* a Body1 at rest (the inline Quat constructor), then its value and damping (the unit's inline setters) */
static void type30_body1_set(Body1 *b, f32 value, f32 damping) {
  quat_identity(hw_addr(b));
  b->value = value;
  b->damping = damping;
}

/* the contacts set up: empty, its radius and kind word, mass 1, its cell, a response */
static void type30_contacts_set(const ActorType30 *a, Contacts *c, int32_t *word, f32 radius, int32_t kind,
                                gaddr respond) {
  c->count = 0; /* field_reset_2c's copy: the count and the word after the Contacts */
  *word = TYPE30_CONTACT_WORD;
  c->unknown18 = F32(1.0f);
  c->radius = radius;
  c->mass = F32(1.0f);
  c->unknown1c = kind;
  c->cell = a->base.cell;
  c->respond = respond;
}

/* point_a: the Body's position, 5 higher */
static void type30_place_point(gaddr obj) {
  ActorType30 *a = type30_at(obj);
  memmove(&a->point_a, hw_ptr(method_self(member_addr(obj, ActorType30, body))), sizeof(Vec3));
  a->point_a.y = fadd(a->point_a.y, TYPE30_POINT_RISE);
}

/* was func_8C05D268 (the init method: this, the placement): its Body at rest at the placement's position; its
 * objects, textures and motions from the group's entries; motion 0 at a random frame; sinking (state 4, 180 steps,
 * the puff sent); its angles the placement's (damping 0.8); its scale 1.5, 0, 1.5 (damping 0.95); both contacts
 * (point_a 5 above the Body); the puffs reset */
void actor_type30_init(gaddr obj, gaddr placement) {
  ActorType30 *a = type30_at(obj);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  a->word28 = 0;
  a->word34 = 0;
  gaddr body = member_addr(obj, ActorType30, body);
  body_reset(body);
  vec3_set(p->position.x, p->position.y, p->position.z, body);
  a->objects = entry_data(a->base.model_set);
  a->textures = entry_data(a->base.texlist);
  a->motions = entry_data(a->base.word1c);
  a->motion = 0;
  a->frames = type30_motion_frames(a);
  a->frame = random_timer_scaled(a->frames);
  a->timer = TYPE30_START_TIMER;
  a->state = TYPE30_START_STATE;
  a->following = 0;
  a->hidden = 0;
  a->motion_done = 0;
  a->puffed = 1;
  type30_body1_set(type30_angle(obj, 0), p->angle.x, TYPE30_TURN_DAMPING);
  type30_body1_set(type30_angle(obj, 1), p->angle.y, TYPE30_TURN_DAMPING);
  type30_body1_set(type30_angle(obj, 2), p->angle.z, TYPE30_TURN_DAMPING);
  a->height = F32(0.0f);
  for (int32_t k = 0; k < 3; k++) {
    Body1 *s = type30_scale(obj, k);
    quat_identity(hw_addr(s));
    s->damping = g_type30_scale_damp;
    s->value = k == 1 ? a->height : TYPE30_WIDE;
  }
  type30_contacts_set(a, &a->contacts_a, &a->contacts_a_word, g_type30_radius_a, TYPE30_CONTACTS_1C_A,
                      TYPE30_RESPOND_A);
  type30_place_point(obj);
  type30_contacts_set(a, &a->contacts_b, &a->contacts_b_word, g_type30_radius_b, TYPE30_CONTACTS_1C_B,
                      TYPE30_RESPOND_B);
  actor_type30_reset(obj);
}
HW_EXPORT(8c05d268, actor_type30_init(ARG0, ARG1))

/* ---- the puffs ---- */

/* was func_8C05E440: the puffs' object and motion from the shared tables; one step of every puff. Kind 0: rising
 * (1) it plays the puff motion to its end, then fades in (2) to g_type30_puff_fade growing by 0.05 a step, then is
 * freed (3). Kind 1: it waits its timer (1), fades in (2) growing by 0.025, then is freed (3). */
void actor_type30_step_puffs(gaddr obj) {
  ActorType30 *a = type30_at(obj);
  a->word360 = ((const gaddr *)hw_ptr(g_shared_objects))[g_type30_puff_object];
  const MotionEntry *e = &((const MotionEntry *)hw_ptr(g_shared_motions))[g_type30_puff_motion];
  a->entry364 = e->motion;
  a->word368 = e->shape;
  int32_t *kind = (int32_t *)hw_ptr(a->puff_kind);
  int32_t *timer = (int32_t *)hw_ptr(a->puff_timer);
  int32_t *state = (int32_t *)hw_ptr(a->puff_state);
  f32 *fade = (f32 *)hw_ptr(a->puff_fade);
  f32 *scale = (f32 *)hw_ptr(a->puff_scale);
  f32 *frame = (f32 *)hw_ptr(a->puff_frame);
  for (int32_t n = g_type30_puffs; n > 0; n--, kind++, timer++, state++, fade++, scale++, frame++) {
    const f32 *growth = 0;
    if (*kind == 0) {
      switch (*state) {
      case 1: {
        *frame = fadd(*frame, F32(1.0f));
        const NjMotion *m = (const NjMotion *)hw_ptr(a->entry364);
        if (fgt(f32_from_uint((uint32_t)(m->nb_frames - 1)), *frame)) break;
        *frame = f32_from_uint((uint32_t)(m->nb_frames - 1));
        *fade = F32(0.0f);
        *state += 1;
        break;
      }
      case 2:
        if (fgt(g_type30_puff_fade, *fade)) {
          *fade = fadd(*fade, F32(1.0f));
        } else {
          *fade = g_type30_puff_fade;
          *state += 1;
        }
        growth = &g_type30_grow_a;
        break;
      case 3:
        *state = 0;
        break;
      default:
        break;
      }
    } else if (*kind == 1) {
      switch (*state) {
      case 1:
        *timer -= 1;
        if (*timer < 0) *state += 1;
        growth = &g_type30_grow_b;
        break;
      case 2:
        if (fgt(g_type30_puff_fade, *fade))
          *fade = fadd(*fade, F32(1.0f));
        else
          *state += 1;
        growth = &g_type30_grow_b;
        break;
      case 3:
        *state = 0;
        break;
      default:
        break;
      }
    }
    if (growth != 0) *scale = fadd(*scale, *growth);
  }
}
HW_EXPORT(8c05e440, actor_type30_step_puffs(ARG0))

/* ---- the update ---- */

/* the original's frame of the update (0x24 bytes under r8 and PR): race_scene_nearest_player writes into it */
typedef struct Type30Frame {
  Vec3 to_player;    /* 0x00: from the Body to its player (the sound loops keep their player in its z) */
  int32_t nearest;   /* 0x0C */
  uint32_t word10;   /* 0x10: an (empty) Vec3 constructor's */
  union {
    Vec3 dir;        /* 0x14: the nearest player's direction */
    struct {
      uint8_t pad_14[8];
      f32 distance;  /* 0x1C: its distance (written after the direction) */
    };
  };
  gaddr self;        /* 0x20 */
  uint32_t saved[2]; /* 0x24: PR, r8 */
} Type30Frame;
HW_OFFSET(Type30Frame, nearest, 0x0C);
HW_OFFSET(Type30Frame, dir, 0x14);
HW_OFFSET(Type30Frame, distance, 0x1C);
HW_OFFSET(Type30Frame, self, 0x20);
HW_SIZE(Type30Frame, 0x2C);

/* the nearest player to the Body (its direction and distance into the frame) */
static int32_t type30_nearest(gaddr obj, gaddr frame) {
  gaddr point = method_self(member_addr(obj, ActorType30, body));
  int32_t n = race_scene_nearest_player(panel_get(), point, member_addr(frame, Type30Frame, dir),
                                        member_addr(frame, Type30Frame, distance));
  ((Type30Frame *)hw_ptr(frame))->nearest = n;
  return n;
}

/* every player within a course cell of the actor hears `sound` */
static void type30_sound_near(const ActorType30 *a, int32_t sound) {
  for (int32_t j = 0; j < (int32_t)a->base.word0c; j++) {
    int32_t cell = a->base.cell;
    int32_t near = race_scene_view_cell(panel_get(), j);
    if (fgt(f32_abs(f32_from_int(cell - near)), TYPE30_SOUND_RANGE)) continue;
    race_scene_sound(panel_get(), j, 0, TYPE30_SOUND_BANK, sound);
  }
}

/* the timer counted down: 1 when it was already below 0 */
static int32_t type30_count(ActorType30 *a) {
  int32_t t = a->timer;
  a->timer = t - 1;
  return t < 0;
}

/* sink back: state 4 for 180 steps, no following, scale y to 0 (damping 0.7), the sinking puff to come */
static void type30_sink(gaddr obj) {
  ActorType30 *a = type30_at(obj);
  a->state = TYPE30_START_STATE;
  a->following = 0;
  a->timer = TYPE30_START_TIMER;
  a->height = F32(0.0f);
  type30_scale(obj, 1)->damping = g_type30_sink_damp;
  a->puffed = 0;
}

/* one step of the state */
static void type30_step_state(gaddr obj, gaddr frame) {
  ActorType30 *a = type30_at(obj);
  Type30Frame *f = (Type30Frame *)hw_ptr(frame);
  int32_t n;
  switch (a->state) {
  case 0: /* waiting: a player within 50 wakes it */
    n = type30_nearest(obj, frame);
    if (!fgt(g_type30_wake_range, f->distance)) break;
    a->state += 1;
    a->following = 1;
    a->timer = TYPE30_RISE_TIME;
    a->height = TYPE30_WIDE;
    type30_scale(obj, 1)->damping = g_type30_scale_damp;
    a->player = n;
    actor_type30_spawn_puff(obj);
    type30_sound_near(a, TYPE30_SOUND_WAKE);
    break;
  case 1: /* risen: following the nearest player, puffing every 30 steps */
    n = type30_nearest(obj, frame);
    if (n >= 0) a->player = n;
    if (type30_count(a)) {
      a->state += 1;
      a->timer = TYPE30_WAIT_TIME;
    }
    if (a->timer % TYPE30_PUFF_EVERY == 0) actor_type30_spawn_puff_b(obj); /* the SHC remainder (signed) */
    break;
  case 2: /* waiting for a player within 30: its motion; else at the end it sinks */
    n = type30_nearest(obj, frame);
    if (n >= 0) a->player = n;
    if (fgt(g_type30_act_range, f->distance)) {
      a->motion = 1;
      a->frame = F32(0.0f);
      a->state += 1;
      a->timer += TYPE30_ACT_TIME;
      type30_sound_near(a, TYPE30_SOUND_ACT);
      break;
    }
    if (type30_count(a)) type30_sink(obj);
    if (a->timer % TYPE30_PUFF_EVERY == 0) actor_type30_spawn_puff_b(obj);
    break;
  case 3: { /* its motion: puffing every 30 steps, sinking once it is done */
    int32_t t = a->timer;
    a->timer = t - 1;
    if (t % TYPE30_PUFF_EVERY == 0) actor_type30_spawn_puff_b(obj);
    if (a->motion_done != 0) type30_sink(obj);
    break;
  }
  case 4: /* sinking: once flat a puff; at the end it waits again with motion 0 at a random frame */
    if (fgt(TYPE30_FLAT, f32_abs(type30_scale(obj, 1)->value)) && a->puffed == 0) {
      a->puffed = 1;
      actor_type30_spawn_puff(obj);
    }
    if (!type30_count(a)) break;
    a->state = 0;
    a->following = 0;
    a->motion_done = 0;
    a->timer = 0;
    a->motion = 0;
    a->frames = type30_motion_frames(a);
    a->frame = random_timer_scaled(a->frames);
    break;
  default:
    break;
  }
}

/* the wave of motions 0-3: 0 up to 2 at the motion's middle, back to 0 at its end */
static void type30_wave_updown(ActorType30 *a) {
  f32 two = fadd(F32(1.0f), F32(1.0f));
  if (fgt(fdiv(a->frames, two), a->frame))
    a->wave = fdiv(fmul(a->frame, two), a->frames);
  else
    a->wave = fadd(fneg(fmul(fdiv(a->frame, a->frames), two)), fadd(F32(1.0f), F32(1.0f)));
}

/* one step of the motion's frame (by the motion) and the wave */
static void type30_step_frame(ActorType30 *a) {
  a->frames = type30_motion_frames(a);
  switch (a->motion) {
  case 0:
  case 3: /* looping */
    a->frame = fadd(a->frame, F32(1.0f));
    if (fgt(a->frame, a->frames)) a->frame = F32(0.0f);
    type30_wave_updown(a);
    break;
  case 1:
  case 2: /* once: held at the end */
    a->frame = fadd(a->frame, F32(1.0f));
    if (fgt(a->frame, a->frames)) {
      a->frame = a->frames;
      a->motion_done = 1;
    }
    type30_wave_updown(a);
    break;
  case 4:
  case 5:
  case 6: /* crawling through frames 60-61 */
    if (fgt(TYPE30_SLOW_FROM, a->frame))
      a->frame = fadd(a->frame, F32(1.0f));
    else
      a->frame = fadd(a->frame, fgt(TYPE30_SLOW_TO, a->frame) ? TYPE30_SLOW_STEP : F32(1.0f));
    if (!fgt(a->frames, a->frame)) a->frame = F32(0.0f);
    if (fgt(TYPE30_WAVE_HOLD, a->frame)) {
      a->wave = fadd(F32(1.0f), F32(1.0f));
      a->wave_hold = fadd(F32(1.0f), F32(1.0f));
    } else if (fgt(TYPE30_SLOW_FROM, a->frame)) {
      a->wave = fadd(fneg(fdiv(fadd(a->frame, fneg(TYPE30_WAVE_HOLD)), TYPE30_WAVE_SPAN)),
                     fadd(F32(1.0f), F32(1.0f)));
      a->wave_hold = fadd(F32(1.0f), F32(1.0f));
    } else {
      a->wave = fdiv(fadd(a->frame, fneg(TYPE30_SLOW_FROM)), TYPE30_WAVE_SPAN);
      a->wave_hold = fadd(F32(1.0f), F32(1.0f));
    }
    break;
  default:
    break;
  }
}

/* was func_8C05D520: the update method: the state's step; while following, its angle about y turned toward its
 * player; its scale pulled to (1.5, height, 1.5) and stepped; hidden while its scale y is below 0.05; the motion's
 * frame stepped; point_a 5 above the Body; both contacts put in the race scene's slot 8 unless hidden; the puffs
 * stepped */
void actor_type30_update(gaddr obj) {
  ActorType30 *a = type30_at(obj);
  gaddr frame = hw_stack_alloc(sizeof(Type30Frame));
  Type30Frame *f = (Type30Frame *)hw_ptr(frame);
  f->self = obj;
  method_self(member_addr(frame, Type30Frame, word10)); /* func_8C05CBD0: a Vec3's (empty) constructor */
  type30_step_state(obj, frame);
  if (a->following != 0) {
    gaddr to_player = member_addr(frame, Type30Frame, to_player);
    method_self(to_player);
    gaddr target = race_player_body(panel_get(), a->player);
    memmove(hw_ptr(to_player), hw_ptr(target), sizeof(Vec3)); /* the SHC copy helper 0x8C0625EC */
    vec3_sub(to_player, method_self(member_addr(obj, ActorType30, body)));
    f32 angle = rt_atan2f(f->to_player.x, f->to_player.z);
    a->yaw = f32_from_int(f32_to_int(fdiv(fmul(angle, TYPE30_ANGLE_UNITS), TYPE30_TWO_PI)));
    body1_pull_angle(a->yaw, TYPE30_YAW_RATE, hw_addr(type30_angle(obj, 1)));
    body1_step(hw_addr(type30_angle(obj, 1)));
  }
  body1_pull(TYPE30_WIDE, g_type30_scale_rate, hw_addr(type30_scale(obj, 0)));
  body1_pull(a->height, g_type30_scale_rate, hw_addr(type30_scale(obj, 1)));
  body1_pull(TYPE30_WIDE, g_type30_scale_rate, hw_addr(type30_scale(obj, 2)));
  for (int32_t k = 0; k < 3; k++) body1_step(hw_addr(type30_scale(obj, k)));
  a->hidden = fgt(TYPE30_FLAT, f32_abs(type30_scale(obj, 1)->value)) ? 1 : 0;
  type30_step_frame(a);
  type30_place_point(obj);
  if (a->hidden == 0) {
    race_scene_slot_insert(panel_get(), member_addr(obj, ActorType30, contacts_a), 8);
    race_scene_slot_insert(panel_get(), member_addr(obj, ActorType30, contacts_b), 8);
  }
  actor_type30_step_puffs(obj);
  hw_stack_free(sizeof(Type30Frame));
}
HW_EXPORT(8c05d520, actor_type30_update(ARG0))

/* ---- the draw ---- */

/* SDK motion calls (port/symbols/sdk_calls.tsv; Ninja's motion state is global) */
/* S22: the motion reader is hand-written C (src/lib/ninja/motion.c; HLE stubs before) */
static void nj_MotionSetup(gaddr motion, f32 frame) { nj_motion_setup(motion, frame); }
static void nj_ShapeOnly(gaddr shape, int32_t flags, f32 frame) { FARGS1(frame); CALL2(0x8C0CE0E4u, shape, flags); }
static void nj_MotionStepA(gaddr out, int32_t n) { nj_motion_step_a(out, n); } /* a node's position */
static void nj_MotionStepB(gaddr out, int32_t n) { nj_motion_step_b(out, n); } /* its angles */
static void nj_MotionStepC(gaddr out, int32_t n) { nj_motion_step_c(out, n); } /* its scale */
static void nj_MotionEnd(void) { nj_motion_end(); }
static void njRotateZXY(gaddr m, int32_t x, int32_t y, int32_t z) { CALL4(0x8C0C4118u, m, x, y, z); }

#define TYPE30_SHAPE_FLAGS  0x29
#define TYPE30_SKY_MATRIX   0x8C1B3114u /* a matrix of the binary (the view's; copied, multiplied by the current) */
#define TYPE30_PUFF_OBJECT  0           /* parts 0: object 2 with motion entry 8; 1-4: object 1 with entry 7 */
#define TYPE30_PART_OBJECT_A 2
#define TYPE30_PART_MOTION_A 8
#define TYPE30_PART_OBJECT_B 1
#define TYPE30_PART_MOTION_B 7
#define TYPE30_PARTS        5
#define TYPE30_PUFF_TURN    F32(16384.0f)         /* a kind-1 puff lies flat: 90 degrees about x */
#define TYPE30_PUFF_CELL    0
#define TYPE30_PUFF_ATTR    0x22u

static const NjObjectPose *node_at(gaddr node) { return (const NjObjectPose *)hw_ptr(node); }

/* the original's frame of actor_type30_pose_node (0x24 bytes under eight saved registers and PR): the SDK writes the
 * node's transform into it */
typedef struct PoseFrame {
  int32_t ang[3];    /* 0x00 */
  Vec3 scl;          /* 0x0C */
  Vec3 pos;          /* 0x18 */
  uint32_t saved[9]; /* 0x24: PR, fr13-fr15, r10-r14 */
} PoseFrame;
HW_OFFSET(PoseFrame, scl, 0x0C);
HW_OFFSET(PoseFrame, pos, 0x18);
HW_SIZE(PoseFrame, 0x48);

/* was func_8C05E098 (with its part at 0x8C05E184): pose a node into the current matrix: its position, angles and
 * scale from the motion where the motion has keys for them, else the node's own (or the identity without a node);
 * translate, rotate z, x, y, scale; then the motion's next node */
void actor_type30_pose_node(gaddr obj, gaddr node) {
  ActorType30 *a = type30_at(obj);
  gaddr frame = hw_stack_alloc(sizeof(PoseFrame));
  PoseFrame *f = (PoseFrame *)hw_ptr(frame);
  const gaddr *keys = (const gaddr *)hw_ptr(a->mdata);
  if (keys[0] != 0) {
    nj_MotionStepA(member_addr(frame, PoseFrame, pos), 1);
  } else if (node == 0) {
    f->pos.x = F32(0.0f);
    f->pos.y = F32(0.0f);
    f->pos.z = F32(0.0f);
  } else {
    f->pos = node_at(node)->pos;
  }
  keys = (const gaddr *)hw_ptr(a->mdata);
  if (keys[1] != 0) {
    nj_MotionStepB(member_addr(frame, PoseFrame, ang), 1);
  } else if (node == 0) {
    f->ang[0] = 0;
    f->ang[1] = 0;
    f->ang[2] = 0;
  } else {
    f->ang[0] = node_at(node)->ang[0];
    f->ang[1] = node_at(node)->ang[1];
    f->ang[2] = node_at(node)->ang[2];
  }
  keys = (const gaddr *)hw_ptr(a->mdata);
  if (keys[2] != 0) {
    nj_MotionStepC(member_addr(frame, PoseFrame, scl), 1);
  } else if (node == 0) {
    f->scl.x = F32(1.0f);
    f->scl.y = F32(1.0f);
    f->scl.z = F32(1.0f);
  } else {
    f->scl = node_at(node)->scl;
  }
  njTranslate(0, f->pos.x, f->pos.y, f->pos.z);
  /* the angles go through floats (the original's conversion) */
  f32 z = f32_from_int(f->ang[2]);
  f32 y = f32_from_int(f->ang[1]);
  f32 x = f32_from_int(f->ang[0]);
  njRotateZXY(0, f32_to_int(x), f32_to_int(y), f32_to_int(z));
  njScale(0, f->scl.x, f->scl.y, f->scl.z);
  nj_MotionEnd();
  a->mdata += 0x18; /* NJS_MDATA3: the next node's keys */
  hw_stack_free(sizeof(PoseFrame));
}
HW_EXPORT(8c05e098, actor_type30_pose_node(ARG0, ARG1))

/* the original's frame of actor_type30_draw_part (0x18 bytes under PR): the call's arguments are read from it (an
 * unknown part leaves them as the stack had them) */
typedef struct PartFrame30 {
  f32 frame;         /* 0x00 */
  gaddr object;      /* 0x04 */
  gaddr shape;       /* 0x08 */
  gaddr motion;      /* 0x0C */
  int32_t i;         /* 0x10 */
  gaddr self;        /* 0x14 */
  uint32_t saved_pr; /* 0x18 */
} PartFrame30;
HW_OFFSET(PartFrame30, motion, 0x0C);
HW_OFFSET(PartFrame30, self, 0x14);
HW_SIZE(PartFrame30, 0x1C);

/* the part's frame: the wave or its hold, by the motion (`hold5`: motion 5 takes the hold too) */
static f32 type30_part_frame(const ActorType30 *a, int32_t hold5) {
  switch (a->motion) {
  case 6:
    return a->wave;
  case 5:
    return hold5 ? a->wave_hold : a->wave;
  case 4:
    return a->wave_hold;
  default:
    return a->wave;
  }
}

/* was func_8C05E1FE: draw part i (0-4) at its kept matrix: part 0 object 2 with motion entry 8, parts 1-4 object 1
 * with entry 7, at the wave (motions 4 (and 5, but for parts 1 and 2) at its hold) */
void actor_type30_draw_part(gaddr obj, int32_t i) {
  const ActorType30 *a = type30_at(obj);
  gaddr frame = hw_stack_alloc(sizeof(PartFrame30));
  PartFrame30 *f = (PartFrame30 *)hw_ptr(frame);
  const MotionEntry *motions = (const MotionEntry *)hw_ptr(a->motions);
  const gaddr *objects = (const gaddr *)hw_ptr(a->objects);
  f->self = obj;
  f->i = i;
  matrix_push();
  matrix_load(member_addr(obj, ActorType30, matrices[i]));
  switch (i) {
  case 0:
    f->motion = motions[TYPE30_PART_MOTION_A].motion;
    f->shape = motions[TYPE30_PART_MOTION_A].shape;
    f->object = objects[TYPE30_PART_OBJECT_A];
    f->frame = type30_part_frame(a, 1);
    break;
  case 1:
  case 2:
    f->motion = motions[TYPE30_PART_MOTION_B].motion;
    f->shape = motions[TYPE30_PART_MOTION_B].shape;
    f->object = objects[TYPE30_PART_OBJECT_B];
    f->frame = type30_part_frame(a, 0);
    break;
  case 3:
  case 4:
    f->motion = motions[TYPE30_PART_MOTION_B].motion;
    f->shape = motions[TYPE30_PART_MOTION_B].shape;
    f->object = objects[TYPE30_PART_OBJECT_B];
    f->frame = type30_part_frame(a, 1);
    break;
  default:
    break;
  }
  nj_ShapeMotionL1(f->object, f->motion, f->shape, f->frame); /* through the unit's thunk func_8C05CDA8 */
  matrix_pop();
  hw_stack_free(sizeof(PartFrame30));
}
HW_EXPORT(8c05e1fe, actor_type30_draw_part(ARG0, (int32_t)ARG1))

/* was func_8C05E5EC: draw the puffs: the puffs' object and motion from the shared tables, the global texlist,
 * njControl3D 0x10 (pushed); each puff not free fading out with its fade: kind 0 the puff object with its motion at
 * its frame, at its position scaled (s, 1, s); kind 1 the sprite lying flat at its position, its size s */
void actor_type30_draw_puffs(gaddr obj) {
  ActorType30 *a = type30_at(obj);
  a->word360 = ((const gaddr *)hw_ptr(g_shared_objects))[g_type30_puff_object];
  a->entry364 = ((const MotionEntry *)hw_ptr(g_shared_motions))[g_type30_puff_motion].motion;
  a->word368 = ((const MotionEntry *)hw_ptr(g_shared_motions))[g_type30_puff_motion].shape;
  njSetTexture(g_effects_texlist);
  control3d_push();
  control3d_set(0x10);
  const int32_t *kind = (const int32_t *)hw_ptr(a->puff_kind);
  const int32_t *state = (const int32_t *)hw_ptr(a->puff_state);
  const f32 *fade = (const f32 *)hw_ptr(a->puff_fade);
  const f32 *scale = (const f32 *)hw_ptr(a->puff_scale);
  gaddr frame = a->puff_frame;
  gaddr pos = a->puff_pos;
  for (int32_t i = 0; i < g_type30_puffs; i++, kind++, state++, fade++, scale++, frame += sizeof(f32),
               pos += sizeof(Vec3)) {
    if (*state == 0) continue;
    material_set_cd7e(F32(1.0f), F32(1.0f), F32(1.0f), fsub(F32(1.0f), fdiv(*fade, g_type30_puff_fade)));
    matrix_push();
    switch (*kind) {
    case 0:
      matrix_translate_v(pos);
      matrix_scale(*scale, F32(1.0f), *scale);
      shape_motion_draw_l3b(a->word360, a->entry364, a->word368, frame, 0);
      break;
    case 1:
      matrix_translate_v(pos);
      matrix_rotate_x(TYPE30_PUFF_TURN);
      a->sprite.sx = *scale;
      a->sprite.sy = *scale;
      njDrawSprite3D(member_addr(obj, ActorType30, sprite), TYPE30_PUFF_CELL, TYPE30_PUFF_ATTR); /* thunk 8C05CDAE */
      break;
    default:
      break;
    }
    matrix_pop();
  }
  control3d_pop();
}
HW_EXPORT(8c05e5ec, actor_type30_draw_puffs(ARG0))

/* the original's frame of the draw (0x94 bytes under PR): its two matrices go to Ninja */
typedef struct DrawFrame30 {
  Matrix current;    /* 0x00 */
  Matrix world;      /* 0x40 */
  int32_t i;         /* 0x80 */
  gaddr node;        /* 0x84 */
  gaddr shape;       /* 0x88 */
  gaddr motion;      /* 0x8C */
  gaddr self;        /* 0x90 */
  uint32_t saved_pr; /* 0x94 */
} DrawFrame30;
HW_OFFSET(DrawFrame30, world, 0x40);
HW_OFFSET(DrawFrame30, node, 0x84);
HW_OFFSET(DrawFrame30, self, 0x90);
HW_SIZE(DrawFrame30, 0x98);

/* a node posed under a pushed matrix, its matrix kept in matrices[k] */
static void type30_keep_node(gaddr obj, DrawFrame30 *f, int32_t k) {
  matrix_push();
  f->node = node_at(f->node)->sibling;
  actor_type30_pose_node(obj, f->node);
  njGetMatrix(member_addr(obj, ActorType30, matrices[k]));
  matrix_pop();
}

/* was func_8C05DD40: the draw method: the puffs; unless hidden: its textures; at its Body's position turned about z,
 * y, x and scaled, its motion set up at its frame (with its shape motion); the model's tree walked by hand: the root
 * and its child drawn, the child's sibling drawn, the five nodes below it posed and their matrices kept (the second's
 * position in the view also as point_b); then the five parts drawn at those matrices */
void actor_type30_draw(gaddr obj) {
  ActorType30 *a = type30_at(obj);
  gaddr frame = hw_stack_alloc(sizeof(DrawFrame30));
  DrawFrame30 *f = (DrawFrame30 *)hw_ptr(frame);
  f->self = obj;
  actor_type30_draw_puffs(obj);
  if (a->hidden == 0) {
    njSetTexture(a->textures);
    matrix_push();
    matrix_translate_v(method_self(member_addr(obj, ActorType30, body)));
    matrix_rotate_z(type30_angle(obj, 2)->value); /* the unit's inline getters (func_8C05CE1A, ...) */
    matrix_rotate_y(type30_angle(obj, 1)->value);
    matrix_rotate_x(type30_angle(obj, 0)->value);
    matrix_scale(type30_scale(obj, 0)->value, type30_scale(obj, 1)->value, type30_scale(obj, 2)->value);
    const MotionEntry *e = &((const MotionEntry *)hw_ptr(a->motions))[a->motion];
    f->motion = e->motion;
    f->shape = e->shape;
    f->node = ((const gaddr *)hw_ptr(a->objects))[0];
    a->mdata = *(const gaddr *)hw_ptr(f->motion); /* NJS_MOTION's mdata */
    nj_MotionSetup(f->motion, a->frame);
    nj_ShapeOnly(f->shape, TYPE30_SHAPE_FLAGS, a->frame);
    /* the root */
    matrix_push();
    actor_type30_pose_node(obj, f->node);
    /* its child, drawn */
    matrix_push();
    f->node = node_at(f->node)->child;
    actor_type30_pose_node(obj, f->node);
    object_draw_model_l1(obj, f->node);
    matrix_pop();
    /* the child's sibling, drawn */
    matrix_push();
    f->node = node_at(f->node)->sibling;
    actor_type30_pose_node(obj, f->node);
    object_draw_model_l1(obj, f->node);
    /* its child: part 1 */
    matrix_push();
    f->node = node_at(f->node)->child;
    actor_type30_pose_node(obj, f->node);
    njGetMatrix(member_addr(obj, ActorType30, matrices[1]));
    matrix_pop();
    /* the next: part 2, and its place in the view as point_b */
    matrix_push();
    f->node = node_at(f->node)->sibling;
    actor_type30_pose_node(obj, f->node);
    njGetMatrix(member_addr(obj, ActorType30, matrices[2]));
    matrix_push();
    gaddr world = member_addr(frame, DrawFrame30, world);
    gaddr current = member_addr(frame, DrawFrame30, current);
    matrix_assign(world, TYPE30_SKY_MATRIX);
    matrix_identity(current); /* the matrix's constructor */
    njGetMatrix(current);
    njMultiMatrix(world, current);
    matrix_load(world);
    vec3_copy(member_addr(obj, ActorType30, point_b), member_addr(frame, DrawFrame30, world.m[12]));
    matrix_pop();
    matrix_pop();
    /* the next three: parts 3, 4 and 0 */
    type30_keep_node(obj, f, 3);
    type30_keep_node(obj, f, 4);
    type30_keep_node(obj, f, 0);
    matrix_pop();
    matrix_pop();
    matrix_pop();
    for (f->i = 0; f->i < TYPE30_PARTS; f->i++) actor_type30_draw_part(obj, f->i);
  }
  hw_stack_free(sizeof(DrawFrame30));
}
HW_EXPORT(8c05dd40, actor_type30_draw(ARG0))
