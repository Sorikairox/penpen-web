/* Pen Pen port, S15 b07: a player's CourseEnv (game/course.h): its set-up from the course's four tracks, its
 * per-frame update and draw: the fog and background colour (eased toward the fog track's keys, or a fixed preset
 * per course), the light rig, the weather particles (placed at random around the camera), the sky model and the
 * course's ambient sounds by cell. The fog / light / particle helpers it calls are in game/camera.c and
 * game/actor.c. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/view.h"
#include "../core/ninja.h"
#include "../core/inline.h"
#include "game.h"
#include "course.h"
#include "race_scene.h"
#include "scenery.h"

#define FOG_EASE    F32_BITS(0x3E999999u) /* 0.3: the damping of the fog's Body1s and colour Body */
#define FOG_PULL    F32_BITS(0x3D8F5C28u) /* 0.07: how fast the blended fog follows its target */

/* the fog presets of the courses that do not follow the fog track: per course {near, far, r, g, b} */
typedef struct FogPreset {
  f32 near, far;
  Vec3 color;
} FogPreset;
HW_SIZE(FogPreset, 20);
#define g_fog_presets ((const FogPreset *)hw_ptr(0x8C084694u))

/* per weather kind: {spread_xz, spread_up, spread_down, damping_y, damping_y range} */
typedef struct WeatherPreset {
  f32 spread[3];
  f32 damping_y;    /* the particles' y damping starts here ... */
  f32 damping_span; /* ... and goes down by this much over particle_max particles */
} WeatherPreset;
HW_SIZE(WeatherPreset, 20);
#define g_weather_presets ((const WeatherPreset *)hw_ptr(0x8C097148u))

#define RANDOM_FLOATS      0x8C084494u /* a table of 100 random floats in 0..1 */
#define RANDOM_FLOAT_COUNT 100
#define g_course_sound_tables ((const gaddr *)hw_ptr(0x8C096F9Cu)) /* a CourseSoundKey table per course */

/* func_8C010F04 (game/gfx_init.c, no header): the background colour and the fog's near / far distances */
static void background_set(uint32_t argb, f32 fog_near, f32 fog_far) {
  FARGS2(fog_near, fog_far);
  CALL1(0x8C010F04u, argb);
}
/* func_8C0433D6 (still translated): move the weather particles (replaced when the weather key changes) */
/* func_8C042648: the variant of the race scene's course */
static int32_t race_scene_course_variant(gaddr scene) { return (int32_t)CALL1(0x8C042648u, scene); }
/* func_8C02EE6A: stop the sound on a player's channel */
static void race_scene_sound_stop(gaddr scene, int32_t player, int32_t channel) {
  CALL3(0x8C02EE6Au, scene, player, channel);
}
/* func_8C02EFEC: play a sound through the music player (bank, id), by the game's mode */
static void race_scene_music_play(gaddr scene, int32_t player, int32_t channel, int32_t bank, int32_t id) {
  CALL5(0x8C02EFECu, scene, player, channel, bank, id);
}
/* func_8C02F076: stop it */
static void race_scene_music_stop(gaddr scene, int32_t player, int32_t channel) {
  CALL3(0x8C02F076u, scene, player, channel);
}

static const FogKey *fog_key_at(gaddr a) { return (const FogKey *)hw_ptr(a); }

/* was func_8C04278E (a jump to func_8C042920): CourseEnv's destructor */
HW_EXPORT(8c04278e, course_env_free(ARG0))

/* was func_8C042C88: hand the colour Body's position (r, g, b, opaque) and the fog's near / far to the background */
void course_env_apply_background(gaddr env) {
  const CourseEnv *e = course_env_at(env);
  uint32_t argb = 0xFF00;
  argb |= (uint8_t)f32_to_int(e->fog_color.position.x);
  argb <<= 8;
  argb |= (uint8_t)f32_to_int(e->fog_color.position.y);
  argb <<= 8;
  argb |= (uint8_t)f32_to_int(e->fog_color.position.z);
  background_set(argb, e->fog_near.value, e->fog_far.value);
}
HW_EXPORT(8c042c88, course_env_apply_background(ARG0))

/* was func_8C04306E: the current view's light intensity, colour and direction from the light rig's three Bodies */
void course_env_apply_lights(gaddr env) {
  view_light_intensity_set(member_addr(env, CourseEnv, lights[0]));
  view_light_color_set(member_addr(env, CourseEnv, lights[1]));
  view_light_dir_set(member_addr(env, CourseEnv, lights[2]));
}
HW_EXPORT(8c04306e, course_env_apply_lights(ARG0))

/* was func_8C0429E4: start on the fog track: its second key's mode, near, far and colour, eased with 0.3 */
void course_env_init_fog(gaddr env, gaddr track) {
  CourseEnv *e = course_env_at(env);
  e->fog_track = track;
  const FogKey *k = fog_key_at(e->fog_track) + 1;
  e->fog_mode = k->mode;
  e->fog_near.value = k->near;
  e->fog_near.damping = FOG_EASE;
  e->fog_far.value = k->far;
  e->fog_far.damping = FOG_EASE;
  vec3_set(k->color.x, k->color.y, k->color.z, member_addr(env, CourseEnv, fog_color.position));
  vec3_fill(member_addr(env, CourseEnv, fog_color.damping), FOG_EASE);
  course_env_apply_background(env);
}
HW_EXPORT(8c0429e4, course_env_init_fog(ARG0, ARG1))

/* was func_8C042D7E: the fixed fog of the set-up's course (mode 0), eased with 0.3 (the track is kept) */
void course_env_init_fog_preset(gaddr env, gaddr track) {
  CourseEnv *e = course_env_at(env);
  e->fog_track = track;
  const FogPreset *p = &g_fog_presets[race_setup_at(race_setup_get())->course];
  e->fog_mode = 0;
  e->fog_near.value = p->near;
  e->fog_near.damping = FOG_EASE;
  e->fog_far.value = p->far;
  e->fog_far.damping = FOG_EASE;
  e->fog_color.position = p->color; /* through a local Vec3, a block copy */
  vec3_fill(member_addr(env, CourseEnv, fog_color.damping), FOG_EASE);
  course_env_apply_background(env);
}
HW_EXPORT(8c042d7e, course_env_init_fog_preset(ARG0, ARG1))

/* was func_8C042A60: follow the fog track (the first key past the camera's cell, `next`, and the one before it):
 * in modes 0 and 1 the colour, near and far are pulled toward a blend of the two keys by vec2c . vec20 (mode 1, or
 * a previous key of mode 3, blends `next` with itself) and the fog is on where the ground's attribute has bit 6;
 * in modes 2 and 3 they jump to `next`'s values while the camera is below its height, else to the other key's
 * (mode 3: the key after `next`). Then the three ease one step. */
void course_env_update_fog_blend(gaddr env) {
  CourseEnv *e = course_env_at(env);
  const FogKey *next = fog_key_at(e->fog_track);
  while (e->cell >= next->cell) next++;
  const FogKey *other = next - 1;
  e->fog_mode = next->mode;
  switch (e->fog_mode) {
  case 1:
    other = next;
    /* fall through */
  case 0: {
    if (other->mode == 3) other = next;
    f32 t = njInnerProduct(member_addr(env, CourseEnv, vec2c), member_addr(env, CourseEnv, vec20));
    gaddr target = hw_stack_alloc(sizeof(Vec3));
    Vec3 *c = vec3_at(target);
    c->x = blend_signed(t, next->color.x, other->color.x);
    c->y = blend_signed(t, next->color.y, other->color.y);
    c->z = blend_signed(t, next->color.z, other->color.z);
    body_pull(FOG_PULL, member_addr(env, CourseEnv, fog_color), target);
    hw_stack_free(sizeof(Vec3));
    body1_pull(blend_signed(t, next->near, other->near), FOG_PULL, member_addr(env, CourseEnv, fog_near));
    body1_pull(blend_signed(t, next->far, other->far), FOG_PULL, member_addr(env, CourseEnv, fog_far));
    uint16_t attribute = (uint16_t)race_scene_cell_attr(panel_get(), e->cell);
    if (attribute & 0x40)
      e->fog_on = 1;
    else
      e->fog_on = 0;
    break;
  }
  case 3:
    other = next + 1;
    /* fall through */
  case 2:
    if (fgt(next->height, e->eye.y)) {
      e->fog_near.value = next->near;
      e->fog_far.value = next->far;
      e->fog_color.position = next->color; /* through a local Vec3, a block copy */
      e->fog_on = 1;
    } else {
      e->fog_near.value = other->near;
      e->fog_far.value = other->far;
      e->fog_color.position = other->color;
      e->fog_on = 0;
    }
    break;
  default:
    break;
  }
  body1_step(member_addr(env, CourseEnv, fog_near));
  body1_step(member_addr(env, CourseEnv, fog_far));
  body_step(member_addr(env, CourseEnv, fog_color));
}
HW_EXPORT(8c042a60, course_env_update_fog_blend(ARG0))

/* was func_8C04315C: place the weather particles around the camera: the kind's spread, each particle at random
 * (three floats of the random table) within it, its damping (0.8, y, 0.8) with y going down over the particles,
 * and its texture animation by the kind (0: 0; 1: 1..4 by the particle; 2: 5, 6, 7 in turn; 3: 10 every fourth,
 * else 0) */
void course_env_place_particles(gaddr env) {
  CourseEnv *e = course_env_at(env);
  const WeatherPreset *p = &g_weather_presets[e->weather];
  e->spread_xz = p->spread[0];
  e->spread_up = p->spread[1];
  e->spread_down = p->spread[2];
  Vec3 damping = {F32_BITS(0x3F4CCCCCu), p->damping_y, F32_BITS(0x3F4CCCCCu)}; /* 0.8 */
  f32 step = fdiv(p->damping_span, f32_from_int(e->particle_max));
  gaddr anim = e->particle_anims;
  int32_t turn = 0;
  gaddr body = e->particles;
  for (int32_t i = 0; i < e->particle_max; i++, body += sizeof(Body)) {
    damping.y = fsub(damping.y, step);
    body_at(body)->damping = damping; /* a block copy */
    const f32 *r = (const f32 *)hw_ptr(e->random_next);
    f32 ra = r[0], rb = r[1], rc = r[2];
    e->random_next += 3 * sizeof(f32);
    e->random_used += 3;
    if (e->random_used > RANDOM_FLOAT_COUNT - 1) {
      e->random_next -= RANDOM_FLOAT_COUNT * sizeof(f32);
      e->random_used -= RANDOM_FLOAT_COUNT;
    }
    f32 x = fadd(fsub(fmul(fmul(ra, e->spread_xz), F32(2.0f)), e->spread_xz), e->eye.x);
    f32 y = fadd(fsub(fmul(rb, fadd(e->spread_up, e->spread_down)), e->spread_down), e->eye.y);
    f32 z = fadd(fsub(fmul(fmul(rc, e->spread_xz), F32(2.0f)), e->spread_xz), e->eye.z);
    vec3_set(x, y, z, body);
    int32_t *a = (int32_t *)hw_ptr(anim);
    switch (e->weather) {
    case 0:
      *a = 0;
      anim += 4;
      break;
    case 1:
      *a = (i & 3) + 1;
      anim += 4;
      break;
    case 2:
      *a = turn + 5;
      anim += 4;
      turn++;
      if (turn >= 3) turn = 0;
      break;
    case 3:
      *a = (i & 3) == 0 ? 10 : 0;
      anim += 4;
      break;
    default:
      break;
    }
  }
}
HW_EXPORT(8c04315c, course_env_place_particles(ARG0))

/* was func_8C04309E: start on the weather track: its first key's kind, count (at most particle_max) and drift, no
 * push, the particles placed around the camera (whose position is noted) */
void course_env_init_weather(gaddr env, gaddr track) {
  CourseEnv *e = course_env_at(env);
  e->weather_track = track;
  e->weather = ((const WeatherKey *)hw_ptr(e->weather_track))->kind;
  e->last_weather = e->weather;
  e->particle_count = ((const WeatherKey *)hw_ptr(e->weather_track))->count;
  if (e->particle_count > e->particle_max) e->particle_count = e->particle_max;
  e->drift.x = ((const WeatherKey *)hw_ptr(e->weather_track))->drift.x;
  e->drift.y = ((const WeatherKey *)hw_ptr(e->weather_track))->drift.y;
  e->drift.z = ((const WeatherKey *)hw_ptr(e->weather_track))->drift.z;
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(env, CourseEnv, push));
  e->word160 = 0;
  e->word15c = 0;
  course_env_place_particles(env);
  memmove(&e->last_eye, &e->eye, sizeof(Vec3));
}
HW_EXPORT(8c04309e, course_env_init_weather(ARG0, ARG1))

/* was func_8C04298E: draw an active CourseEnv: the background (unless the fog is the simple kind), the lights, the
 * weather particles, the sky */
void course_env_draw(gaddr env) {
  const CourseEnv *e = course_env_at(env);
  if (e->active == 0) return;
  if (e->no_fog != 0)
    method_empty();
  else
    course_env_apply_background(env);
  course_env_apply_lights(env);
  course_env_draw_particles(env);
  course_env_draw_sky(env);
}
HW_EXPORT(8c04298e, course_env_draw(ARG0))

/* was func_8C042952: update an active CourseEnv: its fog (switched by the track when no_fog, else blended), its
 * lights, its weather, its cell keys, its sounds */
void course_env_update(gaddr env) {
  const CourseEnv *e = course_env_at(env);
  if (e->active == 0) return;
  if (e->no_fog != 0)
    course_env_update_fog(env);
  else
    course_env_update_fog_blend(env);
  course_env_update_lights(env);
  course_env_update_weather(env);
  course_env_update_keys(env);
  course_env_update_sounds(env);
}
HW_EXPORT(8c042952, course_env_update(ARG0))

/* was func_8C0427DC: set a CourseEnv up for player `view` from the course's tracks ({fog, light, weather, then the
 * three words of the cell keys}): the particle arrays (particle_max Bodies and animation numbers, allocated once;
 * at least one of each), the random table, the fog (on in game modes 0, 4 and 5: from the track; else the
 * course's preset, the view's fog off), the lights, the weather, the cell keys and the course's sound table;
 * then it is active */
void course_env_setup(gaddr env, int32_t view, int32_t particle_max, gaddr tracks) {
  CourseEnv *e = course_env_at(env);
  const gaddr *t = (const gaddr *)hw_ptr(tracks);
  e->view = view;
  e->particle_max = particle_max;
  if (e->particles == 0) {
    gaddr block = syMalloc((uint32_t)(particle_max != 0 ? particle_max : 1) * sizeof(Body));
    gaddr particles;
    if (block != 0)
      particles = array_construct_new(particle_max, sizeof(Body), block, 0x8C042792u); /* body_reset_set2 */
    else
      particles = 0;
    e->particles = particles;
  }
  if (e->particle_anims == 0) {
    gaddr anims = syMalloc((uint32_t)(particle_max != 0 ? particle_max : 1) * 4u);
    e->particle_anims = anims;
  }
  e->random_used = 0;
  e->random_next = RANDOM_FLOATS;
  e->fog_on = 0;
  int32_t mode = race_setup_at(g_race_setup)->mode;
  if (mode == 0 || mode == 4 || mode == 5) {
    e->no_fog = 0;
    view_fog_enable(hw_addr(&g_views[e->view]));
  } else {
    e->no_fog = 1;
    view_fog_disable(hw_addr(&g_views[e->view]));
  }
  if (e->no_fog != 0)
    course_env_init_fog_preset(env, t[0]);
  else
    course_env_init_fog(env, t[0]);
  course_env_init_lights(env, t[1]);
  course_env_init_weather(env, t[2]);
  course_env_set_keys(env, tracks + 3 * sizeof(gaddr));
  course_env_set_table(env, g_course_sound_tables[race_setup_at(g_race_setup)->course]);
  e->active = 1;
}
HW_EXPORT(8c0427dc, course_env_setup(ARG0, (int32_t)ARG1, (int32_t)ARG2, ARG3))

/* translate to the camera's x, z and the given height (raise: that much below the camera) (the vector is made on the guest stack) */
static void sky_draw_at(gaddr env, int32_t raise, f32 height) {
  gaddr at = hw_stack_alloc(sizeof(Vec3));
  vec3_copy(at, member_addr(env, CourseEnv, eye));
  if (raise)
    vec3_at(at)->y = fsub(vec3_at(at)->y, height);
  else
    vec3_at(at)->y = height;
  matrix_translate_v(at);
  hw_stack_free(sizeof(Vec3));
}

/* was func_8C04391E: draw the sky by the cell key's b (keys.word8 the texture list, keys.word4[key_a] the model):
 * 1: 200 below the camera, the fog off unless it is on; 2: at height 270, fog off; 3 (1 when no_fog): 200 below
 * the camera, fog on; then the view's own fog again */
void course_env_draw_sky(gaddr env) {
  CourseEnv *e = course_env_at(env);
  njSetTexture(e->keys.word8);
  if (e->no_fog != 0 && e->key_b == 3) e->key_b = 1;
  switch (e->key_b) {
  case 1:
    matrix_push();
    sky_draw_at(env, 1, F32(200.0f));
    if (e->fog_on == 0) fog_enable(0);
    nj_ModelL1(((const gaddr *)hw_ptr(e->keys.word4))[e->key_a]);
    view_apply_fog();
    break;
  case 2:
    matrix_push();
    sky_draw_at(env, 0, F32(270.0f));
    fog_enable(0);
    nj_ModelL1(((const gaddr *)hw_ptr(e->keys.word4))[e->key_a]);
    view_apply_fog();
    break;
  case 3:
    matrix_push();
    sky_draw_at(env, 1, F32(200.0f));
    fog_enable(1);
    nj_ModelL1(((const gaddr *)hw_ptr(e->keys.word4))[e->key_a]);
    view_apply_fog();
    break;
  default: /* 0: no sky */
    return;
  }
  matrix_pop();
}
HW_EXPORT(8c04391e, course_env_draw_sky(ARG0))

/* was func_8C043AB8: the course's ambient sounds: each record of the sound table starts its sound on its channel
 * when the camera's cell enters its range (not again while it plays; bit 6: not in course variant 1) and stops it
 * when the cell leaves; slots[i] is the sound record i plays (-1 = none) */
void course_env_update_sounds(gaddr env) {
  CourseEnv *e = course_env_at(env);
  gaddr rec = e->course_table;
  for (int32_t i = 0; ((const CourseSoundKey *)hw_ptr(rec))->first_cell != -1; i++, rec += sizeof(CourseSoundKey)) {
    const CourseSoundKey *k = (const CourseSoundKey *)hw_ptr(rec);
    int32_t channel = k->flags & 0xF;
    if (e->cell >= k->first_cell && !(e->cell > k->last_cell)) {
      if (e->slots[i] == k->sound) continue;
      if ((k->flags & 0x40) && race_scene_course_variant(panel_get()) == 1) continue;
      if (k->flags & 0x80)
        race_scene_music_play(panel_get(), e->view, channel, 8, k->sound);
      else
        race_scene_sound(panel_get(), e->view, channel, 8, k->sound);
      e->slots[i] = k->sound;
    } else {
      if (e->slots[i] != k->sound) continue;
      if (k->flags & 0x80)
        race_scene_music_stop(panel_get(), e->view, channel);
      else
        race_scene_sound_stop(panel_get(), e->view, channel);
      e->slots[i] = -1;
    }
  }
}
HW_EXPORT(8c043ab8, course_env_update_sounds(ARG0))

/* ---- the Course's CourseEnvs ---- */

/* func_8C0407B8 (game/camera.c, no header): the address of a view's camera request */
static gaddr view_request(gaddr view) { return CALL1(0x8C0407B8u, view); }

/* was func_8C041222: set up one CourseEnv per player view (by the game's mode: 1 with 250 particles in modes 0, 4
 * and 5, 2 with 250 in mode 1, 4 with 100 in modes 2 and 3) from the course's tracks, each with its view's camera
 * request (position as the eye, direction as vec20); the shown view back to 0 and the views' bit sets cleared */
void course_setup_envs(gaddr course) {
  Course *c = course_at(course);
  int32_t count, particles;
  switch (race_setup_at(g_race_setup)->mode) {
  case 0:
  case 4:
  case 5:
    count = 1;
    particles = 250;
    break;
  case 1:
    count = 2;
    particles = 250;
    break;
  case 2:
  case 3:
    particles = 100;
    count = 4;
    break;
  default: /* never (modes are 0..5): the original leaves both registers as they came */
    count = 0;
    particles = 0;
    break;
  }
  c->env_count = count;
  for (int32_t i = 0; i < count; i++) {
    gaddr request = view_request(hw_addr(&g_views[i]));
    course_env_setup(member_addr(course, Course, envs[i]), i, particles, member_addr(course, Course, fog_track));
    const ViewCamera *r = (const ViewCamera *)hw_ptr(request);
    Vec3 eye = r->position; /* through locals, block copies */
    memmove(&c->envs[i].eye, &eye, sizeof(Vec3));
    Vec3 direction = r->direction;
    memmove(&c->envs[i].vec20, &direction, sizeof(Vec3));
  }
  c->shown_view = 0;
  /* cell_count / 32 + 1 words of each view's bits (the original has no bound against the row's 10 words) */
  for (int32_t row = 0; row < COURSE_ENVS; row++) {
    uint32_t *bits = &c->view_bits[0][0] + row * 10;
    for (int32_t j = 0; j < c->cell_count / 32 + 1; j++) bits[j] = 0;
  }
}
HW_EXPORT(8c041222, course_setup_envs(ARG0))

/* was func_8C041356: the CourseEnvs are set up on the first call; the pad's X steps the shown view through the
 * players */
void course_update_views(gaddr course) {
  Course *c = course_at(course);
  if (!((int32_t)c->word28 > 1)) {
    course_setup_envs(course);
    c->word28 = 2;
  }
  if (pad_pressed(&c->pad, PAD_X) != 0) {
    c->shown_view++;
    if (c->shown_view >= race_setup_at(g_race_setup)->player_count) c->shown_view = 0;
  }
}
HW_EXPORT(8c041356, course_update_views(ARG0))
