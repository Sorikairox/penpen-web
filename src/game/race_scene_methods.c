/* Pen Pen port, S15 (b03): small methods of the RaceScene's unit (game/race_scene.h, game/course.h):
 * the deleting destructor, the course's value at its start segment, and the scene's thunk to the course's camera.
 * game/race_scene_methods.c */
#include "hw.h"
#include "sdk.h"
#include "race_scene.h"
#include "course.h"

void race_scene_destroy(gaddr scene); /* game/race_scene.c */

/* func_8C04207E (game/course.c, still translated): the course's camera for a view: three in-out arguments and a
 * float written to *out (its fourth, a fifth argument on the stack) */
static void course_camera(gaddr course, uint32_t a, uint32_t b, uint32_t c, gaddr out) {
  CALL5(0x8C04207Eu, course, a, b, c, out);
}

/* was func_8C029108: the course's value (course_value) at its start segment */
f32 course_start_value(gaddr course) { return course_value(course, course_at(course)->start_segment); }
HW_EXPORT(8c029108, RETF(course_start_value(ARG0)))

/* was func_8C029290: the scene's course's camera (func_8C04207E with the scene's other arguments) */
void race_scene_course_camera(gaddr scene, uint32_t a, uint32_t b, uint32_t c, gaddr out) {
  course_camera(race_scene_at(scene)->course, a, b, c, out);
}
HW_EXPORT(8c029290, race_scene_course_camera(ARG0, ARG1, ARG2, ARG3, ARG4))

/* was func_8C029884: the RaceScene's deleting destructor: its body (race_scene_destroy), then its memory freed when
 * asked; returns the scene */
gaddr race_scene_delete(gaddr scene, int32_t free_memory) {
  race_scene_destroy(scene);
  if (free_memory)
    syFree(scene);
  return scene;
}
HW_EXPORT(8c029884, RET(race_scene_delete(ARG0, (int32_t)ARG1)))
