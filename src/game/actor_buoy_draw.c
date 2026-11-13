/* Pen Pen port, S18 b05: the draw of the buoy (type 0x2A) (game/actor.h: Buoy; the rest is in game/actor_buoy.c). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/ninja.h"
#include "actor.h"

/* SDK nj_ModelL3 (0x8C0D1996, port/symbols/sdk_calls.tsv) */
static inline void nj_ModelL3(gaddr model) { CALL1(0x8C0D1996u, model); }

#define BUOY_FLOAT_DROP   F32_BITS(0xC17947ADu) /* -15.58: kind 0's model hangs this far below its Body */
#define BUOY_MODELS_KIND0 26                    /* kind 0 draws model 26 + its subkind */
#define BUOY_MODEL_KIND1  9

/* the original's frame (0x10 bytes under PR): the drop goes to njTranslateV */
typedef struct BuoyDrawFrame {
  Vec3 drop;         /* 0x00 */
  gaddr self;        /* 0x0C */
  uint32_t saved_pr; /* 0x10 */
} BuoyDrawFrame;
HW_OFFSET(BuoyDrawFrame, self, 0x0C);
HW_SIZE(BuoyDrawFrame, 0x14);

/* was func_8C0587B8: the draw method: at its Body's position turned about y and x, kind 0 draws model 26 + its
 * subkind 15.58 lower (nj_ModelL3), kind 1 model 9 (nj_ModelL1), other kinds nothing */
void buoy_draw(gaddr obj) {
  const Buoy *b = (const Buoy *)hw_ptr(obj);
  gaddr frame = hw_stack_alloc(sizeof(BuoyDrawFrame));
  BuoyDrawFrame *f = (BuoyDrawFrame *)hw_ptr(frame);
  f->self = obj;
  matrix_push();
  switch (b->kind) {
  case 0: {
    gaddr drop = member_addr(frame, BuoyDrawFrame, drop);
    matrix_translate_v(method_self(member_addr(obj, Buoy, body)));
    matrix_rotate_y(b->angle_y);
    matrix_rotate_x(b->angle_x);
    vec3_set(F32(0.0f), BUOY_FLOAT_DROP, F32(0.0f), drop);
    matrix_translate_v(drop);
    nj_ModelL3(actor_model(&b->base, b->subkind + BUOY_MODELS_KIND0));
    break;
  }
  case 1:
    matrix_translate_v(method_self(member_addr(obj, Buoy, body)));
    matrix_rotate_y(b->angle_y);
    matrix_rotate_x(b->angle_x);
    nj_ModelL1(actor_model(&b->base, BUOY_MODEL_KIND1));
    break;
  default:
    break;
  }
  matrix_pop();
  hw_stack_free(sizeof(BuoyDrawFrame));
}
HW_EXPORT(8c0587b8, buoy_draw(ARG0))
