/* Pen Pen port, S18 b01: one model's triangles added to a Contacts set (core/contact.h, contacts_add_triangle): a
 * basic model's meshsets (triangles, quads, strips), a chunk model's strip chunks (as stored, or placed by the current
 * matrix first, over a whole tree); and the ground under a point among a chunk model tree's triangles. The other
 * walkers over a model tree are core/contact_tree.c and core/cnk_walk.c. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/file.h"
#include "../core/contact.h"
#include "../core/ninja.h"
#include "../core/inline.h"

#define VERTEX_SIZE 12u            /* a basic model's points: Vec3 */
#define g_cnk_points GUEST_VAR(gaddr, 0x8C08A888u) /* a work buffer of placed chunk vertices (Vec3 each) */

/* NJS_OBJECT's evaluation flags and its pos / ang / scl words */
#define NJD_EVAL_UNIT_POS 0x01
#define NJD_EVAL_UNIT_ANG 0x02
#define NJD_EVAL_UNIT_SCL 0x04
#define NJD_EVAL_BREAK    0x10
#define NJD_EVAL_ZXY_ANG  0x20
enum { OBJECT_POS = 0, OBJECT_ANG = 3, OBJECT_SCL = 6 };

void matrix_scale_v(gaddr v); /* core/ninja_thunks.c (declared in game/actor_b05.h) */
/* SDK njOuterProduct (0x8C0C3D04; the original calls it through its thunk func_8C013ABA): out = a x b */
static inline f32 njOuterProduct(gaddr a, gaddr b, gaddr out) { CALL3(0x8C0C3D04u, a, b, out); return hw_fret(); }

/* a basic meshset's kind (the top two bits of type_material) */
#define MESH_KIND_MASK   0xC000u
#define MESH_TRIANGLES   0x0000u
#define MESH_QUADS       0x4000u
#define MESH_STRIPS      0xC000u
#define STRIP_LENGTH_MASK 0x7FFF    /* a strip's first index: its length (bit 15: its winding) */

/* chunk model data (Ninja's chunk format): the vertex chunk's type gives its vertices' size in words; the polygon
 * list is 16-bit words, a chunk's type in the low byte of its first word */
#define CNK_END          0x00FF
#define CNK_BITS_MIN     8          /* 8..15: one-word chunks (and one more word) */
#define CNK_TINY_MIN     0x10       /* 0x10..0x1F: tiny / material chunks */
#define CNK_TINY_TEXTURE 0x14       /* its flags byte follows */
#define CNK_MATERIAL_A   0x15
#define CNK_MATERIAL_B   0x16
#define CNK_MATERIAL_C   0x17
#define CNK_VOLUME_MIN   0x20       /* 0x20..0x3F: a sized chunk skipped */
#define CNK_STRIP_MIN    0x40       /* 0x40..0x48: strips */
#define CNK_NO_FLAGS     0xFFu      /* the triangle kind passed while no tiny chunk set one */
#define CNK_STRIP_COUNT_MASK 0x3FFF /* the strip chunk's header: the strip count (bits 14-15: user words per index) */

static inline const NjModel *model_at(gaddr a) { return (const NjModel *)hw_ptr(a); }
static inline const NjMeshset *meshset_at(gaddr a) { return (const NjMeshset *)hw_ptr(a); }
static inline int16_t s16_at(gaddr a) { return *(const int16_t *)hw_ptr(a); }

/* a vertex of the model's point array by its (signed) index */
static inline gaddr point(gaddr points, int16_t index) { return points + (uint32_t)((int32_t)index * (int32_t)VERTEX_SIZE); }

/* was func_8C01431E: every triangle of one basic model into the contacts (kind 0): per meshset (its count a 16-bit
 * word), triangles take three indices each; quads four (a b c, then c b d); strips a length word (bit 15 its
 * winding, ignored) then length indices, each new one making a triangle with the two before (alternate ones turned
 * over); other kinds add nothing */
void contacts_add_model(gaddr contacts, gaddr model) {
  const NjModel *m = model_at(model);
  int32_t sets = (int32_t)m->nb_meshsets;
  gaddr set = m->meshsets;
  gaddr points = m->points;
  for (; sets > 0; sets--, set += sizeof(NjMeshset)) {
    int32_t meshes = (int32_t)meshset_at(set)->nb_meshes;
    gaddr idx = meshset_at(set)->meshes;
    uint32_t kind = meshset_at(set)->type_material & MESH_KIND_MASK;
    if (kind == MESH_TRIANGLES) {
      while (meshes > 0) {
        gaddr a = point(points, s16_at(idx));
        gaddr b = point(points, s16_at(idx + 2));
        gaddr c = point(points, s16_at(idx + 4));
        idx += 6;
        contacts_add_triangle(contacts, a, b, c, 0);
        meshes--;
      }
    } else if ((meshset_at(set)->type_material & MESH_KIND_MASK) == MESH_QUADS) {
      while (meshes > 0) {
        gaddr a = point(points, s16_at(idx));
        gaddr b = point(points, s16_at(idx + 2));
        gaddr c = point(points, s16_at(idx + 4));
        gaddr d = point(points, s16_at(idx + 6));
        idx += 8;
        contacts_add_triangle(contacts, a, b, c, 0);
        contacts_add_triangle(contacts, c, b, d, 0);
        meshes--;
      }
    } else if ((meshset_at(set)->type_material & MESH_KIND_MASK) == MESH_STRIPS) {
      while (meshes > 0) {
        int32_t left = (s16_at(idx) & STRIP_LENGTH_MASK) - 2;
        gaddr a = point(points, s16_at(idx + 2));
        gaddr b = point(points, s16_at(idx + 4));
        idx += 6;
        while (left > 0) {
          gaddr c = point(points, s16_at(idx));
          idx += 2;
          contacts_add_triangle(contacts, a, b, c, 0);
          left--;
          a = b;
          b = c;
          if (left > 0) {
            c = point(points, s16_at(idx));
            idx += 2;
            contacts_add_triangle(contacts, b, a, c, 0);
            a = b;
            left--;
            b = c;
          }
        }
        meshes--;
      }
    }
  }
}
HW_EXPORT(8c01431e, contacts_add_model(ARG0, ARG1))

/* a chunk vertex list's vertex size in words, by its type (0x20..0x32); `other` for any other type (valid models
 * have none: the two originals differ there, see their callers) */
static int32_t cnk_vertex_words(uint32_t type, int32_t other) {
  switch (type) {
  case 0x20: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27: case 0x28: case 0x30: return 4;
  case 0x21: return 8;
  case 0x22: return 3;
  case 0x29: return 6;
  case 0x2A: case 0x2B: case 0x2C: case 0x2D: case 0x2E: case 0x2F: return 7;
  case 0x31: case 0x32: return 5;
  default: return other;
  }
}

/* a strip chunk's extra words per index (its UVs and the like), by its type; the original keeps the previous strip
 * chunk's value (a frame slot) for 0x49..0xFF: kept across the model's chunks here (0 before the first, where the
 * original reads an unset slot: valid models have no such type) */
static int32_t cnk_strip_extra(int32_t type, int32_t previous) {
  if (type == 0x40) return 0;
  if (type == 0x41 || type == 0x42 || type == 0x46) return 2;
  if (type == 0x47 || type == 0x48) return 4;
  if (type == 0x43) return 6;
  if (type == 0x44 || type == 0x45) return 8;
  return previous;
}

/* the polygon list of a chunk model: every strip's triangles into the contacts, the vertex of index i at
 * vertices + i * stride words. With `tiny_flags`, the triangles carry the flags byte of the last tiny texture /
 * material chunk (0xFF after another tiny chunk, and at the start); without, the tiny chunks are skipped by their
 * size like the sized chunks and the triangles carry 0. */
static void cnk_strips(gaddr contacts, gaddr plist, gaddr vertices, int32_t stride, int tiny_flags) {
  uint32_t flags = tiny_flags ? CNK_NO_FLAGS : 0;
  int32_t extra = 0;
  gaddr p = plist;
  for (;;) {
    int16_t head = s16_at(p);
    p += 2;
    if (head == CNK_END)
      return;
    if (head == 0)
      continue;
    int32_t type = (uint8_t)head;
    if (type < CNK_BITS_MIN)
      continue;
    if (type < CNK_TINY_MIN) {
      p += 2;
      continue;
    }
    if (tiny_flags && type < CNK_VOLUME_MIN) {
      int16_t size = s16_at(p);
      p += 2;
      if (type == CNK_TINY_TEXTURE) {
        flags = (uint8_t)s16_at(p);
        p += 2;
      } else if (type == CNK_MATERIAL_A || type == CNK_MATERIAL_B) {
        p += 4;
        flags = (uint8_t)s16_at(p);
        p += 2;
      } else if (type == CNK_MATERIAL_C) {
        p += 8;
        flags = (uint8_t)s16_at(p);
        p += 2;
      } else {
        flags = CNK_NO_FLAGS;
        p += (uint32_t)((int32_t)size * 2);
        continue;
      }
      p += 2;
      continue;
    }
    if (type < CNK_STRIP_MIN) {
      int16_t size = s16_at(p);
      p += 2;
      p += (uint32_t)((int32_t)size * 2);
      continue;
    }
    /* a strip chunk: its size word, its header (count, user words), then the strips */
    extra = cnk_strip_extra(type, extra);
    p += 2;
    int16_t header = s16_at(p);
    p += 2;
    uint32_t user_bytes = (uint32_t)(((int32_t)header >> 14) & 3) * 2;
    uint32_t extra_bytes = (uint32_t)extra * 2;
    int32_t strips = header & CNK_STRIP_COUNT_MASK;
    while (strips-- != 0) {
      int32_t length = s16_at(p);
      p += 2;
      int16_t i0 = s16_at(p);
      p += 2 + extra_bytes;
      int16_t i1 = s16_at(p);
      p += 2 + extra_bytes;
      gaddr a = vertices + (uint32_t)((int32_t)i0 * stride * 4);
      gaddr b = vertices + (uint32_t)((int32_t)i1 * stride * 4);
      int flip = 0;
      if (length < 0) {
        flip = 1;
        length = -length;
      }
      length -= 2;
      while (length-- != 0) {
        gaddr c = vertices + (uint32_t)((int32_t)s16_at(p) * stride * 4);
        p += 2;
        if (!flip)
          contacts_add_triangle(contacts, a, b, c, flags);
        else
          contacts_add_triangle(contacts, a, c, b, flags);
        a = b;
        flip = !flip;
        p += extra_bytes;
        b = c;
        p += user_bytes;
      }
    }
  }
}

/* was func_8C0144F2: every strip of one chunk model into the contacts, at its vertices as stored (the vertex list's
 * first chunk: its type gives the vertex size; the vertices follow its two header words) */
void contacts_add_cnk_model(gaddr contacts, gaddr model) {
  const NjCnkModel *m = (const NjCnkModel *)hw_ptr(model);
  gaddr vlist = m->vlist;
  uint32_t type = *(const uint32_t *)hw_ptr(vlist) & 0xFFFFu;
  /* another type: the original keeps its register as it came (its caller's r11); valid models have none */
  int32_t stride = cnk_vertex_words(type, 0);
  cnk_strips(contacts, m->plist, vlist + 8, stride, 1);
}
HW_EXPORT(8c0144f2, contacts_add_cnk_model(ARG0, ARG1))

/* was func_8C0146F8: every strip of one chunk model into the contacts (kind 0), its vertices first placed by the
 * current matrix into the work buffer *0x8C08A888 (12 bytes each; the count is the vertex chunk's second word's
 * top half, signed; another type than 0x20..0x32 counts as its own size in words). The vertex is copied to a local
 * of the original's frame (0x20 bytes, the local at 0x14) for njCalcPoint. */
void contacts_add_cnk_model_posed(gaddr contacts, gaddr model) {
  const NjCnkModel *m = (const NjCnkModel *)hw_ptr(model);
  gaddr plist = m->plist;
  const uint32_t *head = (const uint32_t *)hw_ptr(m->vlist);
  uint32_t type = head[0] & 0xFFFFu;
  int32_t count = (int16_t)(head[1] >> 16);
  gaddr src = m->vlist + 8;
  int32_t stride = cnk_vertex_words(type, (int32_t)type);
  gaddr dst = g_cnk_points;
  gaddr frame = hw_stack_alloc(0x20);
  gaddr local = frame + 0x14;
  while (count-- != 0) {
    gaddr out = dst;
    vec3_copy(local, src);
    njCalcPoint(0, local, out);
    dst += VERTEX_SIZE;
    src += (uint32_t)(stride * 4);
  }
  hw_stack_free(0x20);
  cnk_strips(contacts, plist, g_cnk_points, 3, 0);
}
HW_EXPORT(8c0146f8, contacts_add_cnk_model_posed(ARG0, ARG1))

/* was func_8C0148C4: every chunk model of the tree from `object` placed by its node's transform (the evaluation
 * flags read once: position, angles in X Y Z order or Z X Y with NJD_EVAL_ZXY_ANG, scale), the node's children
 * unless NJD_EVAL_BREAK, then its siblings, into the contacts (contacts_add_cnk_model_posed) */
void contacts_add_object_posed(gaddr contacts, gaddr object) {
  const NjObject *o = (const NjObject *)hw_ptr(object);
  uint32_t flags = o->evalflags;
  matrix_push();
  if ((flags & NJD_EVAL_UNIT_POS) == 0)
    matrix_translate_v(member_addr(object, NjObject, pos_ang_scl[OBJECT_POS]));
  if ((flags & NJD_EVAL_UNIT_ANG) == 0) {
    if ((flags & NJD_EVAL_ZXY_ANG) != 0) {
      matrix_rotate_z(f32_from_int((int32_t)o->pos_ang_scl[OBJECT_ANG + 2]));
      matrix_rotate_x(f32_from_int((int32_t)o->pos_ang_scl[OBJECT_ANG + 0]));
      matrix_rotate_y(f32_from_int((int32_t)o->pos_ang_scl[OBJECT_ANG + 1]));
    } else {
      matrix_rotate_x(f32_from_int((int32_t)o->pos_ang_scl[OBJECT_ANG + 0]));
      matrix_rotate_y(f32_from_int((int32_t)o->pos_ang_scl[OBJECT_ANG + 1]));
      matrix_rotate_z(f32_from_int((int32_t)o->pos_ang_scl[OBJECT_ANG + 2]));
    }
  }
  if ((flags & NJD_EVAL_UNIT_SCL) == 0)
    matrix_scale_v(member_addr(object, NjObject, pos_ang_scl[OBJECT_SCL]));
  if (o->model != 0)
    contacts_add_cnk_model_posed(contacts, o->model);
  if ((flags & NJD_EVAL_BREAK) == 0)
    contacts_add_object_posed(contacts, o->child);
  matrix_pop();
  if (o->sibling != 0)
    contacts_add_object_posed(contacts, o->sibling);
}
HW_EXPORT(8c0148c4, contacts_add_object_posed(ARG0, ARG1))

/* ---- the ground under a point: the chunk models' triangles that hold it ---- */

#define CNK_GROUND_FLAGS 0xF4 /* strip chunks are tested only after a tiny chunk with a flags byte of 0xF4 or more */

/* was func_8C014980: is `point` over triangle (a, b, c) seen from above? Its normal (b - a) x (c - b), made unit;
 * a degenerate triangle is not, the normal is then copied to *normal_out (when given), a triangle facing down is
 * not; the point must lie on the inner side of each edge (in x/z); then *height_out = (normal . (point - a)) /
 * normal.y (how far above the triangle's plane) and 1. The vectors are locals of the original's frame (0x70 bytes:
 * kept at the same offsets for the Ninja calls). */
int32_t cnk_triangle_under(gaddr point, gaddr a, gaddr b, gaddr c, gaddr height_out, gaddr normal_out) {
  gaddr frame = hw_stack_alloc(0x70);
  gaddr ab = frame + 0x54, bc = frame + 0x48, normal = frame + 0x3C;
  gaddr ap = frame + 0x2C, bp = frame + 0x20, ca = frame + 0x14, cp = frame + 0x08;
  int32_t hit = 0;
  vec3_diff(ab, a, b);
  vec3_diff(bc, b, c);
  method_self(normal);
  njOuterProduct(ab, bc, normal);
  f32 length = njUnitVector(normal);
  if (fgt(length, F32(0.0f))) {
    if (normal_out != 0)
      memmove(hw_ptr(normal_out), hw_ptr(normal), sizeof(Vec3));
    if (fgt(vec3_at(normal)->y, F32(0.0f))) {
      vec3_diff(ap, a, point);
      if (!fgt(F32(0.0f), vec3_cross_y(ab, ap))) {
        vec3_diff(bp, b, point);
        if (!fgt(F32(0.0f), vec3_cross_y(bc, bp))) {
          vec3_diff(ca, c, a);
          vec3_diff(cp, c, point);
          if (!fgt(F32(0.0f), vec3_cross_y(ca, cp))) {
            f32 dist = njInnerProduct(normal, ap);
            *(f32 *)hw_ptr(height_out) = fdiv(dist, vec3_at(normal)->y);
            hit = 1;
          }
        }
      }
    }
  }
  hw_stack_free(0x70);
  return hit;
}
HW_EXPORT(8c014980, RET(cnk_triangle_under(ARG0, ARG1, ARG2, ARG3, ARG4, ARG5)))

/* was func_8C014A94: is `point` over a triangle of one chunk model (as stored)? Its polygon list is walked as
 * contacts_add_cnk_model's, but a strip chunk is tested only while the last tiny texture / material chunk's flags
 * byte is 0xF4 or more (else it is skipped by its size); each strip's triangles (alternate ones turned over) go to
 * cnk_triangle_under; the first that holds the point ends it with 1 (its height and normal written); else 0. For a
 * vertex type outside 0x20..0x32 the original reads an unset frame slot as the vertex size (0 here), and for a
 * strip chunk type outside 0x40..0x48 the previous strip chunk's extra words (0 before the first). */
int32_t cnk_model_under(gaddr point, gaddr model, gaddr height_out, gaddr normal_out) {
  const NjCnkModel *m = (const NjCnkModel *)hw_ptr(model);
  gaddr vlist = m->vlist;
  gaddr p = m->plist;
  uint32_t type = *(const uint32_t *)hw_ptr(vlist) & 0xFFFFu;
  gaddr vertices = vlist + 8;
  int32_t stride = cnk_vertex_words(type, 0);
  int32_t flags = (int32_t)CNK_NO_FLAGS;
  int32_t extra = 0;
  for (;;) {
    int16_t head = s16_at(p);
    p += 2;
    if (head == CNK_END)
      return 0;
    if (head == 0)
      continue;
    int32_t ctype = (uint8_t)head;
    if (ctype < CNK_BITS_MIN)
      continue;
    if (ctype < CNK_TINY_MIN) {
      p += 2;
      continue;
    }
    if (ctype < CNK_VOLUME_MIN) {
      int16_t size = s16_at(p);
      p += 2;
      if (ctype == CNK_TINY_TEXTURE) {
      } else if (ctype == CNK_MATERIAL_A) {
        p += 4;
      } else if (ctype == CNK_MATERIAL_B) {
        p += 4;
      } else if (ctype == CNK_MATERIAL_C) {
        p += 8;
      } else {
        flags = (int32_t)CNK_NO_FLAGS;
        p += (uint32_t)((int32_t)size * 2);
        continue;
      }
      flags = (uint8_t)s16_at(p);
      p += 2;
      p += 2;
      continue;
    }
    if (ctype < CNK_STRIP_MIN || flags < CNK_GROUND_FLAGS) {
      int16_t size = s16_at(p);
      p += 2;
      p += (uint32_t)((int32_t)size * 2);
      continue;
    }
    /* a strip chunk: its size word, its header (count, user words), then the strips */
    extra = cnk_strip_extra(ctype, extra);
    p += 2;
    int16_t header = s16_at(p);
    p += 2;
    int32_t user = ((int32_t)header >> 14) & 3;
    int32_t strips = header & CNK_STRIP_COUNT_MASK;
    while (strips-- != 0) {
      int32_t length = s16_at(p);
      p += 2;
      int32_t i0 = s16_at(p);
      p += 2;
      p += (uint32_t)(extra * 2);
      i0 *= stride;
      int32_t i1 = s16_at(p);
      p += 2;
      i1 *= stride;
      p += (uint32_t)(extra * 2);
      gaddr a = vertices + (uint32_t)(i0 * 4);
      gaddr b = vertices + (uint32_t)(i1 * 4);
      int32_t flip = 0;
      if (length < 0) {
        length = -length;
        flip = -1;
      }
      length -= 2;
      while (length-- != 0) {
        int32_t i2 = s16_at(p);
        p += 2;
        gaddr c = vertices + (uint32_t)(i2 * stride * 4);
        if (flip != 0) {
          if (cnk_triangle_under(point, a, c, b, height_out, normal_out) != 0)
            return 1;
        } else {
          if (cnk_triangle_under(point, a, b, c, height_out, normal_out) != 0)
            return 1;
        }
        a = b;
        b = c;
        p += (uint32_t)(extra * 2);
        p += (uint32_t)(user * 2);
        flip = ~flip;
      }
    }
  }
}
HW_EXPORT(8c014a94, RET(cnk_model_under(ARG0, ARG1, ARG2, ARG3)))

/* was func_8C014E20: is `point` over a triangle of the chunk model tree from `object` (the node's model, its
 * children unless NJD_EVAL_BREAK, then its siblings; the first hit ends it with 1)? */
int32_t cnk_tree_under(gaddr point, gaddr object, gaddr height_out, gaddr normal_out) {
  const NjObject *o = (const NjObject *)hw_ptr(object);
  uint32_t flags = o->evalflags;
  if (o->model != 0 && cnk_model_under(point, o->model, height_out, normal_out) != 0)
    return 1;
  if ((flags & NJD_EVAL_BREAK) == 0 && cnk_tree_under(point, o->child, height_out, normal_out) != 0)
    return 1;
  if (o->sibling != 0 && cnk_tree_under(point, o->sibling, height_out, normal_out) != 0)
    return 1;
  return 0;
}
HW_EXPORT(8c014e20, RET(cnk_tree_under(ARG0, ARG1, ARG2, ARG3)))
