/* Pen Pen port, S16 (b06): the update of the actor of placement type 5 (Pieces, game/actor.h; func_8C047006).
 *
 * Fifteen pieces (a rock face) wait until a player reaches course cell 0x77. Then each piece, while it waits
 * (state 0), keeps its tip pulled upright and counts its fall delay down (unless the actor is held); at 0 it is
 * kicked loose (piece_kick). A falling piece (state 1) feels gravity, lands at the floor height (-116.5; its first
 * landing may scatter it sideways), rolls (its angles turn with its speed), runs its contact handlers, and is gone
 * (state 2) once it leaves cells 120-128. A piece's contacts sound (0x27 when one is a racer, else 0x28) in every
 * view within a cell of the actor, at most every 60 updates. One piece's cell is refreshed per update; when the
 * actor's own contacts touch something every waiting piece falls on the next update. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "game.h"         /* panel_get */
#include "actor.h"
#include "race_scene.h"   /* race_setup_get, race_scene_slot_insert */
#include "scenery.h"      /* race_player_cell, race_scene_sound, race_scene_find_cell, contacts_call_handlers */

#define PIECES_START_CELL  0x77
#define PIECE_TIP_RATE     F32_BITS(0x3CF5C28Fu) /* 0.03 */
#define PIECE_GRAVITY      F32_BITS(0xBDA740D9u) /* -0.0816667 (-9.8 / 120) */
#define PIECE_FLOOR        F32_BITS(0xC2E8FFFFu) /* -116.49999 */
#define PIECE_SCATTER      F32_BITS(0x3ECCCCCCu) /* 0.39999998: per step of the index's bits */
#define PIECE_ROLL         F32(869.0f)           /* angle units per unit of speed */
#define PIECE_CELL_FIRST   120                   /* a piece outside cells 120-128 is gone */
#define PIECE_CELL_LAST    128
#define RACER_ID_LIMIT     8                     /* contacts with an id below this are racers */
#define PIECE_SOUND_GAP    60
#define SOUND_PIECE_RACER  0x27
#define SOUND_PIECE_OTHER  0x28
#define PIECES_SLOT        8                     /* the race scene's slot list of the actor's contacts */

enum { PIECE_WAIT = 0, PIECE_FALL = 1 };

/* func_8C046BA4: the course cell under view i's camera (-1: the current view) */

/* a falling piece's contacts sound in the views near the actor */
static void piece_sound(gaddr obj, Piece *p) {
  const Pieces *a = (const Pieces *)hw_ptr(obj);
  int32_t racer = 0;
  for (int32_t j = 0; j < p->contacts.count; j++) {
    if (((const int32_t *)hw_ptr(p->contacts.other))[j] < RACER_ID_LIMIT) {
      racer = 1;
      break;
    }
  }
  for (int32_t view = 0; view < (int32_t)a->base.word0c; view++) {
    int32_t cell = a->base.cell;
    int32_t d = cell - race_scene_view_cell(panel_get(), view);
    if (!fgt(f32_abs(f32_from_int(d)), F32(1.0f))) {
      race_scene_sound(panel_get(), view, 0, 8, racer ? SOUND_PIECE_RACER : SOUND_PIECE_OTHER);
      p->sound_timer = PIECE_SOUND_GAP;
    }
  }
}

/* piece k's update while it falls */
static void piece_fall(gaddr obj, int32_t k) {
  gaddr piece = member_addr(obj, Pieces, pieces[k]);
  Piece *p = (Piece *)hw_ptr(piece);
  f32_add_to(PIECE_GRAVITY, member_addr(piece, Piece, body.force.y));
  if (fgt(PIECE_FLOOR, p->body.position.y)) {
    p->body.position.y = PIECE_FLOOR;
    f32 half_fall = fmul(p->body.velocity.y, F32(0.5f));
    f32_add_to(fneg(half_fall), member_addr(piece, Piece, body.force.y));
    p->body.velocity.y = F32(0.0f);
    if (p->word50 != 0) {
      p->body.velocity.x = fmul(f32_from_int(k & 3), PIECE_SCATTER);
      p->body.velocity.z = fmul(f32_from_int((k & 6) >> 1), PIECE_SCATTER);
      p->word50 = 0;
    }
  }
  f32 vz = p->body.velocity.z;
  f32 vx = p->body.velocity.x;
  p->angle.x = fmac(PIECE_ROLL, vz, p->angle.x);
  p->angle.z = fmac(PIECE_ROLL, vx, p->angle.z);
  body_step(piece);
  contacts_call_handlers(member_addr(piece, Piece, contacts), piece, 0, 0);
  if (p->cell < PIECE_CELL_FIRST || p->cell > PIECE_CELL_LAST)
    p->state++;
  if (p->contacts.count != 0) {
    if (p->sound_timer == 0)
      piece_sound(obj, p);
  } else if (p->sound_timer != 0) {
    p->sound_timer--;
  }
  race_scene_slot_insert(panel_get(), member_addr(piece, Piece, contacts), -1);
}

/* was func_8C047006: the update (see the top of the file) */
void pieces_update(gaddr obj) {
  Pieces *a = (Pieces *)hw_ptr(obj);
  switch (a->state) {
  case 0: {
    int32_t found = 0;
    int32_t players = race_setup_at(race_setup_get())->player_count;
    for (int32_t i = 0; i < players; i++) {
      if ((int32_t)race_player_cell(panel_get(), i) >= PIECES_START_CELL) {
        found = 1;
        break;
      }
    }
    if (found)
      a->state++;
    break;
  }
  case 1: {
    for (int32_t k = 0; k < PIECE_COUNT; k++) {
      Piece *p = &a->pieces[k];
      switch (p->state) {
      case PIECE_WAIT:
        body1_pull_angle(F32(0.0f), PIECE_TIP_RATE, member_addr(obj, Pieces, pieces[k].tip));
        body1_step(member_addr(obj, Pieces, pieces[k].tip));
        if (a->held == 0) {
          p->word58--;
          if (p->word58 == 0) {
            p->state++;
            piece_kick(obj, k);
          }
        }
        break;
      case PIECE_FALL:
        piece_fall(obj, k);
        break;
      default:
        break;
      }
    }
    if (a->cell_turn < PIECE_COUNT) {
      Piece *p = &a->pieces[a->cell_turn];
      if (p->state == PIECE_FALL) {
        gaddr piece = member_addr(obj, Pieces, pieces[a->cell_turn]);
        int32_t hint = p->cell;
        p->cell = race_scene_find_cell(a->base.scene, piece, hint);
        p->contacts.cell = p->cell;
      }
      a->cell_turn++;
    } else {
      a->cell_turn = 0;
    }
    if (a->contacts.count != 0) {
      for (int32_t k = 0; k < PIECE_COUNT; k++)
        if (a->pieces[k].word58 != 0)
          a->pieces[k].word58 = 1;
    }
    race_scene_slot_insert(panel_get(), member_addr(obj, Pieces, contacts), PIECES_SLOT);
    break;
  }
  default:
    break;
  }
}
HW_EXPORT(8c047006, pieces_update(ARG0))
