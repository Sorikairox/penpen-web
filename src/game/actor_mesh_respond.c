/* Pen Pen port, S18 b05: two contact responses that test the touching body against the other actor's collision
 * mesh: the turning gate's (type 0x17, game/actor.h: TurningGate; game/actor_gates.c installs it as GATE_RESPOND)
 * and type 0x2D's (game/actor.h: ActorType2D; game/actor_types.c installs it as TYPE2D_RESPOND). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/inline.h"
#include "../core/ninja.h"
#include "actor.h"


#define GATE_MESH_RADIUS   F32_BITS(0x40FFFFFFu) /* 7.9999995: the body's sphere against the gate's mesh */
#define GATE_MESH_CAPACITY 8
#define GATE_MESH_MODEL    56                    /* the gate's collision model */
#define TYPE2D_MESH_MODEL  5                     /* type 0x2D's */
#define GATE_BOUNCE        F32(1.0f)             /* the normal speed taken off, times this */

/* the original's frame (0x8C bytes under PR): the Contacts and the push go to the contact code */
typedef struct GateRespondFrame {
  f32 speed;         /* 0x00: the body's speed into the contact's normal */
  Vec3 push;         /* 0x04 */
  int32_t k;         /* 0x10 */
  f32 bounce;        /* 0x14 */
  gaddr normal;      /* 0x18 */
  gaddr depth;       /* 0x1C */
  Contacts mesh;     /* 0x20: the body against the gate's mesh */
  uint32_t word74;   /* 0x74 */
  gaddr gate;        /* 0x78 */
  uint32_t args[4];  /* 0x7C: r7, body, i, contacts (spilled) */
  uint32_t saved_pr; /* 0x8C */
} GateRespondFrame;
HW_OFFSET(GateRespondFrame, push, 0x04);
HW_OFFSET(GateRespondFrame, mesh, 0x20);
HW_OFFSET(GateRespondFrame, gate, 0x78);
HW_SIZE(GateRespondFrame, 0x90);

/* the response's work: the contact's other object is the actor; a Contacts of the body (radius 8, room for 8) is
 * tested against the actor's collision model at `position` turned by `angle` about y; for each hit the body is
 * pushed out by the depth along the normal and, moving into it, loses its speed along the normal; the Contacts is
 * freed (the 4th argument is not used) */
static void mesh_respond(gaddr contacts, int32_t i, gaddr body, uint32_t unused, uint32_t position_offset,
                         uint32_t angle_offset, int32_t model) {
  const Contacts *c = contacts_at(contacts);
  gaddr frame = hw_stack_alloc(sizeof(GateRespondFrame));
  GateRespondFrame *f = (GateRespondFrame *)hw_ptr(frame);
  gaddr mesh = member_addr(frame, GateRespondFrame, mesh);
  gaddr push = member_addr(frame, GateRespondFrame, push);
  f->args[0] = unused;
  f->args[1] = body;
  f->args[2] = (uint32_t)i;
  f->args[3] = contacts;
  f->gate = ((const gaddr *)hw_ptr(c->unknown4c))[i];
  const Actor *other = (const Actor *)hw_ptr(f->gate);
  contacts_construct(mesh, method_self(body), GATE_MESH_RADIUS, GATE_MESH_CAPACITY);
  matrix_push();
  matrix_reset();
  matrix_translate_v(f->gate + position_offset);
  matrix_rotate_y(*(const f32 *)hw_ptr(f->gate + angle_offset)); /* the unit's inline getter */
  contacts_add_object_posed(mesh, actor_model(other, model));
  matrix_pop();
  const Contacts *m = contacts_at(mesh);
  f->depth = m->depth;
  f->normal = m->normal;
  f->bounce = GATE_BOUNCE;
  for (f->k = 0; f->k < m->count; f->k++, f->depth += sizeof(f32), f->normal += sizeof(Vec3)) {
    method_self(push); /* the Vec3's (empty) constructor */
    vec3_scaled(*(const f32 *)hw_ptr(f->depth), push, f->normal);
    vec3_add(body, push);
    f->speed = fmul(njInnerProduct(f->normal, member_addr(body, Body, velocity)), f->bounce);
    if (!fgt(F32(0.0f), f->speed)) continue;
    vec3_scaled(fneg(f->speed), push, f->normal);
    vec3_add(member_addr(body, Body, velocity), push);
  }
  contacts_free(mesh);
  hw_stack_free(sizeof(GateRespondFrame));
}

/* was func_8C055D78: the turning gate's response to contact i with `body` (a Body): against its model 56 at its
 * position turned by its turn */
void gate_respond(gaddr contacts, int32_t i, gaddr body, uint32_t unused) {
  mesh_respond(contacts, i, body, unused, offsetof(TurningGate, position), offsetof(TurningGate, turn.value),
               GATE_MESH_MODEL);
}
HW_EXPORT(8c055d78, gate_respond(ARG0, (int32_t)ARG1, ARG2, ARG3))

/* was func_8C05CAA8: type 0x2D's response to contact i with `body`: against its model 5 at its position turned by
 * its turn */
void type2d_respond(gaddr contacts, int32_t i, gaddr body, uint32_t unused) {
  mesh_respond(contacts, i, body, unused, offsetof(ActorType2D, position), offsetof(ActorType2D, turn.value),
               TYPE2D_MESH_MODEL);
}
HW_EXPORT(8c05caa8, type2d_respond(ARG0, (int32_t)ARG1, ARG2, ARG3))
