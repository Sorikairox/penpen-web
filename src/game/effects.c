/* Pen Pen port, S14 (b07): a racer's effects (0x8C038A16-0x8C03C198): the child task that owns them (RacerEffects,
 * vtable 0x8C23DB1C) and its five parts: the ribbon (a trail drawn as one Ninja model built at run time) and four
 * particle sets drawn as sprites or small motion models (game/effects.h).
 *
 * A particle set keeps one array per field (`count` entries each) and a Body per particle. Every particle goes
 * through the same states: 0 off, 1 waiting out a counter (or playing its motion), 2 growing a size up to a limit
 * (its alpha is 1 - size / limit when drawn), 3 ending (back to 0). Set C's emitter is here; the ribbon set's and
 * set B's (func_8C0397A8, func_8C03B6F2) are still translated. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/ninja.h"
#include "../core/inline.h"
#include "../core/file.h"
#include "../core/task.h"
#include "effects.h"
#include "racer.h"
#include "game.h"
#include "scenery.h"

/* ---- globals of the effects ---- */
#define g_effects_alive    GUEST_VAR(int32_t, 0x8C096D0Cu) /* RacerEffects alive (the shared data is freed at 0) */
#define g_shared_data      GUEST_VAR(gaddr, 0x8C096D1Cu)   /* model.c's shared container */
#define g_shared_word      GUEST_VAR(uint32_t, 0x8C096D10u) /* model.c's: the texture list the effects draw with */
#define g_ribbon_length    GUEST_VAR(int32_t, 0x8C082FACu)  /* model.c's: pairs per half (32) */
#define g_ribbon_colors    (&GUEST_VAR(uint32_t, 0x8C082FB4u)) /* the diffuse colour of each quad's material */
#define g_ribbon_texture   GUEST_VAR(uint32_t, 0x8C082FB0u)  /* the materials' texture word (0xCA) */
#define MATERIAL_DEFAULT   0x8C096BD0u                       /* an NJS_ARGB the constant material starts from */
#define SET_A_SPRITE_ANIM  0x8C096CD0u                       /* set A's NJS_TEXANIM */
#define SET_C_SPRITE_ANIM  0x8C096CF8u                       /* set C's NJS_TEXANIM */
#define VEC3_CONSTRUCT     0x8C03898Cu                       /* Vec3's empty constructor (for the array helper) */
#define BODY_CONSTRUCT     0x8C0391F2u                       /* body_reset_set (model.c) */
#define RACER_EFFECTS_VTABLE 0x8C23DB1Cu

#define g_ribbon_size_limit GUEST_VAR(f32, 0x8C083038u) /* 20.0 */
#define g_ribbon_spin_step  GUEST_VAR(f32, 0x8C08303Cu) /* 0.033: the motion models' scale step */
#define g_ribbon_grow_0     GUEST_VAR(f32, 0x8C083040u) /* 0.00795: a kind-0 sprite's scale step */
#define g_ribbon_grow       GUEST_VAR(f32, 0x8C083044u) /* 0.00117: the other sprites' */
#define g_set_a_scale       GUEST_VAR(f32, 0x8C083070u) /* 0.05 */
#define g_set_a_size_limit  GUEST_VAR(f32, 0x8C083074u) /* 20.0 */
#define g_set_b_size_limit  GUEST_VAR(f32, 0x8C0830FCu) /* 20.0 */
#define g_set_b_grow        GUEST_VAR(f32, 0x8C083100u) /* 0.015625 */
#define g_set_b_grow_2      GUEST_VAR(f32, 0x8C0830B8u) /* 0.017: kind 2 */
#define g_set_b_grow_0      GUEST_VAR(f32, 0x8C0830C4u) /* 0.01953125: kind 0 */
#define g_set_b_grow_other  GUEST_VAR(f32, 0x8C083104u) /* 0.015625: kinds 3-7 */
#define g_set_b_limit_2     GUEST_VAR(f32, 0x8C0830C0u) /* 20.0: kind 2's size limit */
#define g_set_b_limit       GUEST_VAR(f32, 0x8C0830CCu) /* 5.0: kinds 0, 3-7 */
#define g_set_c_count       GUEST_VAR(int32_t, 0x8C08310Cu) /* 32 (stage.c's g_object_set_c_count) */
#define g_set_c_scale       GUEST_VAR(f32, 0x8C08311Cu) /* 0.078125 */
#define g_set_c_limit       GUEST_VAR(f32, 0x8C083120u) /* 10.0 */
#define SET_C_CAPACITY 32 /* set C's arrays are allocated for 32 particles whatever g_set_c_count says */

/* the rest of a Ninja basic model after file.h's NjModel (NJS_MODEL: its bounding sphere) */
typedef struct NjModelFull {
  NjModel base;        /* 0x00 */
  Vec3 center;         /* 0x18 */
  f32 r;               /* 0x24 */
} NjModelFull;
HW_OFFSET(NjModelFull, center, 0x18);
HW_SIZE(NjModelFull, 0x28);

/* NJS_MATERIAL */
typedef struct NjMaterial {
  uint32_t diffuse;    /* 0x00: ARGB */
  uint32_t specular;   /* 0x04 */
  f32 exponent;        /* 0x08 */
  uint32_t attr_texid; /* 0x0C */
  uint32_t attrflags;  /* 0x10 */
} NjMaterial;
HW_SIZE(NjMaterial, 0x14);

/* NJS_ARGB */
typedef struct NjArgb {
  f32 a, r, g, b;
} NjArgb;
HW_SIZE(NjArgb, 16);

/* an index's texture coordinates in a mesh set's UV array (NJS_TEX) */
typedef struct NjTex {
  int16_t u, v;
} NjTex;
HW_SIZE(NjTex, 4);

enum { NJD_MESHSET_4 = 0x4000 };
#define RIBBON_MATERIAL_FLAGS 0x96B02400u
#define SPRITE_ATTR_SPRITE 0x32u /* njDrawSprite3D attributes of the camera-facing sprites */
#define SPRITE_ATTR_FLAT   0x22u /* of set B's turned ones */
#define SPRITE_ATTR_SET_C  0x33u

/* ---- calls ---- */

/* func_8C014FC0 / func_8C01506E / func_8C015024 (core/model_build.c, not in a header): room for points and normals,
 * mesh sets, materials */
static void model_alloc_points(gaddr model, int32_t n) { CALL2(0x8C014FC0u, model, n); }
static void model_alloc_meshsets(gaddr model, int32_t n) { CALL2(0x8C01506Eu, model, n); }
static void model_alloc_materials(gaddr model, int32_t n) { CALL2(0x8C015024u, model, n); }
/* func_8C01509A: mesh set i: type, material, n polygons, with UVs, with colours (7 arguments, 3 on the stack) */
static void model_alloc_meshset(gaddr model, int32_t i, uint32_t type, uint32_t material, int32_t n, int32_t with_uv,
                                int32_t with_colors) {
  const uint32_t args[7] = {model, (uint32_t)i, type, material, (uint32_t)n, (uint32_t)with_uv, (uint32_t)with_colors};
  hw_call_stack(0x8C01509Au, 7, args);
}
/* func_8C014FAC: free a built model's points, materials and mesh sets */
static void model_free(gaddr model) { CALL1(0x8C014FACu, model); }
/* func_8C011740: draw a shape motion at a frame (as core/motion.c's shape_motion_draw_l1, with another Ninja draw);
 * the frame through a pointer, the loop flag 5th */
static void shape_motion_draw(gaddr object, gaddr motion, gaddr shape, gaddr frame, int32_t loop) {
  CALL5(0x8C011740u, object, motion, shape, frame, loop);
}
/* func_8C02C9C0: the floor's height under a course cell (r4 = the object g_panel points to) */
static f32 floor_height(gaddr scene, int32_t cell) {
  CALL2(0x8C02C9C0u, scene, cell);
  return hw_fret();
}

/* typed views of the per-particle arrays */
static int32_t *i32s(gaddr a) { return (int32_t *)hw_ptr(a); }
static f32 *f32s(gaddr a) { return (f32 *)hw_ptr(a); }
static gaddr body_of(gaddr bodies, int32_t i) { return bodies + (uint32_t)i * sizeof(Body); }
/* syMalloc of n words, at least one */
static gaddr words_alloc(int32_t n) { return syMalloc((uint32_t)(n ? n : 1) * 4); }

/* ---- small helpers ---- */

/* was func_8C038A16: the constant material (Ninja) of colour (r, g, b) and alpha a, from a copy of the default */
void effect_material_set(f32 r, f32 g, f32 b, f32 a) {
  gaddr argb = hw_stack_alloc(20); /* the original's local, 16 bytes below its saved PR */
  memmove(hw_ptr(argb), hw_ptr(MATERIAL_DEFAULT), sizeof(NjArgb));
  NjArgb *c = (NjArgb *)hw_ptr(argb);
  c->a = a;
  c->r = r;
  c->g = g;
  c->b = b;
  njSetConstantMaterial(argb);
  hw_stack_free(20);
}
HW_EXPORT(8c038a16, effect_material_set(FARG0, FARG1, FARG2, FARG3))

/* ---- the effects task ---- */

/* was func_8C038B32: the task part set up by hand (active, stops on pause, no links), the owner noted, the shared
 * data loaded (counted), then the five parts built; returns the task */
gaddr racer_effects_init(gaddr fx, gaddr racer) {
  RacerEffects *e = racer_effects_at(fx);
  e->task.linger = 0;
  e->task.active = 1;
  e->task.stops_on_pause = 1;
  e->task.child = 0;
  e->task.next = 0;
  e->task.parent = 0;
  e->task.owner = 0;
  e->racer = racer;
  shared_data_load();
  g_effects_alive++;
  gaddr p = syMalloc(sizeof(Ribbon));
  e->ribbon = p ? ribbon_construct(p) : 0;
  p = syMalloc(sizeof(RibbonSet));
  e->ribbons = p ? ribbon_set_construct(p) : 0;
  p = syMalloc(sizeof(EffectSetA));
  e->set_a = p ? effect_set_a_construct(p, 32) : 0;
  p = syMalloc(sizeof(EffectSetB));
  e->set_b = p ? effect_set_b_construct(p, 32) : 0;
  p = syMalloc(sizeof(EffectSetC));
  e->set_c = p ? effect_set_c_construct(p) : 0;
  return fx;
}
HW_EXPORT(8c038b32, RET(racer_effects_init(ARG0, ARG1)))

/* was func_8C038C1C: the constructor (vtable, then the parts) */
gaddr racer_effects_construct(gaddr fx, gaddr racer) {
  racer_effects_at(fx)->task.vtable = RACER_EFFECTS_VTABLE;
  racer_effects_init(fx, racer);
  return fx;
}
HW_EXPORT(8c038c1c, RET(racer_effects_construct(ARG0, ARG1)))

/* was func_8C038C32: the destructor body: each part freed (its arrays, then itself); the last one frees the shared
 * data */
void racer_effects_free(gaddr fx) {
  const RacerEffects *e = racer_effects_at(fx);
  gaddr p = e->ribbon;
  if (p != 0) {
    ribbon_free(p);
    syFree(p);
  }
  p = e->ribbons;
  if (p != 0) {
    ribbon_set_free(p);
    syFree(p);
  }
  p = e->set_a;
  if (p != 0) {
    effect_set_a_free(p);
    syFree(p);
  }
  p = e->set_b;
  if (p != 0) {
    effect_set_b_free(p);
    syFree(p);
  }
  p = e->set_c;
  if (p != 0) {
    effect_set_c_free(p);
    syFree(p);
  }
  if (--g_effects_alive == 0) {
    file_free(g_shared_data);
    g_shared_data = 0;
  }
}
HW_EXPORT(8c038c32, racer_effects_free(ARG0))

/* was func_8C038CB4: the deleting destructor (vtable 0x8C23DB1C's first method) */
gaddr racer_effects_delete(gaddr fx, int32_t free_memory) {
  racer_effects_free(fx);
  if (free_memory) syFree(fx);
  return fx;
}
HW_EXPORT(8c038cb4, RET(racer_effects_delete(ARG0, (int32_t)ARG1)))

/* was func_8C038CEA: the update: each part steps (the ribbon has nothing to do here) */
void racer_effects_update(gaddr fx) {
  const RacerEffects *e = racer_effects_at(fx);
  method_empty(); /* the ribbon's update (func_8C038FEC) is empty */
  ribbon_set_update(e->ribbons);
  effect_set_a_update(e->set_a);
  effect_set_b_update(e->set_b);
  effect_set_c_update(e->set_c);
}
HW_EXPORT(8c038cea, racer_effects_update(ARG0))

/* was func_8C038D40: the draw, with the shared texture list, unless the owner hides its effects */
void racer_effects_draw(gaddr fx) {
  const RacerEffects *e = racer_effects_at(fx);
  gaddr texlist = g_shared_word;
  if (e->racer != 0 && racer_at(e->racer)->effects_shown == 0) return;
  njSetTexture(texlist);
  model_draw(e->ribbon);
  ribbon_set_draw(e->ribbons);
  effect_set_a_draw(e->set_a);
  effect_set_b_draw(e->set_b);
  effect_set_c_draw(e->set_c);
}
HW_EXPORT(8c038d40, racer_effects_draw(ARG0))

/* ---- the ribbon ---- */

/* was func_8C038D94: build the ribbon's model: 4 * g_ribbon_length points (all at the origin, normals +z) and a
 * quad per pair (indices 2k .. 2k+3, the texture's corners, a grey that darkens by 8 per quad, its own material);
 * a bounding sphere of radius 10; returns the ribbon */
gaddr ribbon_construct(gaddr ribbon) {
  Ribbon *rb = (Ribbon *)hw_ptr(ribbon);
  rb->points_holder = 0;
  gaddr m = syMalloc(sizeof(NjModelFull));
  m = m ? ribbon_record_clear(m) : 0;
  rb->points_holder = m;
  model_alloc_points(m, g_ribbon_length * 4);
  model_alloc_meshsets(rb->points_holder, g_ribbon_length);
  model_alloc_materials(rb->points_holder, g_ribbon_length);
  for (int32_t k = 0; k < g_ribbon_length; k++) {
    model_alloc_meshset(rb->points_holder, k, NJD_MESHSET_4, (uint32_t)k, 1, 1, 1);
    const NjModel *model = (const NjModel *)hw_ptr(rb->points_holder);
    Vec3 *points = vec3_at(model->points);   /* the first four only, every time: the trail moves them */
    Vec3 *normals = vec3_at(model->normals);
    const NjMeshset *set = (const NjMeshset *)hw_ptr(model->meshsets + (uint32_t)k * sizeof(NjMeshset));
    int16_t *indices = (int16_t *)hw_ptr(set->meshes);
    NjTex *uvs = (NjTex *)hw_ptr(set->vertex_uvs);
    uint32_t *colors = (uint32_t *)hw_ptr(set->vertex_colors);
    for (int i = 0; i < 4; i++) points[i] = (Vec3){F32(0.0f), F32(0.0f), F32(0.0f)};
    for (int i = 0; i < 4; i++) normals[i] = (Vec3){F32(0.0f), F32(0.0f), F32(1.0f)};
    for (int i = 0; i < 4; i++) indices[i] = (int16_t)(2 * k + i);
    uvs[0] = (NjTex){0, 0};
    uvs[1] = (NjTex){0xFF, 0};
    uvs[2] = (NjTex){0, 0xFF};
    uvs[3] = (NjTex){0xFF, 0xFF};
    uint32_t grey = 0xFFu - 8u * (uint32_t)k;
    uint32_t color = grey << 24 | grey << 16 | grey << 8 | grey;
    for (int i = 0; i < 4; i++) colors[i] = color;
    NjMaterial *mat = (NjMaterial *)hw_ptr(((const NjModel *)hw_ptr(rb->points_holder))->materials) + k;
    mat->diffuse = g_ribbon_colors[k];
    mat->specular = 0xFFFFFFFFu;
    mat->exponent = F32(1.0f);
    mat->attr_texid = g_ribbon_texture;
    mat->attrflags = RIBBON_MATERIAL_FLAGS;
  }
  NjModelFull *full = (NjModelFull *)hw_ptr(rb->points_holder);
  full->center.x = F32(0.0f);
  full = (NjModelFull *)hw_ptr(rb->points_holder);
  full->center.y = F32(0.0f);
  full = (NjModelFull *)hw_ptr(rb->points_holder);
  full->center.z = F32(0.0f);
  full = (NjModelFull *)hw_ptr(rb->points_holder);
  full->r = F32(10.0f);
  return ribbon;
}
HW_EXPORT(8c038d94, RET(ribbon_construct(ARG0)))

/* was func_8C038FC8: free the ribbon's model (its arrays, then the model) */
void ribbon_free(gaddr ribbon) {
  gaddr m = ((const Ribbon *)hw_ptr(ribbon))->points_holder;
  if (m != 0) {
    model_free(m);
    syFree(m);
  }
}
HW_EXPORT(8c038fc8, ribbon_free(ARG0))

/* ---- the ribbon set (RibbonSet: motion models and sprites) ---- */

static int ribbon_kind_is_model(int32_t kind) { return kind == 8 || kind == 10; }
static int ribbon_kind_is_sprite(int32_t kind) { return kind == 3 || kind == 4 || kind == 5 || kind == 0; }

/* was func_8C03945C (its body runs on through func_8C039484): step each particle. A motion model (kinds 8, 10)
 * plays its motion to the last frame, grows to the limit, then ends, its scale growing all along; a sprite waits
 * out its counter, grows, then ends, its scale growing and its Body moving */
void ribbon_set_update(gaddr set) {
  const RibbonSet *s = (const RibbonSet *)hw_ptr(set);
  int32_t *kind = i32s(s->kinds), *counter = i32s(s->counters), *state = i32s(s->states);
  f32 *size = f32s(s->sizes), *scale = f32s(s->scales), *frame = f32s(s->frames);
  gaddr body = s->bodies;
  for (int32_t n = s->count; n > 0; n--) {
    const f32 limit = g_ribbon_size_limit;
    if (ribbon_kind_is_model(*kind)) {
      if (*state != 0) {
        if (*state == 1) {
          *frame = fadd(*frame, F32(1.0f));
          f32 last = fadd(f32_from_uint((uint32_t)((const NjMotion *)hw_ptr(s->model))->nb_frames), F32(-1.0f));
          if (!fgt(last, *frame)) {
            *frame = fadd(f32_from_uint((uint32_t)((const NjMotion *)hw_ptr(s->model))->nb_frames), F32(-1.0f));
            *size = F32(0.0f);
            (*state)++;
          }
          *scale = fadd(*scale, g_ribbon_spin_step);
        } else if (*state == 2) {
          if (fgt(limit, *size)) {
            *size = fadd(*size, F32(1.0f));
          } else {
            *size = limit;
            (*state)++;
          }
          *scale = fadd(*scale, g_ribbon_spin_step);
        } else if (*state == 3) {
          *state = 0;
        }
      }
    } else if (ribbon_kind_is_sprite(*kind)) {
      if (*state == 1) {
        if ((*counter)-- < 0) (*state)++;
        *scale = fadd(*scale, *kind == 0 ? g_ribbon_grow_0 : g_ribbon_grow);
        body_step(body);
      } else if (*state == 2) {
        if (fgt(limit, *size))
          *size = fadd(*size, F32(1.0f));
        else
          (*state)++;
        *scale = fadd(*scale, *kind == 0 ? g_ribbon_grow_0 : g_ribbon_grow);
        body_step(body);
      } else if (*state == 3) {
        *state = 0;
      }
    }
    counter++;
    frame++;
    kind++;
    state++;
    scale++;
    body += sizeof(Body);
    size++;
  }
}
HW_EXPORT(8c03945c, ribbon_set_update(ARG0))

/* was func_8C039602: draw the particles that are on, blended (Control3D 0x20): a motion model at its Body, turned
 * and scaled (3 high), fading as it grows; a sprite at its Body, at its scale */
void ribbon_set_draw(gaddr set) {
  control3d_push();
  control3d_set(0x20);
  const RibbonSet *s = (const RibbonSet *)hw_ptr(set);
  gaddr kind = s->kinds, state = s->states, size = s->sizes, scale = s->scales, alpha = s->alphas;
  gaddr frame = s->frames, angle = s->angles, body = s->bodies;
  for (int32_t i = 0; i < ((const RibbonSet *)hw_ptr(set))->count; i++) {
    if (*i32s(state) != 0) {
      int32_t k = *i32s(kind);
      if (ribbon_kind_is_model(k)) {
        *f32s(alpha) = fsub(F32(0.0f), fdiv(*f32s(size), g_ribbon_size_limit));
        effect_material_set(F32(1.0f), F32(1.0f), F32(1.0f), *f32s(alpha));
        matrix_push();
        matrix_translate_v(body);
        matrix_rotate_y(*f32s(angle));
        matrix_scale(*f32s(scale), F32(3.0f), *f32s(scale));
        const RibbonSet *r = (const RibbonSet *)hw_ptr(set);
        shape_motion_draw(r->object, r->model, r->texture, frame, 0);
        matrix_pop();
      } else if (ribbon_kind_is_sprite(k)) {
        *f32s(alpha) = fsub(F32(1.0f), fdiv(*f32s(size), g_ribbon_size_limit));
        effect_material_set(F32(1.0f), F32(1.0f), F32(1.0f), *f32s(alpha));
        RibbonSet *r = (RibbonSet *)hw_ptr(set);
        r->sprite.sx = *f32s(scale);
        r->sprite.sy = *f32s(scale);
        r->sprite.p.x = body_at(body)->position.x;
        r->sprite.p.y = body_at(body)->position.y;
        r->sprite.p.z = body_at(body)->position.z;
        njDrawSprite3D(set, *i32s(kind), SPRITE_ATTR_SPRITE);
      }
    }
    state += 4;
    size += 4;
    scale += 4;
    alpha += 4;
    kind += 4;
    frame += 4;
    angle += 4;
    body += sizeof(Body);
  }
  control3d_pop();
}
HW_EXPORT(8c039602, ribbon_set_draw(ARG0))

/* ---- set A: puffs drifting up from an origin ---- */

/* was func_8C039CF2: build set A of `count` puffs: its sprite (scale g_set_a_scale), the arrays (the two Vec3 arrays
 * constructed), every puff off at offset 0, size 0, alpha 1; returns the set */
gaddr effect_set_a_construct(gaddr set, int32_t count) {
  EffectSetA *a = (EffectSetA *)hw_ptr(set);
  a->sprite.p.x = F32(0.0f);
  a->sprite.p.y = F32(0.0f);
  a->sprite.p.z = F32(0.0f);
  a->sprite.sx = g_set_a_scale;
  a->sprite.sy = g_set_a_scale;
  a->sprite.ang = 0;
  a->sprite.tlist = g_shared_word;
  a->sprite.tanim = SET_A_SPRITE_ANIM;
  a->heading = F32(0.0f);
  a->count = count;
  a->word24 = 0;
  a->counters = words_alloc(a->count);
  a->types = words_alloc(a->count);
  int32_t n = a->count;
  gaddr block = syMalloc((uint32_t)(n ? n : 1) * sizeof(Vec3));
  a->origins = block ? array_construct_new(a->count, sizeof(Vec3), block, VEC3_CONSTRUCT) : 0;
  n = a->count;
  block = syMalloc((uint32_t)(n ? n : 1) * sizeof(Vec3));
  a->offsets = block ? array_construct_new(a->count, sizeof(Vec3), block, VEC3_CONSTRUCT) : 0;
  a->scales = words_alloc(a->count);
  a->sizes = words_alloc(a->count);
  a->alphas = words_alloc(a->count);
  a->states = words_alloc(a->count);
  gaddr counter = a->counters, state = a->states, type = a->types;
  gaddr offset = a->offsets, scale = a->scales, size = a->sizes, alpha = a->alphas;
  for (int32_t k = a->count; k > 0; k--) {
    *i32s(counter) = 0;
    *f32s(scale) = g_set_a_scale;
    Vec3 *o = vec3_at(offset);
    o->z = F32(0.0f);
    o->y = F32(0.0f);
    o->x = F32(0.0f);
    *f32s(size) = F32(0.0f);
    *f32s(alpha) = F32(1.0f);
    *i32s(state) = 0;
    *i32s(type) = 0;
    counter += 4;
    scale += 4;
    offset += sizeof(Vec3);
    size += 4;
    alpha += 4;
    state += 4;
    type += 4;
  }
  return set;
}
HW_EXPORT(8c039cf2, RET(effect_set_a_construct(ARG0, (int32_t)ARG1)))

/* the drift of a waiting puff along the set's heading: offset += 0.0066667 * (trig(heading) * counter) */
static void set_a_drift(gaddr set, gaddr counter, gaddr offset) {
  const f32 rate = F32_BITS(0x3BDA740Cu); /* 0.0066666668 */
  f32 c = njCos(f32_to_int(((const EffectSetA *)hw_ptr(set))->heading));
  Vec3 *o = vec3_at(offset);
  o->z = fmac(rate, fmul(c, f32_from_int(*i32s(counter))), o->z);
  f32 s = njSin(f32_to_int(((const EffectSetA *)hw_ptr(set))->heading));
  o->x = fmac(rate, fmul(s, f32_from_int(*i32s(counter))), o->x);
}

/* a random jitter of a puff's offset in x and z: random(range) + shift on each */
static void set_a_jitter(gaddr offset, f32 range, f32 shift) {
  Vec3 *o = vec3_at(offset);
  f32 r = fadd(random_timer_scaled(range), shift);
  o->z = fadd(o->z, r);
  r = fadd(random_timer_scaled(range), shift);
  o->x = fadd(o->x, r);
}

/* was func_8C039F6E: step each puff: waiting (state 1), it counts down while it rises (type 1 faster) and drifts
 * along the heading, jittered every fifth count; growing (2), it rises slowly, jittered every third size step;
 * 3 switches it off */
void effect_set_a_update(gaddr set) {
  const EffectSetA *a = (const EffectSetA *)hw_ptr(set);
  gaddr offset = a->offsets, size = a->sizes, type = a->types, state = a->states, counter = a->counters;
  for (int32_t n = a->count; n > 0; n--) {
    int32_t *st = i32s(state);
    if (*st == 1) {
      int32_t *c = i32s(counter);
      if (*c > 0)
        (*c)--;
      else
        (*st)++;
      Vec3 *o = vec3_at(offset);
      if (*i32s(type) == 1)
        o->y = fadd(o->y, F32_BITS(0x3E4CCCCCu)); /* + 0.2 */
      else
        o->y = fadd(o->y, F32_BITS(0x3DF5C28Eu)); /* + 0.12 */
      set_a_drift(set, counter, offset);
      if (*i32s(counter) % 5 == 0) set_a_jitter(offset, F32_BITS(0x3EFFFFFFu), F32_BITS(0xBE7FFFFFu)); /* 0.49999997, -0.24999999 */
    } else if (*st == 2) {
      if (fgt(g_set_a_size_limit, *f32s(size)))
        *f32s(size) = fadd(*f32s(size), F32(1.0f));
      else
        (*st)++;
      Vec3 *o = vec3_at(offset);
      o->y = fadd(o->y, F32_BITS(0x3DCCCCCCu)); /* + 0.1 */
      if (f32_to_int(*f32s(size)) % 3 == 0)
        set_a_jitter(offset, F32_BITS(0x3E4CCCCCu), F32_BITS(0xBDCCCCCCu)); /* 0.2, -0.1 */
    } else if (*st == 3) {
      *st = 0;
    }
    state += 4;
    type += 4;
    counter += 4;
    offset += sizeof(Vec3);
    size += 4;
  }
}
HW_EXPORT(8c039f6e, effect_set_a_update(ARG0))

/* was func_8C03A144: draw the puffs that are on, as sprites at origin + offset, fading as they grow, while above the
 * floor of the set's cell (less 2) (Control3D 0x10) */
void effect_set_a_draw(gaddr set) {
  control3d_push();
  control3d_set(0x10);
  const EffectSetA *a = (const EffectSetA *)hw_ptr(set);
  gaddr state = a->states, origin = a->origins, offset = a->offsets, size = a->sizes;
  gaddr alpha = a->alphas, type = a->types;
  for (int32_t n = a->count; n > 0; n--) {
    if (*i32s(state) != 0) {
      f32 al = fsub(F32(1.0f), fdiv(*f32s(size), g_set_a_size_limit));
      *f32s(alpha) = al;
      effect_material_set(F32(1.0f), F32(1.0f), F32(1.0f), al);
      EffectSetA *s = (EffectSetA *)hw_ptr(set);
      const Vec3 *o = vec3_at(origin), *d = vec3_at(offset);
      s->sprite.p.x = fadd(o->x, d->x);
      s->sprite.p.y = fadd(o->y, d->y);
      s->sprite.p.z = fadd(o->z, d->z);
      f32 floor = fadd(floor_height(g_panel, s->cell), F32_BITS(0xBFFFFFFFu)); /* - 1.9999999 */
      if (fgt(floor, ((const EffectSetA *)hw_ptr(set))->sprite.p.y))
        njDrawSprite3D(set, *i32s(type), SPRITE_ATTR_SPRITE);
    }
    state += 4;
    origin += sizeof(Vec3);
    alpha += 4;
    offset += sizeof(Vec3);
    size += 4;
    type += 4;
  }
  control3d_pop();
}
HW_EXPORT(8c03a144, effect_set_a_draw(ARG0))

/* ---- set B ---- */

/* the scale step of set B's sprite kinds 0-7 */
static f32 set_b_sprite_grow(int32_t kind) {
  if (kind == 2) return g_set_b_grow_2;
  if (kind == 0) return g_set_b_grow_0;
  return g_set_b_grow_other;
}

/* was func_8C03B194: step each particle by its kind: 12-15 a motion model (plays to its last frame, grows, ends; no
 * Body); 8-11 a flat sprite (waits, grows, its scale growing twice as fast); 0, 2-7 a sprite (waits, grows to its
 * kind's limit); 1 and above 15 nothing. The sprites' Bodies move */
void effect_set_b_update(gaddr set) {
  const EffectSetB *b = (const EffectSetB *)hw_ptr(set);
  gaddr kind = b->kinds, counter = b->counters, state = b->states, scale = b->scales, size = b->sizes;
  gaddr frame = b->frames, body = b->bodies;
  for (int32_t n = b->count; n > 0; n--) {
    uint32_t k = (uint32_t)*i32s(kind);
    int32_t *st = i32s(state);
    if (k >= 12 && k <= 15) {
      if (*st == 1) {
        *f32s(frame) = fadd(*f32s(frame), F32(1.0f));
        const NjMotion *m = (const NjMotion *)hw_ptr(((const EffectSetB *)hw_ptr(set))->motion);
        if (!fgt(f32_from_uint((uint32_t)(m->nb_frames - 1)), *f32s(frame))) {
          m = (const NjMotion *)hw_ptr(((const EffectSetB *)hw_ptr(set))->motion);
          *f32s(frame) = f32_from_uint((uint32_t)(m->nb_frames - 1));
          *f32s(size) = F32(0.0f);
          (*st)++;
        }
      } else if (*st == 2) {
        f32 limit = g_set_b_size_limit;
        if (fgt(limit, *f32s(size))) {
          *f32s(size) = fadd(*f32s(size), F32(1.0f));
        } else {
          *f32s(size) = limit;
          (*st)++;
        }
        *f32s(scale) = fadd(*f32s(scale), g_set_b_grow);
      } else if (*st == 3) {
        *st = 0;
      }
    } else if (k >= 8 && k <= 11) {
      if (*st == 1) {
        if ((*i32s(counter))-- < 0) (*st)++;
        *f32s(scale) = fmac(F32(2.0f), g_set_b_grow, *f32s(scale));
        body_step(body);
      } else if (*st == 2) {
        if (fgt(g_set_b_size_limit, *f32s(size)))
          *f32s(size) = fadd(*f32s(size), F32(1.0f));
        else
          (*st)++;
        *f32s(scale) = fmac(F32(2.0f), g_set_b_grow, *f32s(scale));
        body_step(body);
      } else if (*st == 3) {
        *st = 0;
      }
    } else if (k < 8 && k != 1) {
      if (*st == 1) {
        if ((*i32s(counter))-- < 0) (*st)++;
        *f32s(scale) = fadd(*f32s(scale), set_b_sprite_grow(*i32s(kind)));
        body_step(body);
      } else if (*st == 2) {
        f32 limit = *i32s(kind) == 2 ? g_set_b_limit_2 : g_set_b_limit;
        if (fgt(limit, *f32s(size)))
          *f32s(size) = fadd(*f32s(size), F32(1.0f));
        else
          (*st)++;
        *f32s(scale) = fadd(*f32s(scale), set_b_sprite_grow(*i32s(kind)));
        body_step(body);
      } else if (*st == 3) {
        *st = 0;
      }
    }
    counter += 4;
    scale += 4;
    size += 4;
    state += 4;
    body += sizeof(Body);
    kind += 4;
    frame += 4;
  }
}
HW_EXPORT(8c03b194, effect_set_b_update(ARG0))

/* a sprite of set B at its Body and scale, fading by size / limit */
static void set_b_draw_sprite(gaddr set, gaddr kind, gaddr size, gaddr alpha, gaddr scale, gaddr body, f32 limit) {
  *f32s(alpha) = fsub(F32(1.0f), fdiv(*f32s(size), limit));
  effect_material_set(F32(1.0f), F32(1.0f), F32(1.0f), *f32s(alpha));
  EffectSetB *b = (EffectSetB *)hw_ptr(set);
  b->sprite.p.x = body_at(body)->position.x;
  b->sprite.p.y = body_at(body)->position.y;
  b->sprite.p.z = body_at(body)->position.z;
  b->sprite.sx = *f32s(scale);
  b->sprite.sy = *f32s(scale);
  njDrawSprite3D(set, *i32s(kind), SPRITE_ATTR_SPRITE);
}

/* was func_8C03B488: draw the particles that are on (Control3D 0x20): kinds 8-11 a sprite laid flat (turned by the
 * sprite's angle, then a quarter turn about x) at its Body; 12-15 the motion model at its Body (scaled in x and z);
 * 0, 2-7 a sprite */
void effect_set_b_draw(gaddr set) {
  control3d_push();
  control3d_set(0x20);
  const EffectSetB *b = (const EffectSetB *)hw_ptr(set);
  gaddr kind = b->kinds, state = b->states, scale = b->scales, alpha = b->alphas;
  gaddr size = b->sizes, frame = b->frames, body = b->bodies;
  for (int32_t n = b->count; n > 0; n--) {
    if (*i32s(state) != 0) {
      uint32_t k = (uint32_t)*i32s(kind);
      if (k >= 8 && k <= 11) {
        *f32s(alpha) = fsub(F32(1.0f), fdiv(*f32s(size), g_set_b_size_limit));
        effect_material_set(F32(1.0f), F32(1.0f), F32(1.0f), *f32s(alpha));
        matrix_push();
        matrix_translate_v(body);
        matrix_rotate_y(f32_from_int(((const EffectSetB *)hw_ptr(set))->sprite.ang));
        matrix_rotate_x(F32(16384.0f));
        matrix_scale_all(*f32s(scale));
        njDrawSprite3D(set, *i32s(kind), SPRITE_ATTR_FLAT);
        matrix_pop();
      } else if (k >= 12 && k <= 15) {
        *f32s(alpha) = fsub(F32(0.0f), fdiv(*f32s(size), g_set_b_size_limit));
        effect_material_set(F32(1.0f), F32(1.0f), F32(1.0f), *f32s(alpha));
        matrix_push();
        matrix_translate_v(body);
        matrix_scale(*f32s(scale), F32(1.0f), *f32s(scale));
        const EffectSetB *s = (const EffectSetB *)hw_ptr(set);
        shape_motion_draw(s->object, s->motion, s->texture, frame, 0);
        matrix_pop();
      } else if (k == 2) {
        set_b_draw_sprite(set, kind, size, alpha, scale, body, g_set_b_limit_2);
      } else if (k < 8 && k != 1) {
        set_b_draw_sprite(set, kind, size, alpha, scale, body, g_set_b_limit);
      }
    }
    body += sizeof(Body);
    scale += 4;
    state += 4;
    size += 4;
    alpha += 4;
    kind += 4;
    frame += 4;
  }
  control3d_pop();
}
HW_EXPORT(8c03b488, effect_set_b_draw(ARG0))

/* ---- set C ---- */

/* was func_8C03BE38: build set C: its sprite (scale 0.15625, a random angle), arrays for SET_C_CAPACITY particles and
 * their bodies; the first g_set_c_count particles off, scale g_set_c_scale, alpha 1; returns the set */
gaddr effect_set_c_construct(gaddr set) {
  EffectSetC *c = (EffectSetC *)hw_ptr(set);
  c->sprite.p.x = F32(0.0f);
  c->sprite.p.y = F32(0.0f);
  c->sprite.p.z = F32(0.0f);
  c->sprite.sx = F32_BITS(0x3E200000u); /* 0.15625 */
  c->sprite.sy = F32_BITS(0x3E200000u);
  c->sprite.ang = f32_to_int(random_timer_scaled(F32(65536.0f)));
  c->sprite.tlist = g_shared_word;
  c->sprite.tanim = SET_C_SPRITE_ANIM;
  c->counters = syMalloc(SET_C_CAPACITY * 4);
  c->states = syMalloc(SET_C_CAPACITY * 4);
  c->array30 = syMalloc(SET_C_CAPACITY * 4);
  c->array34 = syMalloc(SET_C_CAPACITY * 4);
  c->values = syMalloc(SET_C_CAPACITY * 4);
  c->array3c = syMalloc(SET_C_CAPACITY * 4);
  gaddr block = syMalloc(SET_C_CAPACITY * sizeof(Body));
  c->bodies = block ? array_construct_new(SET_C_CAPACITY, sizeof(Body), block, BODY_CONSTRUCT) : 0;
  gaddr counter = c->counters, state = c->states, scale = c->array30, alpha = c->array34;
  gaddr value = c->values, other = c->array3c, body = c->bodies;
  for (int32_t n = g_set_c_count; n > 0; n--) {
    *i32s(counter) = 0;
    *i32s(state) = 0;
    *f32s(scale) = g_set_c_scale;
    *f32s(alpha) = F32(1.0f);
    *f32s(value) = F32(0.0f);
    *f32s(other) = F32(0.0f);
    body_reset(body);
    counter += 4;
    state += 4;
    scale += 4;
    alpha += 4;
    value += 4;
    other += 4;
    body += sizeof(Body);
  }
  return set;
}
HW_EXPORT(8c03be38, RET(effect_set_c_construct(ARG0)))

/* was func_8C03C0BC: draw the particles that are on as sprites at their Bodies, fading as their value grows to
 * g_set_c_limit (Control3D 0x10) */
void effect_set_c_draw(gaddr set) {
  control3d_push();
  control3d_set(0x10);
  const EffectSetC *c = (const EffectSetC *)hw_ptr(set);
  gaddr body = c->bodies, value = c->values, state = c->states, scale = c->array30, alpha = c->array34;
  for (int32_t n = g_set_c_count; n > 0; n--) {
    if (*i32s(state) != 0) {
      f32 al = fsub(F32(1.0f), fdiv(*f32s(value), g_set_c_limit));
      *f32s(alpha) = al;
      effect_material_set(F32(1.0f), F32(1.0f), F32(1.0f), al);
      EffectSetC *s = (EffectSetC *)hw_ptr(set);
      s->sprite.p.x = body_at(body)->position.x;
      s->sprite.p.y = body_at(body)->position.y;
      s->sprite.p.z = body_at(body)->position.z;
      s->sprite.sx = *f32s(scale);
      s->sprite.sy = *f32s(scale);
      njDrawSprite3D(set, 0, SPRITE_ATTR_SET_C);
    }
    alpha += 4;
    value += 4;
    body += sizeof(Body);
    state += 4;
    scale += 4;
  }
  control3d_pop();
}
HW_EXPORT(8c03c0bc, effect_set_c_draw(ARG0))

#define g_set_c_wait_range GUEST_VAR(f32, 0x8C083114u)     /* 20.0: the spread of a new particle's wait */
#define g_set_c_wait       GUEST_VAR(int32_t, 0x8C083110u) /* 30: its middle */
#define g_set_c_damping    GUEST_VAR(f32, 0x8C083124u)     /* 1.0 */
#define SET_C_EMIT_MAX 8 /* particles started per call */

/* was func_8C03C198: start up to 8 particles of set C (the first ones that are off) at the frame's position: a
 * random velocity (x and z in [-0.67, 1], y in [-0.5, 1.17]), no damping, a wait of 30 +- 10 updates, a random
 * value in the third array (+-910) */
void effect_set_c_emit(gaddr set, gaddr frame) {
  const f32 spread = F32_BITS(0x3FD55555u);    /* 1.6666666 */
  const f32 shift_xz = F32_BITS(0xBF555555u);  /* -0.6666666 */
  const f32 shift_y = F32(-0.5f);
  const EffectSetC *c = (const EffectSetC *)hw_ptr(set);
  gaddr counter = c->counters, scale = c->array30, state = c->states, alpha = c->array34;
  gaddr body = c->bodies, other = c->array3c, value = c->values;
  int32_t started = 0;
  for (int32_t n = g_set_c_count; n > 0; n--) {
    if (*i32s(state) == 0) {
      body_reset(body);
      vec3_copy_bytes(body, member_addr(frame, RibbonFrame, position));
      body_at(body)->velocity.x = fadd(random_timer_scaled(spread), shift_xz);
      body_at(body)->velocity.y = fadd(random_timer_scaled(spread), shift_y);
      body_at(body)->velocity.z = fadd(random_timer_scaled(spread), shift_xz);
      vec3_fill(member_addr(body, Body, damping), g_set_c_damping);
      *f32s(scale) = g_set_c_scale;
      f32 wait = fsub(random_timer_scaled(g_set_c_wait_range), fmul(g_set_c_wait_range, F32(0.5f)));
      *i32s(counter) = f32_to_int(wait) + g_set_c_wait;
      *f32s(value) = F32(0.0f);
      *f32s(alpha) = F32(1.0f);
      *i32s(state) = 1;
      *f32s(other) = fadd(random_timer_scaled(F32_BITS(0x44E38E38u)), F32_BITS(0xC4638E38u)); /* 1820.44, -910.22 */
      if (++started >= SET_C_EMIT_MAX) return;
    }
    counter += 4;
    value += 4;
    other += 4;
    scale += 4;
    state += 4;
    alpha += 4;
    body += sizeof(Body);
  }
}
HW_EXPORT(8c03c198, effect_set_c_emit(ARG0, ARG1))
