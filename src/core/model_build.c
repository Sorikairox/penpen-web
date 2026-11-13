/* Pen Pen port, S13: building a basic Ninja model (NJS_MODEL, file.h's NjModel) in memory (core/model_build.c).
 *
 * The game makes some models at run time: these allocate and free the model's point and normal arrays, its
 * materials (NJS_MATERIAL, 20 bytes each) and its mesh sets with their index, colour and UV arrays. */
#include "hw.h"
#include "sdk.h"
#include "../core/file.h"

#define NJS_MATERIAL_SIZE 20
enum { NJD_MESHSET_3 = 0x0000, NJD_MESHSET_4 = 0x4000 }; /* triangles, quads (the type bits of type_material) */

static inline NjModel *model_at(gaddr a) { return (NjModel *)hw_ptr(a); }

/* was func_8C014FC0: room for n points and n normals */
void model_alloc_points(gaddr model, int32_t n) {
  NjModel *m = model_at(model);
  m->nb_points = n;
  uint32_t size = (uint32_t)n * 12;
  m->points = syMalloc(size);
  m->normals = syMalloc(size);
}
HW_EXPORT(8c014fc0, model_alloc_points(ARG0, (int32_t)ARG1))

/* was func_8C014FF4: frees the points and normals */
void model_free_points(gaddr model) {
  NjModel *m = model_at(model);
  if (m->points != 0) {
    syFree(m->points);
    m->points = 0;
  }
  if (m->normals != 0) {
    syFree(m->normals);
    m->normals = 0;
  }
  m->nb_points = 0;
}
HW_EXPORT(8c014ff4, model_free_points(ARG0))

/* was func_8C015024: room for n materials */
void model_alloc_materials(gaddr model, int32_t n) {
  model_at(model)->nb_materials = (uint16_t)n;
  model_at(model)->materials = syMalloc((uint32_t)n * NJS_MATERIAL_SIZE);
}
HW_EXPORT(8c015024, model_alloc_materials(ARG0, (int32_t)ARG1))

/* was func_8C01504E: frees the materials */
void model_free_materials(gaddr model) {
  NjModel *m = model_at(model);
  if (m->materials != 0) {
    syFree(m->materials);
    m->materials = 0;
  }
  m->nb_materials = 0;
}
HW_EXPORT(8c01504e, model_free_materials(ARG0))

/* was func_8C01506E: room for n mesh sets */
void model_alloc_meshsets(gaddr model, int32_t n) {
  model_at(model)->nb_meshsets = (uint16_t)n;
  model_at(model)->meshsets = syMalloc((uint32_t)n * sizeof(NjMeshset));
}
HW_EXPORT(8c01506e, model_alloc_meshsets(ARG0, (int32_t)ARG1))

/* was func_8C01509A: mesh set i: its type and material, n polygons (3 or 4 indices each), the index array and,
 * when asked, a colour and a UV word per index. For another type the original uses a register it never set (the
 * caller's r13) as the index count: that value is taken from the CPU as it was at entry. */
void model_alloc_meshset(gaddr model, int32_t i, uint32_t type, uint32_t material, int32_t n, int32_t with_uv,
                         int32_t with_colors) {
  gaddr set = model_at(model)->meshsets + (uint32_t)i * sizeof(NjMeshset);
  NjMeshset *s = (NjMeshset *)hw_ptr(set);
  int32_t indices;
  if (type == NJD_MESHSET_3)
    indices = n * 3;
  else if (type == NJD_MESHSET_4)
    indices = n * 4;
  else
    indices = (int32_t)hw_cpu->r[13]; /* the caller's r13 (callee-saved: unchanged so far) */
  s->type_material = (uint16_t)(type | material);
  s->nb_meshes = (uint16_t)n;
  s->meshes = syMalloc((uint32_t)indices * 2);
  s->attrs = 0;
  s->normals = 0;
  s->vertex_colors = with_colors != 0 ? syMalloc((uint32_t)indices * 4) : 0;
  s->vertex_uvs = with_uv != 0 ? syMalloc((uint32_t)indices * 4) : 0;
}
/* S17: exported (tested by synthetic samples: a translated caller's r13 is the CPU's at entry, as the original reads) */
HW_EXPORT(8c01509a, model_alloc_meshset(ARG0, (int32_t)ARG1, ARG2, ARG3, (int32_t)ARG4, (int32_t)ARG5, (int32_t)ARG6))

/* was func_8C01513C: frees every mesh set's arrays, then the mesh sets */
void model_free_meshsets(gaddr model) {
  NjModel *m = model_at(model);
  gaddr set = m->meshsets;
  for (int32_t n = m->nb_meshsets; n > 0; n--, set += sizeof(NjMeshset)) {
    NjMeshset *s = (NjMeshset *)hw_ptr(set);
    if (s->meshes != 0)
      syFree(s->meshes);
    if (s->attrs != 0)
      syFree(s->attrs);
    if (s->normals != 0)
      syFree(s->normals);
    if (s->vertex_colors != 0)
      syFree(s->vertex_colors);
    if (s->vertex_uvs != 0)
      syFree(s->vertex_uvs);
    s->meshes = 0;
    s->attrs = 0;
    s->normals = 0;
    s->vertex_colors = 0;
    s->vertex_uvs = 0;
  }
  syFree(m->meshsets);
  m->meshsets = 0;
  m->nb_meshsets = 0;
}
HW_EXPORT(8c01513c, model_free_meshsets(ARG0))
