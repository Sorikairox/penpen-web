/* Pen Pen port, S13/S14: a body's contact list (core/contact.c; S14 moved the struct here for its users: the racer,
 * the actors, the race scene's slot list, which chains these through their first word). */
#ifndef PENPEN_CONTACT_H
#define PENPEN_CONTACT_H
#include "vec.h"

typedef struct Contacts {
  gaddr next;            /* 0x00: 0; the next Contacts of its slot in the race scene's SlotList */
  gaddr owner;           /* 0x04 */
  f32 radius;            /* 0x08 */
  int32_t cell;          /* 0x0C: -1; the course cell of the owner (an actor sets its own) */
  gaddr owner_velocity;  /* 0x10: 0; the owner's velocity (Vec3) */
  f32 mass;              /* 0x14: this body's mass (1) */
  f32 unknown18;         /* 0x18: 1 */
  int32_t unknown1c;     /* 0x1C: 0 (a chain sets 4) */
  int32_t unknown20;     /* 0x20 */
  gaddr respond;         /* 0x24: the response, contact_respond (func_8C014E88) */
  int32_t capacity;      /* 0x28 */
  int32_t count;         /* 0x2C: contacts in use */
  gaddr other;           /* 0x30: a word per contact */
  gaddr velocity;        /* 0x34: Vec3 per contact: the other body's velocity */
  gaddr other_mass;      /* 0x38: f32 per contact */
  gaddr restitution;     /* 0x3C: f32 per contact */
  gaddr normal;          /* 0x40: Vec3 per contact (unit) */
  gaddr depth;           /* 0x44: f32 per contact: how far to push out */
  gaddr unknown48;       /* 0x48: a word per contact (S16 w4: the kind its adder gave it, contacts_add_triangle) */
  gaddr unknown4c;       /* 0x4C: a word per contact */
  gaddr handler;         /* 0x50: per contact, a function (self, index, a, b, c) or 0 */
} Contacts;
HW_OFFSET(Contacts, respond, 0x24);
HW_OFFSET(Contacts, other, 0x30);
HW_OFFSET(Contacts, handler, 0x50);
HW_SIZE(Contacts, 0x54);

static inline Contacts *contacts_at(gaddr a) { return (Contacts *)hw_ptr(a); }

/* core/contact.c */
gaddr contacts_construct(gaddr self, gaddr owner, f32 radius, int32_t capacity); /* S14 b05 declares it */
void contacts_free(gaddr self);

/* S16 w4: core/contact_triangle.c (was func_8C013E1C) */
void contacts_add_triangle(gaddr self, gaddr a, gaddr b, gaddr c, uint32_t kind);

/* S18 b01: core/contact_models.c: one model's triangles into the contacts, and the ground under a point */
void contacts_add_model(gaddr contacts, gaddr model);              /* was func_8C01431E: a basic model */
void contacts_add_cnk_model(gaddr contacts, gaddr model);          /* was func_8C0144F2: a chunk model as stored */
void contacts_add_cnk_model_posed(gaddr contacts, gaddr model);    /* was func_8C0146F8: placed by the matrix */
void contacts_add_object_posed(gaddr contacts, gaddr object);      /* was func_8C0148C4: a chunk model tree */
int32_t cnk_triangle_under(gaddr point, gaddr a, gaddr b, gaddr c, gaddr height_out, gaddr normal_out); /* 8C014980 */
int32_t cnk_model_under(gaddr point, gaddr model, gaddr height_out, gaddr normal_out);   /* was func_8C014A94 */
int32_t cnk_tree_under(gaddr point, gaddr object, gaddr height_out, gaddr normal_out);  /* was func_8C014E20 */

void contacts_holder_destroy(gaddr obj);
gaddr contacts_holder_destroy_self(gaddr obj);

#endif
