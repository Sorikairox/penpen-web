/* Pen Pen port, S13: a body's contact list (core/contact.c).
 *
 * An object that collides keeps one of these: room for `capacity` contacts, each with the other body's velocity,
 * mass and the contact's restitution, normal and depth, and a handler to call. The default response (contact_respond)
 * pushes the body out along the normal and bounces its velocity like a collision between two masses. Field names
 * are what the response does with them; the words at 0x00-0x20 other than mass are only initialised here. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"

typedef struct Contacts {
  int32_t unknown00;     /* 0x00: 0 */
  gaddr owner;           /* 0x04 */
  f32 radius;            /* 0x08 */
  int32_t unknown0c;     /* 0x0C: -1 */
  int32_t unknown10;     /* 0x10: 0 */
  f32 mass;              /* 0x14: this body's mass (1) */
  f32 unknown18;         /* 0x18: 1 */
  int32_t unknown1c;     /* 0x1C */
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
  gaddr unknown48;       /* 0x48: a word per contact */
  gaddr unknown4c;       /* 0x4C: a word per contact */
  gaddr handler;         /* 0x50: per contact, a function (self, index, a, b, c) or 0 */
} Contacts;
HW_OFFSET(Contacts, respond, 0x24);
HW_OFFSET(Contacts, other, 0x30);
HW_OFFSET(Contacts, handler, 0x50);
HW_SIZE(Contacts, 0x54);

static inline Contacts *contacts_at(gaddr a) { return (Contacts *)hw_ptr(a); }

#define CONTACT_RESPOND 0x8C014E88u /* contact_respond */
#define VEC3_CTOR       0x8C013B66u /* an empty constructor (method_empty) */

/* func_8C062790: the C++ runtime's array constructor (runs ctor on n elements of `size` bytes); returns the array */
static gaddr array_new(int32_t n, uint32_t size, gaddr array, gaddr ctor) { return CALL4(0x8C062790u, n, size, array, ctor); }

/* an array of Vec3, constructed (0 when the allocation failed) */
static gaddr vec3_array_new(int32_t capacity, int32_t n) {
  gaddr p = syMalloc((uint32_t)n * 12);
  return p != 0 ? array_new(capacity, 12, p, VEC3_CTOR) : 0;
}

/* was func_8C013BBE: the constructor; the arrays are allocated only when capacity is not 0 (for at least one
 * element, except the handlers) */
gaddr contacts_construct(gaddr self, gaddr owner, f32 radius, int32_t capacity) {
  Contacts *c = contacts_at(self);
  c->unknown00 = 0;
  c->owner = owner;
  c->radius = radius;
  c->unknown0c = -1;
  c->unknown10 = 0;
  c->mass = F32(1.0f);
  c->unknown18 = F32(1.0f);
  c->unknown1c = 0;
  c->unknown20 = 0;
  c->respond = CONTACT_RESPOND;
  c->capacity = capacity;
  c->count = 0;
  if (capacity == 0)
    return self;
  int32_t n = c->capacity != 0 ? c->capacity : 1;
  c->other = syMalloc((uint32_t)n * 4);
  c->velocity = vec3_array_new(c->capacity, n);
  c->other_mass = syMalloc((uint32_t)n * 4);
  c->restitution = syMalloc((uint32_t)n * 4);
  c->normal = vec3_array_new(c->capacity, n);
  c->depth = syMalloc((uint32_t)n * 4);
  c->unknown48 = syMalloc((uint32_t)n * 4);
  c->unknown4c = syMalloc((uint32_t)n * 4);
  c->handler = syMalloc((uint32_t)c->capacity * 4);
  return self;
}
HW_EXPORT(8c013bbe, RET(contacts_construct(ARG0, ARG1, FARG0, (int32_t)ARG2)))

/* was func_8C013D38: frees the arrays (when there are any) */
void contacts_free(gaddr self) {
  Contacts *c = contacts_at(self);
  if (c->capacity == 0)
    return;
  const gaddr arrays[] = {c->other, c->velocity, c->other_mass, c->restitution, c->normal, c->depth, c->unknown48,
                          c->unknown4c};
  for (unsigned i = 0; i < sizeof arrays / sizeof arrays[0]; i++)
    if (arrays[i] != 0)
      syFree(arrays[i]);
  syFree(c->handler);
}
HW_EXPORT(8c013d38, contacts_free(ARG0))

/* was func_8C013DCE: calls each contact's handler (self, index, a, b, c) */
void contacts_call_handlers(gaddr self, uint32_t a, uint32_t b, uint32_t c3) {
  for (int32_t i = 0; i < contacts_at(self)->count; i++) {
    gaddr fn = U32_AT(contacts_at(self)->handler + 4u * (uint32_t)i);
    if (fn != 0)
      CALL5(fn, self, i, a, b, c3);
  }
}
HW_EXPORT(8c013dce, contacts_call_handlers(ARG0, ARG1, ARG2, ARG3))

/* the three scratch vectors of the response, in the order of the original's frame */
typedef struct RespondScratch {
  Vec3 other;     /* the other body's velocity along the normal */
  Vec3 own;       /* this body's velocity along the normal */
  Vec3 push;      /* normal * depth */
} RespondScratch;
HW_SIZE(RespondScratch, 36);

/* was func_8C014E88: the default response to contact i for `body` (a Body: position, velocity): the position moves
 * out by depth along the normal; along the normal the velocity becomes
 *   v (e (m - m2) / (m + m2) - 1) + u 2 m2 e / (m + m2)   added to the velocity,
 * with v this body's velocity along the normal, u the other's, m, m2 the masses, e the restitution */
void contact_respond(gaddr self, int32_t i, gaddr body) {
  const Contacts *c = contacts_at(self);
  uint32_t k = (uint32_t)i;
  gaddr normal = c->normal + 12 * k;
  gaddr s = hw_stack_alloc(sizeof(RespondScratch));
  gaddr other = s, own = s + 12, push = s + 24;
  gaddr velocity = body + (gaddr)__builtin_offsetof(Body, velocity);

  vec3_scaled(F32_AT(c->depth + 4 * k), push, normal);
  vec3_add(body, push);
  f32 inv_total = f32_reciprocal(fadd(c->mass, F32_AT(c->other_mass + 4 * k)));
  f32 m2 = F32_AT(c->other_mass + 4 * k);
  f32 twice_m2 = fmul(m2, F32(2.0f));
  f32 diff = fsub(c->mass, m2);
  vec3_scaled(njInnerProduct(normal, velocity), own, normal);
  vec3_scaled(njInnerProduct(normal, c->velocity + 12 * k), other, normal);
  diff = fmul(diff, inv_total);
  vec3_scale(fmac(F32_AT(c->restitution + 4 * k), diff, F32(-1.0f)), own);
  twice_m2 = fmul(twice_m2, inv_total);
  vec3_scale(fmul(twice_m2, F32_AT(c->restitution + 4 * k)), other);
  vec3_add(velocity, own);
  vec3_add(velocity, other);
  hw_stack_free(sizeof(RespondScratch));
}
HW_EXPORT(8c014e88, contact_respond(ARG0, (int32_t)ARG1, ARG2))
