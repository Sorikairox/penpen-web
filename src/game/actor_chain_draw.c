/* Pen Pen port, S19 (u): the per-view draw of the chain (type 0x1D, Chain in game/actor.h; vtable 0x8C23DD4C,
 * slot 3): a sprite at the body, then one at each of the 20 links, additively blended. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/ninja.h"
#include "actor.h"

#define CHAIN_TEXLIST 0x8C09703Cu /* the chain's NJS_TEXLIST */
#define CHAIN_TEXANIM 0x8C097044u /* and its NJS_TEXANIM */
#define CHAIN_HUB_SPRITE 9
#define CHAIN_HUB_ATTR 0x31
#define CHAIN_LINK_SPRITE 5
#define CHAIN_LINK_ATTR 0x30
#define CHAIN_LINK_SCALE F32(8.0f) /* a link's sprite is an eighth of its length */

/* SDK njColorBlendingMode (0x8C0A23A8): the source (0) / destination (1) blending */
static void njColorBlendingMode(int32_t target, int32_t mode) { CALL2(0x8C0A23A8u, target, mode); }

/* the draw's frame on the guest stack (the original's: 0x94 bytes under its saved PR): Ninja works on the matrix,
 * the two vectors and the sprite by address. The body's position is copied at 0x50, inside the matrix's room
 * (the matrix is set up only after it is used) */
typedef struct ChainDrawFrame {
  Vec3 offset;          /* 0x00: (0, 0, the link's length) */
  Vec3 point;           /* 0x0C: the link's sprite's place */
  f32 scale;            /* 0x18 */
  f32 length;           /* 0x1C */
  union {
    Matrix m;           /* 0x20 */
    struct {
      uint8_t pad_20[0x50 - 0x20];
      Vec3 hub;         /* 0x50: the body's position */
      f32 swing;        /* 0x5C */
    };
  };
  int32_t i;            /* 0x60 */
  gaddr link_b, link_a; /* 0x64 */
  gaddr link;           /* 0x6C */
  NjSprite sprite;      /* 0x70 */
  gaddr self;           /* 0x90 */
  uint32_t saved_pr;    /* 0x94: the original's PR slot */
} ChainDrawFrame;
HW_OFFSET(ChainDrawFrame, point, 0x0C);
HW_OFFSET(ChainDrawFrame, m, 0x20);
HW_OFFSET(ChainDrawFrame, hub, 0x50);
HW_OFFSET(ChainDrawFrame, swing, 0x5C);
HW_OFFSET(ChainDrawFrame, sprite, 0x70);
HW_OFFSET(ChainDrawFrame, self, 0x90);
HW_SIZE(ChainDrawFrame, 0x98);

/* was func_8C047C58: the chain's draw in a view: blending (src 8, dst 6); the hub sprite (9, attr 0x31) at the body,
 * turned by word_b4 and scaled by the swing; blending (src 9, dst 10); then each link's sprite (5, attr 0x30), an
 * eighth of its length in size, at its length along z turned by the link's two angles (x, then y), from the body */
void chain_draw_view(gaddr obj) {
  gaddr fp = hw_stack_alloc(sizeof(ChainDrawFrame));
  ChainDrawFrame *f = (ChainDrawFrame *)hw_ptr(fp);
  Chain *c = (Chain *)hw_ptr(obj);
  gaddr sp = member_addr(fp, ChainDrawFrame, sprite);
  f->self = obj;
  f->sprite.tlist = CHAIN_TEXLIST;
  f->sprite.tanim = CHAIN_TEXANIM;
  njColorBlendingMode(0, 8);
  njColorBlendingMode(1, 6);
  f->sprite.ang = f32_to_int(c->word_b4);
  f->swing = c->swing.value;
  f->sprite.sx = f->swing;
  f->sprite.sy = f->swing;
  vec3_copy(member_addr(fp, ChainDrawFrame, hub), member_addr(obj, Chain, body));
  f->sprite.p = f->hub;
  njDrawSprite3D(sp, CHAIN_HUB_SPRITE, CHAIN_HUB_ATTR);

  njColorBlendingMode(0, 9);
  njColorBlendingMode(1, 10);
  f->sprite.ang = 0;
  f->link = member_addr(obj, Chain, links);
  f->link_a = member_addr(obj, Chain, link_a);
  f->link_b = member_addr(obj, Chain, link_b);
  gaddr m = member_addr(fp, ChainDrawFrame, m);
  for (f->i = 0; f->i < CHAIN_LINKS; f->i++) {
    matrix_identity(m);
    njUnitMatrix(m);
    gaddr a = f->link_a;
    f->link_a = a + sizeof(f32);
    njRotateX(m, f32_to_int(F32_AT(a)));
    gaddr b = f->link_b;
    f->link_b = b + sizeof(f32);
    njRotateY(m, f32_to_int(F32_AT(b)));
    f->length = body1_at(f->link)->value;
    f->scale = fdiv(f->length, CHAIN_LINK_SCALE);
    f->sprite.sx = f->scale;
    f->sprite.sy = f->scale;
    vec3_set(F32(0.0f), F32(0.0f), f->length, member_addr(fp, ChainDrawFrame, offset));
    njCalcPoint(m, member_addr(fp, ChainDrawFrame, offset), member_addr(fp, ChainDrawFrame, point));
    vec3_add(member_addr(fp, ChainDrawFrame, point), member_addr(obj, Chain, body));
    f->sprite.p = f->point;
    njDrawSprite3D(sp, CHAIN_LINK_SPRITE, CHAIN_LINK_ATTR);
    f->link += sizeof(Body1);
  }
  hw_stack_free(sizeof(ChainDrawFrame));
}
HW_EXPORT(8c047c58, chain_draw_view(ARG0))
