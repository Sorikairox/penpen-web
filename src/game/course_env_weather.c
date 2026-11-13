/* Pen Pen port, S16 (b05): a CourseEnv's weather update (func_8C0433D6), the step course_env_update runs every
 * frame.
 *
 * The weather track's key for the camera's cell gives the kind of weather, the number of particles and their drift.
 * A new kind places the particles afresh around the camera. Otherwise, when the camera jumped more than 200 units
 * (in x or z) since the particles were last moved with it, they all shift by the camera's move. Then every particle
 * (all particle_max of them) is pushed by the drift, the scene's push and a fall, plus a little random sideways
 * jitter, steps, and wraps around inside a box around the camera (spread_xz sideways, spread_up above,
 * spread_down below). The scene's push is used up. */
#include <string.h>
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "course.h"

#define WEATHER_JUMP     F32(200.0f)              /* a camera move this far (x or z) carries the particles along */
#define WEATHER_FALL     F32_BITS(0xBECCCCCCu)    /* -0.39999998: added to the force's y */
#define JITTER_SCALE     F32_BITS(0x3EFFFFFFu)    /* 0.49999997 */
#define JITTER_OFFSET    F32_BITS(0xBE7FFFFFu)    /* -0.24999999: a random float 0..1 becomes -0.25..0.25 */
#define WRAP_MARGIN      F32(10.0f)               /* a particle leaving the box comes back this far inside */
#define RANDOM_FLOAT_COUNT 100                    /* the random table's floats (course_env.c) */

/* the original's frame (0x60 bytes under PR): the Vec3s it hands to the vector helpers */
typedef struct WeatherFrame {
  uint8_t pad_00[0x08];  /* 0x00: the jitter (kept in variables here) */
  Vec3 shift;            /* 0x08: the camera's move since last_eye */
  uint8_t pad_14[0x04];  /* 0x14: the particle counter */
  Vec3 below;            /* 0x18: (spread_xz, spread_down, spread_xz) */
  Vec3 low;              /* 0x24: the box's low corner */
  Vec3 above;            /* 0x30: (spread_xz, spread_up, spread_xz) */
  Vec3 high;             /* 0x3C: its high corner */
  Vec3 force;            /* 0x48: drift + push + the fall */
  uint8_t rest[0x60 - 0x54]; /* 0x54: the particle pointer, the key, env */
} WeatherFrame;
HW_OFFSET(WeatherFrame, shift, 0x08);
HW_OFFSET(WeatherFrame, below, 0x18);
HW_OFFSET(WeatherFrame, high, 0x3C);
HW_OFFSET(WeatherFrame, force, 0x48);
HW_SIZE(WeatherFrame, 0x60);

/* was func_8C0433D6: follow the weather track and move the weather particles (see the top of the file) */
void course_env_update_weather(gaddr env) {
  CourseEnv *e = course_env_at(env);
  gaddr frame = hw_stack_alloc(sizeof(WeatherFrame));
  WeatherFrame *f = (WeatherFrame *)hw_ptr(frame);

  const WeatherKey *key = (const WeatherKey *)hw_ptr(e->weather_track);
  while (e->cell >= key->cell)
    key++;
  e->weather = key->kind;
  e->particle_count = key->count;
  if (e->particle_count > e->particle_max)
    e->particle_count = e->particle_max;
  e->drift.x = key->drift.x;
  e->drift.y = key->drift.y;
  e->drift.z = key->drift.z;

  if (e->weather != e->last_weather) {
    course_env_place_particles(env);
    e->word160 = 0;
    e->word15c = 0;
  } else {
    if (fgt(f32_abs(fsub(e->last_eye.x, e->eye.x)), WEATHER_JUMP) ||
        fgt(f32_abs(fsub(e->last_eye.z, e->eye.z)), WEATHER_JUMP)) {
      gaddr shift = member_addr(frame, WeatherFrame, shift);
      vec3_copy(shift, member_addr(env, CourseEnv, eye));
      vec3_sub(shift, member_addr(env, CourseEnv, last_eye));
      gaddr body = e->particles;
      for (int32_t i = 0; i < e->particle_max; i++, body += sizeof(Body))
        vec3_add(body, shift);
    }
    memmove(&e->last_eye, &e->eye, sizeof(Vec3));
  }
  e->last_weather = e->weather;

  gaddr body = e->particles;
  gaddr force = member_addr(frame, WeatherFrame, force);
  vec3_copy(force, member_addr(env, CourseEnv, drift));
  vec3_add(force, member_addr(env, CourseEnv, push));
  f->force.y = fadd(f->force.y, WEATHER_FALL);
  gaddr high = member_addr(frame, WeatherFrame, high);
  vec3_copy(high, member_addr(env, CourseEnv, eye));
  vec3_set(e->spread_xz, e->spread_up, e->spread_xz, member_addr(frame, WeatherFrame, above));
  vec3_add(high, member_addr(frame, WeatherFrame, above));
  gaddr low = member_addr(frame, WeatherFrame, low);
  vec3_copy(low, member_addr(env, CourseEnv, eye));
  vec3_set(e->spread_xz, e->spread_down, e->spread_xz, member_addr(frame, WeatherFrame, below));
  vec3_sub(low, member_addr(frame, WeatherFrame, below));

  for (int32_t i = 0; i < e->particle_max; i++, body += sizeof(Body)) {
    const f32 *r = (const f32 *)hw_ptr(e->random_next);
    e->random_next += sizeof(f32);
    f32 ra = r[0];
    r = (const f32 *)hw_ptr(e->random_next);
    e->random_next += sizeof(f32);
    f32 rb = r[0];
    e->random_used += 2;
    if (e->random_used > RANDOM_FLOAT_COUNT - 1) {
      e->random_next -= RANDOM_FLOAT_COUNT * sizeof(f32);
      e->random_used -= RANDOM_FLOAT_COUNT;
    }
    f32 jitter_x = fmac(JITTER_SCALE, ra, JITTER_OFFSET);
    f32 jitter_z = fmac(JITTER_SCALE, rb, JITTER_OFFSET);
    vec3_add_xyz(jitter_x, F32(0.0f), jitter_z, member_addr(body, Body, force));
    vec3_add(member_addr(body, Body, force), force);
    body_step(body);
    /* past a side of the box, a particle comes back WRAP_MARGIN inside the other side */
    Body *b = body_at(body);
    if (fgt(b->position.x, f->high.x))
      b->position.x = fadd(f->low.x, WRAP_MARGIN);
    else if (fgt(f->low.x, b->position.x))
      b->position.x = fadd(f->high.x, F32(-10.0f));
    if (fgt(b->position.y, f->high.y))
      b->position.y = fadd(f->low.y, WRAP_MARGIN);
    else if (fgt(f->low.y, b->position.y))
      b->position.y = fadd(f->high.y, F32(-10.0f));
    if (fgt(b->position.z, f->high.z))
      b->position.z = fadd(f->low.z, WRAP_MARGIN);
    else if (fgt(f->low.z, b->position.z))
      b->position.z = fadd(f->high.z, F32(-10.0f));
  }
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(env, CourseEnv, push));
  hw_stack_free(sizeof(WeatherFrame));
}
HW_EXPORT(8c0433d6, course_env_update_weather(ARG0))
