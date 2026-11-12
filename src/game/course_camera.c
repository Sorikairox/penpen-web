/* Pen Pen port, S15 (c05): the course's camera for a view (func_8C04207E), the "TV" camera that
 * race_scene_course_camera (game/race_scene_methods.c) asks for.
 *
 * Each course has a table of camera zones (CourseCameraZone, 0x14 bytes): the zones follow the course's cells, each
 * up to a cell, with a point the camera stands at. The camera looks at a player's body (the focused view's in
 * modes 1-3, else the course's shown view), stands at the point of the zone that player's cell is in (a zone that is
 * not fixed has a second point in the next record: the one on the player's side of their middle height), and its
 * angle of view narrows from 60 degrees to 10 as the player gets farther from it. */
#include <string.h>
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h" /* method_self */
#include "course.h"
#include "race_scene.h"     /* race_setup_get, RaceSetup */
#include "scenery.h"        /* race_player_body, race_scene_view_focus */
#include "game.h"           /* panel_get */

/* a camera zone (0x14 bytes) */
typedef struct CourseCameraZone {
  f32 last_cell;       /* 0x00: the zone holds the cells up to this one */
  f32 fixed;           /* 0x04: 1.0 = one point; else the point is chosen between this record's and the next's */
  Vec3 eye;            /* 0x08: where the camera stands */
} CourseCameraZone;
HW_OFFSET(CourseCameraZone, eye, 0x08);
HW_SIZE(CourseCameraZone, 0x14);

#define g_course_camera_zones ((const gaddr *)hw_ptr(0x8C096F64u)) /* CourseCameraZone[] per course */

#define CAMERA_FOV_NEAR      F32_BITS(0x462AAAAAu) /* 10922.666: 60 degrees (Ninja angle units) */
#define CAMERA_FOV_FAR       F32_BITS(0x44E38E38u) /* 1820.4445: 10 degrees */
#define CAMERA_NEAR_DISTANCE F32(10.0f)
#define CAMERA_FAR_DISTANCE  F32(300.0f)
#define CAMERA_FOV_SLOPE     F32_BITS(0x41FB1884u) /* 31.387: (60 - 10 degrees) / (300 - 10) */

/* the original's frame (0x3C bytes under PR): the player's position, which course_cell_search reads */
typedef struct CourseCameraFrame {
  uint8_t pad_00[0x14]; /* 0x00: scratch the C keeps in variables */
  Vec3 target;          /* 0x14 */
  uint8_t rest[0x40 - 0x20]; /* 0x20: the arguments it keeps, PR */
} CourseCameraFrame;
HW_OFFSET(CourseCameraFrame, target, 0x14);
HW_SIZE(CourseCameraFrame, 0x40);

/* was func_8C04207E: the camera for the course's view: *eye (vec3_set), *target (the player's position), *roll = 0,
 * *fov (Ninja angle units) */
void course_tv_camera(gaddr course, gaddr eye, gaddr target, gaddr roll, gaddr fov) {
  Course *c = course_at(course);
  gaddr scene = panel_get();
  const CourseCameraZone *zone = (const CourseCameraZone *)hw_ptr(g_course_camera_zones[c->course]);
  gaddr locals = hw_stack_alloc(sizeof(CourseCameraFrame));
  gaddr pos = member_addr(locals, CourseCameraFrame, target);
  method_self(pos); /* the local's (empty) constructor */
  int32_t mode = race_setup_at(race_setup_get())->mode;
  gaddr body;
  if (mode == 1 || mode == 2 || mode == 3)
    body = race_player_body(scene, race_scene_view_focus(scene, 0));
  else
    body = race_player_body(scene, c->shown_view);
  memmove(hw_ptr(pos), hw_ptr(body), sizeof(Vec3));
  int32_t cell = course_cell_search(course, pos);
  while (fgt(f32_from_int(cell), zone->last_cell))
    zone++;
  if (feq(zone->fixed, F32(1.0f))) {
    vec3_set(zone->eye.x, zone->eye.y, zone->eye.z, eye);
  } else {
    const CourseCameraZone *next = zone + 1;
    f32 middle = fadd(fdiv(fsub(next->eye.y, zone->eye.y), F32(2.0f)), zone->eye.y);
    if (fgt(vec3_at(pos)->y, middle))
      vec3_set(zone->eye.x, zone->eye.y, zone->eye.z, eye);
    else
      vec3_set(next->eye.x, next->eye.y, next->eye.z, eye);
  }
  memmove(hw_ptr(target), hw_ptr(pos), sizeof(Vec3));
  *(f32 *)hw_ptr(roll) = F32(0.0f);
  f32 distance = vec3_distance(eye, pos);
  f32 angle = CAMERA_FOV_NEAR;
  if (fgt(distance, CAMERA_NEAR_DISTANCE)) {
    if (fgt(CAMERA_FAR_DISTANCE, distance))
      angle = fsub(CAMERA_FOV_NEAR, fmul(fadd(distance, F32(-10.0f)), CAMERA_FOV_SLOPE));
    else
      angle = CAMERA_FOV_FAR;
  }
  *(f32 *)hw_ptr(fov) = angle;
  hw_stack_free(sizeof(CourseCameraFrame));
}
HW_EXPORT(8c04207e, course_tv_camera(ARG0, ARG1, ARG2, ARG3, ARG4))
