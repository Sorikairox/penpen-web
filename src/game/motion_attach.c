/* Pen Pen port, S17 b04: the motion draw's node callback (game/motion_draw.c hands it to Ninja: while a character's
 * model is drawn it watches for the four nodes the parts hang on and keeps the current matrix of each) and an
 * identity-matrix constructor next to it. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"

#define g_attach_nodes  GUEST_VAR(gaddr, 0x8C096BA4u) /* the four nodes the parts hang on (the player's texlist) */
#define g_attach_flags  GUEST_VAR(gaddr, 0x8C096BA8u) /* &player->attached[0] */
#define ATTACH_MATRICES 0x8C1BAB18u                   /* four Ninja matrices the callback fills */
#define ATTACH_PARTS 4

/* was func_8C037CD6: Ninja's node callback while a character is drawn: when `node` is one of the four attach nodes,
 * keep the current matrix for that part and mark it attached (every match is taken) */
void motion_attach_node_callback(gaddr node) {
  const gaddr *nodes = (const gaddr *)hw_ptr(g_attach_nodes);
  int32_t *flags = (int32_t *)hw_ptr(g_attach_flags);
  for (int32_t k = 0; k < ATTACH_PARTS; k++) {
    if (nodes[k] == node) {
      njGetMatrix(ATTACH_MATRICES + (uint32_t)k * sizeof(Matrix));
      flags[k] = 1;
    }
  }
}
HW_EXPORT(8c037cd6, motion_attach_node_callback(ARG0))

/* was func_8C037CAE: make `m` the identity matrix (written from the last element down) */
void matrix_set_identity(gaddr m) {
  Matrix *x = (Matrix *)hw_ptr(m);
  for (int32_t i = 15; i >= 0; i--)
    x->m[i] = (i % 5 == 0) ? F32(1.0f) : F32(0.0f);
}
HW_EXPORT(8c037cae, matrix_set_identity(ARG0))
