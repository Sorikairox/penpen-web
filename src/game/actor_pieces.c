/* Pen Pen port, S14 b10: the type-5 actor (Pieces, game/actor.h): fifteen pieces, each a Body with a tip and its
 * own contacts, plus a Body and contacts for the whole; their construction and destruction. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "actor.h"
#include "scenery.h"

#define PIECE_DESTRUCTOR  0x8C046AD4u /* piece_destroy, handed to the runtime's array destructor */
#define PIECE_CONSTRUCTOR 0x8C046BDAu /* piece_construct's thunk, handed to the array constructor */

/* was func_8C046AD4: a piece's destructor: its contacts freed */
void piece_destroy(gaddr piece) { contacts_free(member_addr(piece, Piece, contacts)); }
HW_EXPORT(8c046ad4, piece_destroy(ARG0))

/* was func_8C046D0A (and its thunk func_8C046BDA): a piece's constructor: its Body at rest at the origin, its tip a
 * Body1 at rest, its contacts owned by the Body (radius 10, room for 3) */
gaddr piece_construct(gaddr piece) {
  Piece *p = (Piece *)hw_ptr(piece);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(piece, Piece, body.position));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(piece, Piece, body.velocity));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(piece, Piece, body.force));
  vec3_set(F32(1.0f), F32(1.0f), F32(1.0f), member_addr(piece, Piece, body.damping));
  p->tip.value = F32(0.0f);
  p->tip.velocity = F32(0.0f);
  p->tip.force = F32(0.0f);
  p->tip.damping = F32(1.0f);
  contacts_construct(member_addr(piece, Piece, contacts), piece, F32(10.0f), 3);
  return piece;
}
HW_EXPORT(8c046d0a, RET(piece_construct(ARG0)))
HW_EXPORT(8c046bda, RET(piece_construct(ARG0)))

/* was func_8C046BDE: the type-5 actor's base construction: the Actor, the 15 pieces, the Body at rest at the
 * origin and its contacts (radius 15.999999, room for 4) */
gaddr pieces_base_construct(gaddr obj) {
  actor_construct(obj);
  array_construct(PIECE_COUNT, sizeof(Piece), member_addr(obj, Pieces, pieces), PIECE_CONSTRUCTOR);
  gaddr body = member_addr(obj, Pieces, body);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, position));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, velocity));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, force));
  vec3_set(F32(1.0f), F32(1.0f), F32(1.0f), member_addr(body, Body, damping));
  contacts_construct(member_addr(obj, Pieces, contacts), body, F32_BITS(0x417FFFFFu) /* 15.999999 */, 4);
  return obj;
}
HW_EXPORT(8c046bde, RET(pieces_base_construct(ARG0)))

/* was func_8C046C50: type 5's constructor (the factory's): both vtables, then the base */
gaddr pieces_construct(gaddr obj) {
  Pieces *p = (Pieces *)hw_ptr(obj);
  p->base.vtable = 0x8C23DCCCu;
  p->second_vtable = 0x8C23DCECu;
  pieces_base_construct(obj);
  return obj;
}
HW_EXPORT(8c046c50, RET(pieces_construct(ARG0)))

/* was func_8C046CB0: the destructor body: the contacts freed, then the 15 pieces destroyed */
void pieces_destroy(gaddr obj) {
  contacts_free(member_addr(obj, Pieces, contacts));
  array_destroy(PIECE_COUNT, sizeof(Piece), member_addr(obj, Pieces, pieces), PIECE_DESTRUCTOR);
}
HW_EXPORT(8c046cb0, pieces_destroy(ARG0))

/* was func_8C046CD4: the deleting destructor */
gaddr pieces_delete(gaddr obj, int32_t free_memory) {
  pieces_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c046cd4, RET(pieces_delete(ARG0, (int32_t)ARG1)))
