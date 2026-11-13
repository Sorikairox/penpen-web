/* Pen Pen port, S16 (b04): the racer and the course's ground and walls (func_8C03207C, the update step after
 * racer_sort_contacts).
 *
 * First the racer's own contacts tell whether it touches special ground (ice 0xFF / 0xF9: ground 3 / 4, 0xF4:
 * ground 2; vec240 sums their normals). Then the race scene collides its collider with the course
 * (func_8C030A5C), and each course contact, by the attribute of the face it touched, sets the ground kind (the
 * highest wins), marks a bump (mark_b: the velocity along the normal, in vec260), bounces it back (0xCC, with a
 * sound), or pushes it out of a wall (0xE0 / 0xE5: once each, vec534 / vec540). Most contacts then push the body
 * out by their depth and take away the velocity into the face (times a bounce factor, 1.25 more in state 5);
 * mark_a_value keeps the strongest such hit. On special ground the heading turns with how the ground's normal
 * turned; elsewhere vec240 is straight up. After a wall push (either kind) the ground is at least 4; else the
 * racer is pulled back along vec240 by its radius. Last the edge flags: word254 (left special ground), mark_a_on (came
 * onto it), word178 (word174 newly set). */
#include <string.h>
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "racer.h"
#include "race_scene.h"   /* race_sound_play_by_mode */

/* the attribute words of the faces the racer can touch (Contacts.unknown48) */
#define FACE_ICE          0xFF  /* special ground 3 */
#define FACE_ICE_B        0xF9  /* special ground 4 */
#define FACE_ICE_C        0xF4  /* special ground 2 (a wall of kind 0xF4 also sets word180) */
#define FACE_BUMP         0xEA
#define FACE_SLOPE_A      0xDB  /* with 0xD6, 0xD1, 0xFF, 0xF9: ground 3, or a bump on steep faces in state 3 */
#define FACE_SLOPE_B      0xD6
#define FACE_SLOPE_C      0xD1
#define FACE_BOUNCE       0xCC  /* throws the racer back, with sound 0x2A */
#define FACE_BOOST        0xEF  /* a bump that bounces 1.25 times: ground 5 */
#define FACE_WALL_A       0xE0  /* pushes it out once (word174) */
#define FACE_WALL_B       0xE5  /* the same (word17c) */

#define STEEP_Y           F32_BITS(0x3F333333u) /* 0.7: a face whose normal's y is below it is steep */
#define BOUNCE_BACK_PUSH  F32_BITS(0xC04097B3u) /* -3.0092595: the extra throw along the normal */
#define BOUNCE_BACK       F32_BITS(0x3FF99999u) /* 1.9499999 */
#define BOUNCE_BOOST      F32(1.25f)
#define SOUND_BOUNCE      0x2A

/* func_8C030A5C (game/race_hud.c, no header): the race scene's course collides an object */
static void race_scene_course_update(gaddr scene, gaddr object) { CALL2(0x8C030A5Cu, scene, object); }
/* func_8C062898 (C runtime, float): asinf by its use (game/transform.c calls it so too) */
static f32 rt_asinf(f32 x) {
  FARGS1(x);
  CALL0(0x8C062898u);
  return hw_fret();
}

/* the original's frame (0x4C bytes under r8 and PR) */
typedef struct GroundFrame {
  union {
    struct {
      Vec3 push;         /* 0x00: a push out of a wall / the bounce back */
      f32 into;          /* 0x0C: the velocity into the face, times the bounce */
    };
    struct {
      f32 sine;          /* 0x00: the turn's sine (the original's slot; kept in a variable here) */
      Vec3 now;          /* 0x04: vec240 now, for the turn */
    };
  };
  Vec3 out;              /* 0x10: the push out by the depth, then the velocity taken away */
  f32 bounce;            /* 0x1C */
  uint8_t pad_20[0x38 - 0x20]; /* 0x20: the walking pointers, the counters */
  Vec3 old240;           /* 0x38: vec240 of the previous update */
  uint8_t rest[0x54 - 0x44]; /* 0x44: the contacts, the racer, r8, PR */
} GroundFrame;
HW_OFFSET(GroundFrame, now, 0x04);
HW_OFFSET(GroundFrame, into, 0x0C);
HW_OFFSET(GroundFrame, out, 0x10);
HW_OFFSET(GroundFrame, old240, 0x38);
HW_SIZE(GroundFrame, 0x54);

static void ground_at_least(Racer *r, int32_t kind) {
  if (r->ground < kind)
    r->ground = kind;
}

/* the bump: mark_b, vec260 = the normal times the velocity along it, ground at least 1 */
static void racer_bump(gaddr racer, gaddr normal) {
  Racer *r = racer_at(racer);
  r->mark_b_on = 1;
  f32 along = njInnerProduct(normal, member_addr(racer, Racer, body.body.velocity));
  vec3_scaled(along, member_addr(racer, Racer, vec260), normal);
  ground_at_least(r, 1);
}

/* special ground under the racer: ground_touch, the normal summed into vec240, ground at least `kind` */
static void racer_on_special(gaddr racer, gaddr normal, int32_t kind) {
  Racer *r = racer_at(racer);
  r->ground_touch = 1;
  vec3_add(member_addr(racer, Racer, vec240), normal);
  ground_at_least(r, kind);
}

/* a wall's push out (FACE_WALL_A / B): the normal times (depth - radius) into vec534, the normal into vec540 */
static void racer_wall_push(gaddr racer, gaddr frame, gaddr normal, f32 depth) {
  Racer *r = racer_at(racer);
  gaddr push = member_addr(frame, GroundFrame, push);
  f32 radius = character_params_at(r->params)->radius;
  vec3_scaled(fsub(depth, radius), push, normal);
  vec3_add(member_addr(racer, Racer, vec534), push);
  memmove(&r->vec540, hw_ptr(normal), sizeof(Vec3));
}

/* was func_8C03207C: the racer's ground and walls of this update (see the top of the file) */
void racer_ground(gaddr racer) {
  Racer *r = racer_at(racer);
  gaddr frame = hw_stack_alloc(sizeof(GroundFrame));
  GroundFrame *f = (GroundFrame *)hw_ptr(frame);
  gaddr contacts = member_addr(racer, Racer, contacts);
  gaddr velocity = member_addr(racer, Racer, body.body.velocity);

  r->ground_touch_prev = r->ground_touch;
  r->ground_prev = r->ground;
  vec3_copy(member_addr(frame, GroundFrame, old240), member_addr(racer, Racer, vec240));
  r->word170 = r->word174;
  r->ground_touch = 0;
  r->mark_b_on = 0;
  r->ground = 0;
  vec3_zero(member_addr(racer, Racer, vec240));
  r->mark_a_value = F32(0.0f);
  r->mark_b_value = F32(0.0f);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(racer, Racer, vec260));
  r->word174 = 0;
  r->word178 = 0;
  r->word17c = 0;
  r->word180 = 0;
  vec3_zero(member_addr(racer, Racer, vec534));

  /* the racer's own contacts (other racers and objects) on special ground */
  for (int32_t i = 0; i < contacts_at(contacts)->count; i++) {
    const Contacts *c = contacts_at(contacts);
    int32_t kind = ((const int32_t *)hw_ptr(c->unknown48))[i];
    gaddr normal = c->normal + (uint32_t)i * sizeof(Vec3);
    switch (kind) {
    case FACE_ICE:   r->ground = 3; break;
    case FACE_ICE_B: r->ground = 4; break;
    case FACE_ICE_C: r->ground = 2; break;
    default: continue;
    }
    r->ground_touch = 1;
    vec3_add(member_addr(racer, Racer, vec240), normal);
  }

  /* the course's faces */
  race_scene_course_update(r->scene, member_addr(racer, Racer, collider_data));
  r->word174 = 0;
  r->word17c = 0;
  r->word180 = 0;
  const Contacts *c = contacts_at(contacts);
  int32_t n = c->count;
  gaddr normal = c->normal;
  gaddr depth = c->depth;
  gaddr kinds = c->unknown48;
  f->bounce = F32(1.0f);
  for (; n > 0; n--, normal += sizeof(Vec3), depth += sizeof(f32), kinds += sizeof(int32_t)) {
    int32_t kind = *(const int32_t *)hw_ptr(kinds);
    switch (kind) {
    case FACE_BUMP:
      f->bounce = F32(1.0f);
      racer_bump(racer, normal);
      break;
    case FACE_SLOPE_A:
    case FACE_SLOPE_B:
    case FACE_SLOPE_C:
    case FACE_ICE:
    case FACE_ICE_B:
      f->bounce = F32(1.0f);
      if (fgt(STEEP_Y, vec3_at(normal)->y) && r->state == 3)
        racer_bump(racer, normal);
      else
        racer_on_special(racer, normal, 3);
      break;
    case FACE_BOUNCE: {
      gaddr push = member_addr(frame, GroundFrame, push);
      f32 along = njInnerProduct(velocity, normal);
      vec3_scaled(fneg(along), push, normal);
      vec3_add(velocity, push);
      vec3_scaled(BOUNCE_BACK_PUSH, push, normal);
      vec3_add(velocity, push);
      f->bounce = BOUNCE_BACK;
      ground_at_least(r, 1);
      race_sound_play_by_mode(r->scene, r->index, 0, 11, SOUND_BOUNCE, 0, 0);
      break;
    }
    case FACE_ICE_C:
      f->bounce = F32(1.0f);
      if (fgt(STEEP_Y, vec3_at(normal)->y) && r->state == 3)
        racer_bump(racer, normal);
      else
        racer_on_special(racer, normal, 2);
      r->word180 = 1;
      break;
    case FACE_BOOST:
      f->bounce = BOUNCE_BOOST;
      r->mark_b_on = 1;
      vec3_scaled(njInnerProduct(normal, velocity), member_addr(racer, Racer, vec260), normal);
      ground_at_least(r, 5);
      break;
    case FACE_WALL_A: /* no push out by the depth for the walls */
      if (r->word174 == 0) {
        r->word174 = 1;
        racer_wall_push(racer, frame, normal, *(const f32 *)hw_ptr(depth));
      }
      continue;
    case FACE_WALL_B:
      if (r->word17c == 0) {
        r->word17c = 1;
        racer_wall_push(racer, frame, normal, *(const f32 *)hw_ptr(depth));
      }
      continue;
    default:
      break;
    }
    /* out of the face by the depth; the velocity into it taken away, times the bounce */
    if (r->state == 5)
      f->bounce = fmul(f->bounce, BOUNCE_BOOST);
    gaddr out = member_addr(frame, GroundFrame, out);
    vec3_scaled(*(const f32 *)hw_ptr(depth), out, normal);
    vec3_add(member_addr(racer, Racer, body), out);
    f->into = fmul(njInnerProduct(normal, velocity), f->bounce);
    if (fgt(F32(0.0f), f->into)) {
      vec3_scaled(fneg(f->into), out, normal);
      vec3_add(velocity, out);
      f32 hit = njScalor(out);
      if (fgt(hit, r->mark_a_value))
        r->mark_a_value = hit;
    }
  }

  if (r->ground_touch != 0) {
    /* the heading turns by the angle the ground's normal turned (about y) */
    njUnitVector(member_addr(racer, Racer, vec240));
    gaddr was = member_addr(frame, GroundFrame, out);
    gaddr now = member_addr(frame, GroundFrame, now);
    vec3_copy(was, member_addr(frame, GroundFrame, old240));
    vec3_copy(now, member_addr(racer, Racer, vec240));
    f32 radians = rt_asinf(vec3_cross_y(was, now));
    f32 turn = f32_from_int(f32_to_int(fdiv(fmul(radians, F32(65536.0f)), F32_BITS(0x40C90FD7u) /* 2 pi */)));
    f32_add_to(fmul(turn, F32(1.0f)), member_addr(racer, Racer, heading.value));
  } else {
    vec3_set(F32(0.0f), F32(1.0f), F32(0.0f), member_addr(racer, Racer, vec240));
  }
  if (r->word174 != 0 || r->word17c != 0) { /* (S16: the merge's difftest replay found word17c read as ground) */
    ground_at_least(r, 4);
  } else {
    /* no ground and no wall: pulled back along vec240 by the racer's radius */
    gaddr out = member_addr(frame, GroundFrame, out);
    vec3_scaled(character_params_at(r->params)->radius, out, member_addr(racer, Racer, vec240));
    vec3_sub(member_addr(racer, Racer, vec534), out);
    memmove(&r->vec540, &r->vec240, sizeof(Vec3));
  }
  r->mark_b_value = njScalor(member_addr(racer, Racer, vec260));
  r->word254 = (r->ground_touch == 0 && r->ground_touch_prev != 0) ? 1 : 0;
  r->mark_a_on = (r->ground_touch != 0 && r->ground_touch_prev == 0) ? 1 : 0;
  r->word178 = (r->word174 & (r->word170 == 0)) != 0 ? 1 : 0;
  hw_stack_free(sizeof(GroundFrame));
}
HW_EXPORT(8c03207c, racer_ground(ARG0))
