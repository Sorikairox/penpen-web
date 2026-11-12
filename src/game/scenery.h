/* Pen Pen port, S14 b10: what the scenery actor classes of the other cups' courses share (game/actor_*.c): the
 * hand-written functions they call that no header declares yet, and small wrappers of the guest code (SHC runtime,
 * still-translated game functions) they call. */
#ifndef PENPEN_SCENERY_H
#define PENPEN_SCENERY_H

#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"

/* hand-written elsewhere, not in a header yet */
gaddr contacts_construct(gaddr self, gaddr owner, f32 radius, int32_t capacity);          /* core/contact.c */
void contacts_call_handlers(gaddr self, uint32_t a, uint32_t b, uint32_t c3);            /* core/contact.c */
f32 random_timer_scaled(f32 range);                                                       /* core/random.c */
f32 random_scaled(f32 range);                                                             /* core/random.c */
int32_t motion_draw_l1(gaddr object, gaddr motion, gaddr frame, int32_t loop);            /* core/motion.c */
int32_t shape_motion_draw_l1(gaddr object, gaddr motion, gaddr shape, gaddr frame, int32_t loop); /* core/motion.c */
gaddr race_player_body(gaddr scene, int32_t i);                                           /* game/race_scene.c */
uint32_t race_player_cell(gaddr scene, int32_t i);                                        /* game/race_scene.c */

/* ---- the SHC runtime ---- */
/* func_8C062800: construct n objects of `size` bytes in place at p with ctor */
static inline void array_construct(int32_t n, uint32_t size, gaddr p, gaddr ctor) { CALL4(0x8C062800u, n, size, p, ctor); }
/* func_8C062754: destroy n objects of `size` bytes at p with dtor */
static inline void array_destroy(int32_t n, uint32_t size, gaddr p, gaddr dtor) { CALL4(0x8C062754u, n, size, p, dtor); }
/* func_8C062790: construct n objects in a block from operator new[] (returns the array) */
static inline gaddr array_construct_new(int32_t n, uint32_t size, gaddr p, gaddr ctor) {
  return CALL4(0x8C062790u, n, size, p, ctor);
}
/* the empty constructors the arrays above are built with (guest code: their addresses are data) */
#define CTOR_EMPTY_VEC3_TRIPLE 0x8C0483E4u /* Triple's Vec3s */
#define CTOR_EMPTY_VEC3_2B     0x8C0498D4u /* type 0x2B's Vec3s */
#define CTOR_EMPTY_VEC3_BALLS  0x8C04D6E0u /* type 0x1E's Vec3s */

/* ---- still-translated game code ---- */
/* func_8C02EDAA: play sound `id` of bank `bank` on channel `channel` for player `player`, by the game's mode */
static inline void race_scene_sound(gaddr scene, int32_t player, int32_t channel, int32_t bank, int32_t id) {
  CALL5(0x8C02EDAAu, scene, player, channel, bank, id);
}
/* func_8C041B34: the course cell a position is on, searched from `hint` */
static inline int32_t course_find_cell(gaddr course, gaddr pos, int32_t hint) {
  return (int32_t)CALL3(0x8C041B34u, course, pos, hint);
}
/* func_8C04443E: the actor's cell = the course cell its position is on (from `hint`) */
static inline void actor_locate(gaddr actor, gaddr pos, int32_t hint) { CALL3(0x8C04443Eu, actor, pos, hint); }

/* game/actor_bodies.c, game/actor_misc.c */
gaddr sphere_construct(gaddr slot);
int32_t race_scene_find_cell(gaddr scene, gaddr pos, int32_t hint);

int32_t race_scene_view_focus(gaddr scene, int32_t i);

/* S14 b11: game/actor_motes.c (exported there, no header yet) */
gaddr motes_construct(gaddr obj, int32_t count);
gaddr motes_destroy(gaddr obj);
void motes_setup(gaddr obj, gaddr origin, int32_t mode, f32 top);
void motes_restart(gaddr obj);
int32_t motes_update(gaddr obj);

#endif
