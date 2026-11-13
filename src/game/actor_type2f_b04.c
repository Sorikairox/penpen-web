/* Pen Pen port, S18 b04: the update of placement type 0x2F (ActorType2F, game/actor_b05.h; its init is
 * game/actor_type2f.c). A marker floating at home: the first player to touch it is marked (red, state 0; green from
 * state 1), a second touch by another player passes it on; then it rises (state 3) and, 150 above home, picks the
 * player ranked one before the marked one and rides down onto it (state 4) until they touch (state 5). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "game.h"         /* panel_get */
#include "actor.h"
#include "race_scene.h"   /* race_scene_slot_insert, race_setup_get */
#include "scenery.h"      /* race_player_body, race_player_cell */
#include "actor_b05.h"

#define TYPE2F_PULL       F32_BITS(0x3C23D70Au)  /* 0.01: the body's pull home */
#define TYPE2F_ALPHA      F32_BITS(0x3F199999u)  /* 0.6 */
#define TYPE2F_RISE       F32_BITS(0x3D638E38u)  /* 0.0556: the push up while it rises */
#define TYPE2F_HEIGHT     F32(150.0f)            /* it stops rising this far above home; and rides this high */
#define TYPE2F_DESCENT    F32_BITS(0x3FFFFFFFu)  /* 1.9999999 a step */
#define CONTACT_RESPOND   0x8C014E88u            /* contact_respond (the default response) */

static inline ActorType2F *type2f_at(gaddr a) { return (ActorType2F *)hw_ptr(a); }

/* the last player (below 8) among its contacts' others, but the marked one when `other_than_marked` (-1 if none) */
static int32_t type2f_toucher(const ActorType2F *a, int other_than_marked) {
  int32_t found = -1;
  for (int32_t i = 0; i < a->contacts.count; i++) {
    int32_t other = ((const int32_t *)hw_ptr(a->contacts.other))[i];
    if (other > 7) continue;
    if (other_than_marked && other == a->player) continue;
    found = other;
  }
  return found;
}

/* was func_8C048DE6: the update (see the top of the file); every state then steps the body and offers the contacts
 * to the scene's slot 8 */
void actor_type2f_update(gaddr obj) {
  ActorType2F *a = type2f_at(obj);
  gaddr body = member_addr(obj, ActorType2F, body);
  gaddr contacts = member_addr(obj, ActorType2F, contacts);
  int32_t found;
  switch (a->state) {
  case 0: /* the first toucher is marked: red */
    body_pull(TYPE2F_PULL, body, member_addr(obj, ActorType2F, home));
    found = type2f_toucher(a, 0);
    if (found == -1) break;
    a->player = found;
    argb_set(F32(1.0f), F32(0.0f), F32(0.0f), TYPE2F_ALPHA, member_addr(obj, ActorType2F, color));
    a->state += 2;
    break;
  case 1: /* another toucher is marked: green */
    body_pull(TYPE2F_PULL, body, member_addr(obj, ActorType2F, home));
    found = type2f_toucher(a, 1);
    if (found == -1) break;
    a->player = found;
    argb_set(F32(0.0f), F32(1.0f), F32(0.0f), TYPE2F_ALPHA, member_addr(obj, ActorType2F, color));
    a->state += 1;
    break;
  case 2: /* another toucher is marked */
    body_pull(TYPE2F_PULL, body, member_addr(obj, ActorType2F, home));
    found = type2f_toucher(a, 1);
    if (found == -1) break;
    a->player = found;
    a->state += 1;
    break;
  case 3: { /* rising; at the top, the player ranked one before the marked one */
    f32_add_to(TYPE2F_RISE, member_addr(body, Body, force.y));
    if (!fgt(a->body.position.y, fadd(a->home.y, TYPE2F_HEIGHT))) break;
    a->contacts.unknown1c = 1;
    a->contacts.respond = CONTACT_RESPOND;
    int32_t rank = (int32_t)race_scene_at(panel_get())->player_item[a->player];
    if (rank != 0) {
      rank--;
      int32_t players = race_setup_at(race_setup_get())->player_count;
      for (int32_t i = 0; i < players; i++) {
        if ((int32_t)race_scene_at(panel_get())->player_item[i] == rank) {
          a->player = i;
          break;
        }
      }
    }
    a->lift = TYPE2F_HEIGHT;
    a->state += 1;
    break;
  }
  case 4: { /* riding down onto that player, until they touch */
    vec3_copy_bytes(body, race_player_body(panel_get(), a->player));
    f32_add_to(a->lift, member_addr(body, Body, position.y));
    a->contacts.cell = (int32_t)race_player_cell(panel_get(), a->player);
    a->lift = fsub(a->lift, TYPE2F_DESCENT);
    int32_t touching = 0;
    for (int32_t i = 0; i < a->contacts.count; i++) {
      if (((const int32_t *)hw_ptr(a->contacts.other))[i] == a->player) {
        touching = 1;
        break;
      }
    }
    if (touching) a->state += 1;
    break;
  }
  default: /* 5 */
    break;
  }
  body_step(body);
  race_scene_slot_insert(panel_get(), contacts, 8);
}
HW_EXPORT(8c048de6, actor_type2f_update(ARG0))
