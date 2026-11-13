/* Pen Pen port, S16 (w3): a racer put back at the start (game/racer_reset.c).
 *
 * The racer (game/racer.h) loses its controls, marks, ground and action words, is placed and turned
 * (racer_reset_at), its collider set up from its character's parameters, its state set to 0, its frame, camera and
 * ribbon reset; players 0-3 of the set-up's mode get their controller's vibration unit (its port, whether it is
 * ready, a stop time). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "racer.h"
#include "effects.h"
#include "race_scene.h"

/* functions of other files that no header declares yet */
void frame_body_set_matrices(gaddr obj, gaddr world, gaddr local); /* game/racer.c */
void racer_camera_reset(gaddr racer);                              /* game/racer.c */

/* the vibration units (SDK): pdVibMxIsReady (called through its thunk at 0x8C0A10F4) and pdVibMxSetStopTime */
static inline int32_t pdVibMxIsReady(int32_t port) { return (int32_t)CALL1(0x8C0A10F4u, port); }
static inline void pdVibMxSetStopTime(int32_t port, int32_t time) { CALL2(0x8C0A10FAu, port, time); }

#define CONTACT_RESPOND 0x8C014E88u /* contact_respond (core/contact.c), the collider's response */
#define VIB_NONE        0xFF
#define VIB_STOP_TIME   20
#define CONTROLLED_PLAYERS 4

/* was func_8C030F00: the racer back at `position`, turned to `heading`; `word` kept (word0fc) by players 0-3 */
void racer_reset(gaddr racer, gaddr position, f32 heading, uint32_t word) {
  Racer *r = racer_at(racer);
  if (r->index < CONTROLLED_PLAYERS)
    r->word0fc = word;
  else
    r->word0fc = 0;
  r->word1a4 = 0;
  r->word1a8 = 0;
  r->word1ac = 0;
  /* the controls */
  r->stick_x = F32(0.0f);
  r->stick_y = F32(0.0f);
  r->edges[0].released = 0;
  r->button[0] = 0;
  r->edges[0].pressed = 0;
  r->button_prev[0] = 0;
  r->edges[1].released = 0;
  r->button[1] = 0;
  r->edges[1].pressed = 0;
  r->button_prev[1] = 0;
  r->stick_length = F32(0.0f);
  r->stick_angle = F32(0.0f);
  r->button_lock[1] = 0;
  r->button_lock[0] = 0;
  r->on_kind4_prev = 0;
  r->on_kind4 = 0;
  r->on_kind4_new = 0;
  r->stroke_level = 0;
  r->hold_frames = 0;
  r->last_hold = 0;
  r->release_frames = 0;
  r->word144 = 1;
  /* the marks and the ground words */
  for (int32_t i = 0; i < 5; i++)
    r->marks[i] = 0;
  for (int32_t i = 0; i < 5; i++)
    r->marks_prev[i] = 0;
  r->word170 = 0;
  r->word174 = 0;
  r->word178 = 0;
  r->word17c = 0;
  r->word180 = 0;
  r->stick_damped = 0;
  r->f188 = F32(0.0f);
  r->word190 = 0;

  racer_reset_at(racer, position, F32(0.0f), heading, F32(0.0f));
  r->f434 = F32(0.0f);
  vec3_set(F32(0.0f), F32(-1.0f), F32(0.0f), member_addr(racer, Racer, vec428));
  /* the collider: no cell yet, the moving body's velocity, the character's radius and mass */
  const CharacterParams *params = &((const CharacterParams *)hw_ptr(CHARACTER_PARAMS))[r->character];
  r->contacts.cell = -1;
  r->contacts.owner_velocity = member_addr(racer, Racer, body.body.velocity);
  r->contacts.radius = params->radius;
  r->contacts.mass = params->mass;
  r->contacts.unknown18 = F32(1.0f);
  r->contacts.unknown1c = 0;
  r->contacts.unknown20 = 0;
  r->contacts.respond = CONTACT_RESPOND;
  r->contacts.count = 0;
  r->contacts_word = 4;
  r->collider_word = -1;
  r->word210 = 3;
  r->word198 = 0;
  racer_set_state(racer, 0);
  r->ground_prev = 0;
  r->ground = 0;
  vec3_set(F32(0.0f), F32(1.0f), F32(0.0f), member_addr(racer, Racer, vec240));
  r->ground_touch_prev = 0;
  r->ground_touch = 0;
  r->word254 = 0;
  r->mark_a_on = 0;
  r->mark_b_on = 0;
  r->mark_a_value = F32(0.0f);
  r->mark_b_value = F32(0.0f);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(racer, Racer, vec260));
  r->hit_strength = 0;
  r->step_timer = -1;
  frame_body_set_matrices(member_addr(racer, Racer, body), member_addr(racer, Racer, matrix3a8),
                          member_addr(racer, Racer, matrix3e8));
  racer_camera_reset(racer);
  for (int32_t i = 0; i < 4; i++)
    r->times298[i] = F32(0.0f);
  r->word554 = 0;
  r->word530 = 0; /* (stored as the float 0) */
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(racer, Racer, vec534));
  vec3_set(F32(0.0f), F32(1.0f), F32(0.0f), member_addr(racer, Racer, vec540));
  racer_effects_reset_ribbon(r->child, r->contacts.radius, member_addr(racer, Racer, matrix3a8));
  r->word558 = 0;
  r->word55c = 0;
  r->word560 = 0;
  r->word564 = 0;
  r->effects_shown = 0;
  r->vib_ready = 0;
  r->vib_port = VIB_NONE;
  r->variant_word = 0;
  r->variant = 0;

  /* the vibration unit: players below the mode's count, the port's first expansion slot */
  int32_t vib_port = VIB_NONE;
  int32_t players = 0;
  switch (race_setup_at(g_race_setup)->mode) {
  case 0:
  case 4: players = 1; break;
  case 1: players = 2; break;
  case 2: players = 3; break;
  case 3: players = 4; break;
  default: break;
  }
  if (r->index < players) {
    switch (r->index) {
    case 0: vib_port = 2; break;
    case 1: vib_port = 8; break;
    case 2: vib_port = 14; break;
    case 3: vib_port = 20; break;
    default: break;
    }
  }
  if (vib_port == VIB_NONE) {
    r->vib_ready = 0;
  } else {
    r->vib_port = vib_port;
    r->vib_ready = pdVibMxIsReady(r->vib_port);
    pdVibMxSetStopTime(r->vib_port, VIB_STOP_TIME);
  }
}
HW_EXPORT(8c030f00, racer_reset(ARG0, ARG1, FARG0, ARG2))
