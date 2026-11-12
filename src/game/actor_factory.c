/* Pen Pen port, S15 b07: the scenery actors' factory (game/actor.h): a new actor of a placement's type. */
#include "hw.h"
#include "sdk.h"
#include "actor.h"

/* per placement type: the class's size and its constructor (which stores both vtables and runs the Actor
 * constructor; hand-written or still translated, entered by its address either way) */
typedef struct ActorClass {
  uint32_t size;
  gaddr construct;
} ActorClass;

#define ACTOR_TYPES 0x34
static const ActorClass actor_classes[ACTOR_TYPES] = {
  {0x0C0, 0x8C044AECu}, /* 0x00: func_8C044AEC */
  {0x110, 0x8C045020u}, /* 0x01: actor_type01_construct */
  {0x0E0, 0x8C0458FCu}, /* 0x02: actor_type02_construct */
  {0x08C, 0x8C046028u}, /* 0x03: actor_pair_construct */
  {0x058, 0x8C0466F2u}, /* 0x04: strip_construct */
  {0xBD4, 0x8C046C50u}, /* 0x05: pieces_construct */
  {0x0FC, 0x8C04BBEEu}, /* 0x06: func_8C04BBEE */
  {0x11C, 0x8C04C1E8u}, /* 0x07: bubbles_construct */
  {0x078, 0x8C04CB26u}, /* 0x08: spring_construct */
  {0x0B4, 0x8C04CDA0u}, /* 0x09: func_8C04CDA0 */
  {0x060, 0x8C05568Au}, /* 0x0A: func_8C05568A */
  {0x13C, 0x8C0529B2u}, /* 0x0B: func_8C0529B2 */
  {0x0B4, 0x8C053008u}, /* 0x0C: bumper_construct */
  {0x10C, 0x8C053600u}, /* 0x0D: flap_construct */
  {0x044, 0x8C047576u}, /* 0x0E: actor_type0e_construct */
  {0x154, 0x8C053E26u}, /* 0x0F: func_8C053E26 */
  {0x13C, 0x8C0548E0u}, /* 0x10: func_8C0548E0 */
  {0x074, 0x8C059C30u}, /* 0x11: flyer_construct */
  {0x110, 0x8C05A72Eu}, /* 0x12: fence_construct */
  {0xD34, 0x8C05AA76u}, /* 0x13: wave_field_construct */
  {0x10C, 0x8C05B194u}, /* 0x14: ornament_construct */
  {0x068, 0x8C05B942u}, /* 0x15: swinger_construct */
  {0x0EC, 0x8C05BC60u}, /* 0x16: sparkles_construct */
  {0x0A8, 0x8C055AE2u}, /* 0x17: turning_gate_construct */
  {0x0C0, 0x8C0560E2u}, /* 0x18: geyser_construct */
  {0x1BC, 0x8C050504u}, /* 0x19: type19_construct */
  {0x18C, 0x8C04B072u}, /* 0x1A: effect_construct */
  {0x090, 0x8C04928Au}, /* 0x1B: func_8C04928A */
  {0x090, 0x8C0494F6u}, /* 0x1C: actor_type1c_construct */
  {0x2AC, 0x8C04795Cu}, /* 0x1D: chain_construct */
  {0x3F8, 0x8C04D924u}, /* 0x1E: balls_construct */
  {0x068, 0x8C048476u}, /* 0x1F: triple_construct */
  {0x0E8, 0x8C061964u}, /* 0x20: actor_type20_construct */
  {0x270, 0x8C056886u}, /* 0x21: swimmer_construct */
  {0x124, 0x8C057784u}, /* 0x22: sign_construct */
  {0x040, 0x8C0487C4u}, /* 0x23: turner_construct */
  {0x1EC, 0x8C051FE0u}, /* 0x24: type24_construct */
  {0x09C, 0x8C0595D0u}, /* 0x25: actor_type25_construct */
  {0x11C, 0x8C058C2Au}, /* 0x26: func_8C058C2A */
  {0x69C, 0x8C05A0A8u}, /* 0x27: func_8C05A0A8 */
  {0x27C, 0x8C04E528u}, /* 0x28: func_8C04E528 */
  {0x068, 0x8C055132u}, /* 0x29: flock_construct */
  {0x0DC, 0x8C058538u}, /* 0x2A: buoy_construct */
  {0x1B8, 0x8C049CB0u}, /* 0x2B: type2b_construct */
  {0x3F0, 0x8C05BFC6u}, /* 0x2C: rings_construct */
  {0x0B8, 0x8C05C6AAu}, /* 0x2D: actor_type2d_construct */
  {0x0B0, 0x8C04D220u}, /* 0x2E: post_construct */
  {0x0E4, 0x8C048C58u}, /* 0x2F: func_8C048C58 */
  {0x3AC, 0x8C05D190u}, /* 0x30: func_8C05D190 */
  {0x1FC, 0x8C05EFB4u}, /* 0x31: func_8C05EFB4 */
  {0x1D0, 0x8C0603B6u}, /* 0x32: func_8C0603B6 */
  {0x12C, 0x8C04FAB4u}, /* 0x33: wobbler_construct */
};

/* was func_8C043D4C: a new actor of placement type `type`: its memory from syMalloc, constructed; 0 when out of
 * memory. A type past the table (unsigned) returns the first argument as it came (the original's switch default) */
gaddr actor_create(gaddr group, uint32_t type) {
  if (type >= ACTOR_TYPES) return group;
  const ActorClass *k = &actor_classes[type];
  gaddr p = syMalloc(k->size);
  if (p == 0) return 0;
  return CALL1(k->construct, p);
}
HW_EXPORT(8c043d4c, RET(actor_create(ARG0, ARG1)))
