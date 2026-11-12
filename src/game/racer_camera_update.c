/* Pen Pen port, S15 (b06): the racer's camera update (func_8C03C69E) and the camera of its state 0
 * (func_8C03C804); the other modes are in game/racer_camera.c.
 *
 * The update runs the camera mode of the racer's state, then (for the states that follow the race: 3, 4, 5 and
 * any other) keeps the eye out of the course: the eye's Contacts (Racer.contacts2, radius 2.5, owned by the eye's
 * position) are tested against the course around it, and the eye is pushed out along each contact's normal by its
 * depth, its velocity losing the part along that normal. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/contact.h"
#include "game.h"
#include "course.h"
#include "race_scene.h"
#include "racer.h"

/* 0x8C0C4A80 (Ninja, named by port/symbols/sdk_calls.tsv): makes the camera's point from a vector and t (the
 * same call as racer_camera.c's) */
static inline void njProjectScreen(gaddr v, gaddr out, uint32_t flag, f32 t) {
  FARGS1(t);
  CALL3(0x8C0C4A80u, v, out, flag);
}

/* the vectors the state-0 camera's point is made from (static data) */


/* func_8C03C804 through the original: the C version (game/racer_camera_start.c) fails the difftest (S15: two
 * writers' versions, the same differences in the eye's position at the races' first frame; to find) */
static void racer_camera_start_translated(gaddr racer) { CALL1(0x8C03C804u, racer); }

/* the camera modes still translated */
/* func_8C03C99C: the camera of state 1 */
static void racer_camera_state1(gaddr racer) { CALL1(0x8C03C99Cu, racer); }
/* func_8C03D2C4: the camera of state 5 */
static void racer_camera_state5(gaddr racer) { CALL1(0x8C03D2C4u, racer); }
/* func_8C041928 (game/course_ground.c, S15 b07): the Contacts tested against the course around *cell */
static void course_contacts_update_(gaddr course, gaddr contacts, gaddr cell) {
  CALL3(0x8C041928u, course, contacts, cell);
}

#define CAMERA_EYE_RADIUS F32(2.5f)

/* the update's frame (0x14 bytes under six registers and PR: 0x30 in all) */
typedef struct CameraUpdateFrame {
  gaddr scratch;      /* 0x00 */
  int32_t cell;       /* 0x04: the eye's cell hint (-1: search) */
  Vec3 push;          /* 0x08 */
  uint8_t saved[0x30 - 0x14]; /* 0x14: r9-r14, PR */
} CameraUpdateFrame;
HW_OFFSET(CameraUpdateFrame, cell, 0x04);
HW_OFFSET(CameraUpdateFrame, push, 0x08);
HW_SIZE(CameraUpdateFrame, 0x30);

/* was func_8C03C69E: the racer's camera update (nothing while the racer is shown on no view): the mode of its
 * state; for the states after the intro, the eye kept out of the course */
void racer_camera_update(gaddr racer) {
  Racer *r = racer_at(racer);
  if (race_scene_at(r->scene)->player_view[r->index] == -1)
    return;
  switch (r->state) {
  case 0:
    racer_camera_start_translated(racer);
    return;
  case 2:
    racer_camera_intro(racer);
    return;
  case 1:
    racer_camera_state1(racer);
    return;
  case 3:
    racer_camera_follow(racer);
    break;
  case 4:
    racer_camera_high(racer);
    break;
  case 5:
    racer_camera_state5(racer);
    break;
  default:
    break;
  }
  gaddr frame = hw_stack_alloc(sizeof(CameraUpdateFrame));
  CameraUpdateFrame *f = (CameraUpdateFrame *)hw_ptr(frame);
  gaddr eye = member_addr(racer, Racer, camera_eye);
  gaddr eye_velocity = member_addr(eye, Body, velocity);
  gaddr push = member_addr(frame, CameraUpdateFrame, push);
  f->cell = -1;
  r->contacts2.radius = CAMERA_EYE_RADIUS;
  r->contacts2.owner = eye;
  course_contacts_update_(race_scene_at(r->scene)->course, member_addr(racer, Racer, contacts2),
                          member_addr(frame, CameraUpdateFrame, cell));
  int32_t n = r->contacts2.count;
  gaddr normal = r->contacts2.normal;
  const f32 *depth = (const f32 *)hw_ptr(r->contacts2.depth);
  for (; n > 0; n--, normal += sizeof(Vec3), depth++) {
    const Vec3 *nv = (const Vec3 *)hw_ptr(normal);
    f32 d = *depth;
    f->push.x = fmul(nv->x, d);
    f->push.y = fmul(nv->y, d);
    f->push.z = fmul(nv->z, d);
    vec3_add(eye, push);
    f32 along = fneg(njInnerProduct(normal, eye_velocity));
    f->push.x = fmul(nv->x, along);
    f->push.y = fmul(nv->y, along);
    f->push.z = fmul(nv->z, along);
    vec3_add(eye_velocity, push);
  }
  hw_stack_free(sizeof(CameraUpdateFrame));
}
HW_EXPORT(8c03c69e, racer_camera_update(ARG0))
