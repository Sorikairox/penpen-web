/* Pen Pen port, S19 (u): the init method of the type-5 actor (Pieces, game/actor.h; vtable 0x8C23DCCC, slot 1):
 * its fifteen pieces from a table, the actor's own Body and contacts at the placement. */
#include "hw.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "actor.h"
#include "race_scene.h"
#include "course.h"

/* a piece's start as the table holds it (15 records of 0x1C bytes) */
typedef struct PieceStart {
  uint32_t delay; /* 0x00: updates before it falls (0: it does not wait; else 300 more) */
  Vec3 position;  /* 0x04 */
  Vec3 angle;     /* 0x10 */
} PieceStart;
HW_OFFSET(PieceStart, angle, 0x10);
HW_SIZE(PieceStart, 0x1C);
#define PIECE_STARTS 0x8C098B6Cu /* PieceStart[PIECE_COUNT] */

#define PIECE_EXTRA_DELAY 300
#define PIECE_MASS F32_BITS(0x3ECCCCCCu)       /* 0.39999998 */
#define PIECE_CONTACTS_1C 0xCD
#define PIECE_TIP F32_BITS(0x45AAAAAAu)        /* 5461.333: a twelfth of a turn, Ninja units */
#define PIECE_DAMPING F32_BITS(0x3F7FBE76u)    /* 0.999 */
#define PIECE_ALT_FROM 7                        /* pieces 7 and on use the other model */
#define CONTACTS_WORD_INIT 4

/* was func_8C046D78: the init: each piece at rest at its table place and angle, on its course cell, its contacts
 * (mass 0.4) following its velocity, its tip at a twelfth of a turn, damped by 0.999; a piece with a delay waits (and
 * scatters when it lands), else it falls now (kicked) unless the actor is held (param), then it is gone. The actor's
 * Body at the placement with no damping, its contacts on its cell. */
void pieces_init(gaddr obj, gaddr placement) {
  Pieces *a = (Pieces *)hw_ptr(obj);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  const PieceStart *start = (const PieceStart *)hw_ptr(PIECE_STARTS);
  a->held = (int32_t)p->param;
  for (int32_t i = 0; i < PIECE_COUNT; i++, start++) {
    Piece *pc = &a->pieces[i];
    gaddr piece = member_addr(obj, Pieces, pieces[i]);
    pc->word58 = start->delay;
    if (start->delay != 0)
      pc->word58 += PIECE_EXTRA_DELAY;
    body_reset(piece);
    pc->body.position.x = start->position.x;
    pc->body.position.y = start->position.y;
    pc->body.position.z = start->position.z;
    pc->angle.x = start->angle.x;
    pc->angle.y = start->angle.y;
    pc->angle.z = start->angle.z;
    pc->cell = course_cell_search(race_scene_at(a->base.scene)->course, piece);

    Contacts *c = &pc->contacts;
    c->count = 0;
    pc->contacts_word = CONTACTS_WORD_INIT;
    c->owner_velocity = member_addr(piece, Body, velocity);
    c->mass = PIECE_MASS;
    c->unknown18 = F32(1.0f);
    c->cell = pc->cell;
    c->unknown1c = PIECE_CONTACTS_1C;

    pc->tip.value = F32(0.0f);
    pc->tip.velocity = F32(0.0f);
    pc->tip.force = F32(0.0f);
    pc->tip.damping = F32(1.0f);
    pc->tip.value = PIECE_TIP;
    pc->tip.damping = F32(1.0f);
    vec3_fill(member_addr(piece, Body, damping), PIECE_DAMPING);

    if (pc->word58 != 0) {
      pc->state = 0;
      pc->word50 = 1;
    } else if (a->held == 0) {
      pc->state = 1;
      piece_kick(obj, i);
      pc->word50 = 0;
    } else {
      pc->state = 2;
    }
    pc->alt_model = i >= PIECE_ALT_FROM ? 1 : 0;
    pc->sound_timer = 0;
  }
  a->base.flags = 1;
  a->state = 0;
  a->cell_turn = 0;
  a->word_bc8 = 0;

  gaddr body = member_addr(obj, Pieces, body);
  body_reset(body);
  a->body.position.x = p->position.x;
  a->body.position.y = p->position.y;
  a->body.position.z = p->position.z;
  vec3_fill(member_addr(body, Body, damping), F32(0.0f));
  Contacts *c = &a->contacts;
  c->count = 0;
  a->contacts_word = CONTACTS_WORD_INIT;
  c->mass = F32(1.0f);
  c->unknown18 = F32(1.0f);
  c->cell = a->base.cell;
}
HW_EXPORT(8c046d78, pieces_init(ARG0, ARG1))
