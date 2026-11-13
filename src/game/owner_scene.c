/* Pen Pen port, S17 b04: the owner scene (game/owner_scene.h): its update, the owner's motion cycle and draw, its
 * release and destructors, its penguins' constructor; and the inline copies of this unit (a Body's reset, a
 * Body's force.z; the RaceSetup's words at 0x3C / 0x40 are exported in src/copies).
 *
 * The update: state 0 starts the course's music cue (0x2B + course for courses 0-3), sets a timer of 540 frames
 * and resets the owner's motion; state 1 waits for button 4 or 8 of its pad or the timer, then gives state 2 60
 * frames; state 2 then asks the set-up for its next scene: 4 when the race set-up's word at 0x40 or 0x3C is set,
 * else 0. Every update the owner's motion runs and the penguins are updated. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/ninja.h"
#include "../core/pad.h"
#include "../core/file.h"
#include "sound.h"
#include "motion.h"
#include "menu.h"
#include "race_scene.h"
#include "owner_scene.h"

#define OWNER_PENGUIN_PART 0xDCu /* the penguin's first word points at its part at 0xDC (as the menus' penguins) */
#define OWNER_SHOW_FRAMES 540
#define OWNER_LEAVE_FRAMES 60
#define OWNER_MUSIC_FIRST 0x2B   /* the music cue of course 0 (1-3 follow) */

/* ---- inline copies of this unit ---- */

/* was func_8C02F53E: a copy of body_reset (core/vec.c) */
HW_EXPORT(8c02f53e, body_reset(ARG0))

/* was func_8C030750: set a Body's force.z (an inline method; the racer's swimming clears it) */
void body_set_force_z(gaddr body, f32 value) { body_at(body)->force.z = value; }
HW_EXPORT(8c030750, body_set_force_z(ARG0, FARG0))

/* ---- the penguins ---- */

/* was func_8C02F73A: a penguin of the scene: its first word points at its part at 0xDC, then the MotionPlayer's
 * construction (character, variant, container; no table) */
gaddr owner_penguin_construct(gaddr penguin, int32_t character, int32_t variant, gaddr container) {
  motion_player_at(penguin)->unknown00 = penguin + OWNER_PENGUIN_PART;
  motion_player_build(penguin, character, variant, container, 0);
  return penguin;
}
HW_EXPORT(8c02f73a, RET(owner_penguin_construct(ARG0, (int32_t)ARG1, (int32_t)ARG2, ARG3)))

/* ---- the owner ---- */

/* was func_8C030138: the owner's motion from the start (pair 0, frame 0, not yet played), its Body at the origin
 * damped by 0.75, scale 1 */
void owner_scene_reset_owner(gaddr scene) {
  OwnerScene *s = owner_scene_at(scene);
  gaddr body = member_addr(scene, OwnerScene, body);
  s->pair = 0;
  s->pair_prev = 0;
  s->frame = F32(0.0f);
  s->fb8 = F32(0.0f);
  body_reset(body);
  vec3_fill(member_addr(body, Body, damping), F32(0.75f));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), body);
  s->scale = F32(1.0f);
  s->played = 0;
  s->pair = 0;
  s->pair_prev = s->pair;
  s->frame = F32(0.0f);
  s->fb8 = F32(0.0f);
}
HW_EXPORT(8c030138, owner_scene_reset_owner(ARG0))

/* was func_8C0301B4: one frame of the owner's motion; past its last frame it starts over with the next pair (0 to
 * 3 in turn; course 2 alternates 0 and 2, and stays on 1 or 3) */
void owner_scene_animate(gaddr scene) {
  OwnerScene *s = owner_scene_at(scene);
  s->frame = fadd(s->frame, F32(1.0f));
  const OwnerPair *pair = &((const OwnerPair *)hw_ptr(s->pairs))[s->pair];
  s->played = 1;
  const NjMotion *motion = (const NjMotion *)hw_ptr(pair->motion);
  if (!fgt(s->frame, f32_from_uint((uint32_t)(motion->nb_frames - 1)))) return;
  s->frame = F32(0.0f);
  if (s->course == 2) {
    if (s->pair == 0)
      s->pair = 2;
    else if (s->pair == 2)
      s->pair = 0;
  } else {
    if (++s->pair > 3) s->pair = 0;
  }
}
HW_EXPORT(8c0301b4, owner_scene_animate(ARG0))

/* was func_8C030244: draw the owner: the file's textures, at its Body's position, scaled, its current pair at its
 * frame */
void owner_scene_draw_owner(gaddr scene) {
  OwnerScene *s = owner_scene_at(scene);
  njSetTexture(s->texlist);
  matrix_push();
  matrix_translate_v(method_self(member_addr(scene, OwnerScene, body)));
  matrix_scale_all(s->scale);
  shape_motion_pair_draw(s->object, s->pairs + (uint32_t)s->pair * sizeof(OwnerPair),
                         member_addr(scene, OwnerScene, frame), s->played);
  matrix_pop();
}
HW_EXPORT(8c030244, owner_scene_draw_owner(ARG0))

/* ---- the scene ---- */

/* was func_8C02FD06: the scene's update (see the top of the file) */
void owner_scene_update(gaddr scene) {
  OwnerScene *s = owner_scene_at(scene);
  switch (s->state) {
  case 0:
    switch (s->course) {
    case 0: music_play(OWNER_MUSIC_FIRST, 0, 1); break;
    case 1: music_play(OWNER_MUSIC_FIRST + 1, 0, 1); break;
    case 2: music_play(OWNER_MUSIC_FIRST + 2, 0, 1); break;
    case 3: music_play(OWNER_MUSIC_FIRST + 3, 0, 1); break;
    }
    s->timer = OWNER_SHOW_FRAMES;
    field_set_and_clear(scene, 1);
    owner_scene_reset_owner(scene);
    break;
  case 1:
    if (pad_pressed(&s->pad, 4) || pad_pressed(&s->pad, 8)) {
      s->timer = OWNER_LEAVE_FRAMES;
      field_set_and_clear(scene, 2);
    } else if (--s->timer < 0) {
      s->timer = OWNER_LEAVE_FRAMES;
      field_set_and_clear(scene, 2);
    }
    break;
  case 2:
    if (--s->timer < 0) {
      if (race_setup_word40(race_setup_get()) != 0)
        race_setup_at(s->setup)->requested = 4;
      else
        race_setup_at(s->setup)->requested = race_setup_word3c(race_setup_get()) != 0 ? 4 : 0;
    }
    break;
  }
  owner_scene_animate(scene);
  for (int32_t i = 0; i < s->penguin_count; i++)
    motion_player_update(s->penguins[i]);
}
HW_EXPORT(8c02fd06, owner_scene_update(ARG0))

/* was func_8C02FC28: release what the scene holds: the owner file's textures and the file, the penguins (deleted
 * with their memory), the other texture list; stop the music and wait for the drive */
void owner_scene_release(gaddr scene) {
  OwnerScene *s = owner_scene_at(scene);
  njReleaseTexture(s->texlist);
  file_free(s->file);
  for (int32_t i = 0; i < s->penguin_count; i++) {
    gaddr penguin = s->penguins[i];
    if (penguin != 0) motion_player_delete(penguin, 1);
  }
  njReleaseTexture(s->texlist64);
  music_stop();
  cd_wait_stopped();
  method_empty(); /* (func_8C02F4CA on the scene) */
}
HW_EXPORT(8c02fc28, owner_scene_release(ARG0))

/* was func_8C02FCD0: the scene's deleting destructor: the release, then the memory when asked */
gaddr owner_scene_delete(gaddr scene, int32_t free_memory) {
  owner_scene_release(scene);
  if (free_memory) syFree(scene);
  return scene;
}
HW_EXPORT(8c02fcd0, RET(owner_scene_delete(ARG0, (int32_t)ARG1)))

/* was func_8C02FCF4: the release, returning the scene */
gaddr owner_scene_destruct(gaddr scene) {
  owner_scene_release(scene);
  return scene;
}
HW_EXPORT(8c02fcf4, RET(owner_scene_destruct(ARG0)))
