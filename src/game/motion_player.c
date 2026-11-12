/* Pen Pen port, S14 (b07): the penguins' motion player (game/motion.h): its constructor, the call that starts a
 * motion set and the per-update step of the frame (0x8C037D24-0x8C0385CC).
 *
 * A motion set is an array of MotionRequests; the player plays the current request's motion and, by the request's
 * flags, loops it, holds its end, replays the set or goes on to the next request of the array. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/file.h"
#include "motion.h"

/* the model files of the penguins: two names (variants) per character */
#define MOTION_MODEL_NAMES 0x8C097254u
/* the motion sets of the race (a model the player loads), of the menus (a container, table 0) and of the other
 * container table (1) */
#define MOTION_SETS_RACE   0x8C097294u
#define MOTION_SETS_MENU   0x8C09731Cu
#define MOTION_SETS_OTHER  0x8C097344u
enum {
  RACE_FIRST_SET = 0x22,     /* the set noted at construction (motion_count) */
  RACE_START_SET = 10,       /* the set started */
  RACE_FIRST_MOTION = 0x15,
};

/* the parts' models of the shared container (the effects' data, game/model.c), noted for the draw */
#define g_texlist_word     GUEST_VAR(uint32_t, 0x8C1B7E40u) /* model.c's g_word_8c1b7e40 */
#define g_draw_texlist     GUEST_VAR(uint32_t, 0x8C096B9Cu) /* the texture list the player draws with */
#define g_parts_container  GUEST_VAR(gaddr, 0x8C1B7E48u)    /* a loaded container (its entries 1-6) */
#define g_part_models      (&GUEST_VAR(gaddr, 0x8C1BAC1Cu)) /* four: its entries 1-4 */
#define g_part_model_5     GUEST_VAR(gaddr, 0x8C096BB0u)    /* its entry 5 */
#define g_part_model_6     GUEST_VAR(gaddr, 0x8C096BB4u)    /* its entry 6 */
#define PART_MODELS 4

/* look numbers of the characters (motion.h looks[0]) */
enum { CHARACTER_0_LOOK = 10, CHARACTER_3_LOOK = 0x1A };

/* func_8C03851E (game/model.c, not in a header): start a request */
static void motion_player_start(gaddr player, gaddr request) { CALL2(0x8C03851Eu, player, request); }

static const ContainerEntry *container_entries(gaddr c) { return (const ContainerEntry *)hw_ptr(c); }

/* was func_8C0385AC: play motion set `set` of the player's table at `speed` (from its first request) */
void motion_player_play_set(gaddr player, int32_t set, f32 speed) {
  MotionPlayer *p = motion_player_at(player);
  p->motion_count = set;
  p->speed = speed;
  motion_player_start(player, ((const gaddr *)hw_ptr(p->motion_table))[set]);
}
HW_EXPORT(8c0385ac, motion_player_play_set(ARG0, (int32_t)ARG1, FARG0))

/* was func_8C037D24: the constructor. With a container, its first three entries are the model, the motions and the
 * texture list (table 0: the menus' sets, playing the character's own; 1: the other table); without, the
 * character's model file is loaded (the race's sets). Then the four look numbers, the timers, and the parts'
 * models of the shared container are noted; returns the player */
gaddr motion_player_build(gaddr player, int32_t character, int32_t variant, gaddr container, int32_t table) {
  MotionPlayer *p = motion_player_at(player);
  p->character = character;
  p->variant = variant;
  if (container != 0) {
    if (table == 0) {
      p->motion_table = MOTION_SETS_MENU;
      p->motion_count = p->character;
      p->request = 0;
      p->index = 1;
    } else if (table == 1) {
      p->motion_table = MOTION_SETS_OTHER;
      p->motion_count = 0;
      p->request = 0;
      p->prev_index = 0;
      p->index = 0;
    }
    p->frame = F32(0.0f);
    p->speed = F32(1.0f);
    motion_player_play_set(player, p->motion_count, F32(1.0f));
    p->model = (gaddr)container_entries(container)[0].offset;
    p->motions = (gaddr)container_entries(container)[1].offset;
    p->texlist = (gaddr)container_entries(container)[2].offset;
    p->file = 0;
  } else {
    const gaddr *names = (const gaddr *)hw_ptr(MOTION_MODEL_NAMES);
    p->file = container_load(names[2 * p->character + p->variant], 0);
    p->motion_table = MOTION_SETS_RACE;
    p->motion_count = RACE_FIRST_SET;
    p->request = 0;
    p->index = RACE_FIRST_MOTION;
    p->frame = F32(0.0f);
    p->speed = F32(1.0f);
    motion_player_play_set(player, RACE_START_SET, F32(1.0f));
    p->model = (gaddr)container_entries(p->file)[0].offset;
    p->motions = (gaddr)container_entries(p->file)[1].offset;
    p->texlist = (gaddr)container_entries(p->file)[2].offset;
  }
  if (variant != 0) {
    p->looks[0] = 0;
    p->looks[1] = 0;
    p->looks[2] = 1;
  } else if (character == 0) {
    p->looks[0] = CHARACTER_0_LOOK;
    p->looks[1] = 0;
    p->looks[2] = 1;
  } else if (character == 2) {
    p->looks[0] = 0;
    p->looks[1] = 0;
    p->looks[2] = 2;
  } else {
    p->looks[0] = character == 3 ? CHARACTER_3_LOOK : 0;
    p->looks[1] = 0;
    p->looks[2] = 1;
  }
  p->looks[3] = p->looks[2];
  for (int i = 0; i < 4; i++) p->timers[i] = F32(0.0f);
  p->word14 = 1;
  g_draw_texlist = g_texlist_word;
  for (int i = 0; i < PART_MODELS; i++)
    g_part_models[i] = (gaddr)container_entries(g_parts_container)[1 + i].offset;
  g_part_model_5 = (gaddr)container_entries(g_parts_container)[5].offset;
  g_part_model_6 = (gaddr)container_entries(g_parts_container)[6].offset;
  p->draw_l2 = 0;
  return player;
}
HW_EXPORT(8c037d24, RET(motion_player_build(ARG0, (int32_t)ARG1, (int32_t)ARG2, ARG3, (int32_t)ARG4)))

/* the motion's last frame, as SHC converts the unsigned count */
static f32 motion_last_frame(const NjMotion *m) { return f32_from_uint((uint32_t)(m->nb_frames - 1)); }

/* was func_8C037F86: one update: blend towards the new motion, or move the frame by the request's speed; then, by
 * the request's flags, keep the frame in the motion (wrap, hold, replay the set, go on to the next request, count
 * a loop down); the four timers count up */
void motion_player_advance(gaddr player) {
  MotionPlayer *p = motion_player_at(player);
  p->ended = 0;
  if (p->prev_index != p->index)
    p->blend = fadd(p->blend, p->blend_speed);
  else
    p->frame = fmac(((const MotionRequest *)hw_ptr(p->request))->speed, p->speed, p->frame);
  if (p->request != 0) {
    const MotionRequest *r = (const MotionRequest *)hw_ptr(p->request);
    const NjMotion *m = (const NjMotion *)hw_ptr(((const MotionEntry *)hw_ptr(p->motions))[r->index].motion);
    const f32 zero = F32(0.0f);
    switch (p->flags) {
    case -2: /* hold the end */
    case -3: /* hold, then the set again */
      if (fgt(p->frame, motion_last_frame(m))) {
        p->frame = motion_last_frame(m);
      } else if (fgt(zero, p->frame)) {
        p->frame = zero;
      } else {
        break;
      }
      p->ended = 1;
      if (p->flags == -3) motion_player_play_set(player, p->motion_count, F32(1.0f));
      break;
    case 0: /* the next request now */
      p->frame = zero;
      p->request += sizeof(MotionRequest);
      motion_player_start(player, p->request);
      break;
    case -1: /* loop */
      if (fgt(p->frame, motion_last_frame(m)))
        p->frame = fsub(p->frame, motion_last_frame(m));
      else if (fgt(zero, p->frame))
        p->frame = fadd(p->frame, motion_last_frame(m));
      break;
    case 1: /* the next request at either end */
      if (fgt(p->frame, motion_last_frame(m))) {
        p->frame = motion_last_frame(m);
      } else if (fgt(zero, p->frame)) {
        p->frame = zero;
      } else {
        break;
      }
      p->request += sizeof(MotionRequest);
      motion_player_start(player, p->request);
      break;
    default: /* n more times */
      if (fgt(p->frame, motion_last_frame(m))) {
        p->flags--;
        p->frame = zero;
      }
      break;
    }
  }
  for (int i = 0; i < 4; i++) p->timers[i] = fadd(p->timers[i], F32(1.0f));
}
HW_EXPORT(8c037f86, motion_player_advance(ARG0))
