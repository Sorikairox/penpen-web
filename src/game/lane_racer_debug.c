/* Pen Pen port, S18 (b03): the computer player's debug drawing (game/lane_racer_debug.c): its lane's vertex pairs as
 * marker spheres, and a numbered sprite at a point. Neither has a caller in the game code (left-over debug methods
 * of the LaneRacer, game/racer.h). */
#include "hw.h"
#include "sdk.h"
#include "racer.h"
#include "../core/vec.h"
#include "../core/ninja.h"

void sphere_draw_marker(gaddr sphere); /* core/draw2d.c (func_8C011A94) */

/* the marker's sphere (a local of the original: a centre and a radius) */
typedef struct DebugSphere {
  Vec3 center;  /* 0x00 */
  f32 radius;   /* 0x0C */
} DebugSphere;
HW_SIZE(DebugSphere, 16);

#define LANE_MARKER_RADIUS F32(5.0f)

/* was func_8C03FB62: draw a marker at every vertex of the lane table in use (lanes[lane][branch]); each vertex is
 * first put at the racer's height (the table itself is changed) */
void lane_racer_draw_lane(gaddr self) {
  LaneRacer *l = (LaneRacer *)hw_ptr(self);
  Racer *r = racer_at(self);
  gaddr vertex = l->lanes[l->lane][l->branch];
  f32 height = r->body.body.position.y;
  gaddr sphere = hw_stack_alloc(sizeof(DebugSphere));
  for (int32_t i = 0; i < l->lane_last[l->lane] * 2; i++) {
    vec3_at(vertex)->y = height;
    vec3_copy(member_addr(sphere, DebugSphere, center), vertex);
    ((DebugSphere *)hw_ptr(sphere))->radius = LANE_MARKER_RADIUS;
    sphere_draw_marker(sphere);
    vertex += sizeof(Vec3);
  }
  hw_stack_free(sizeof(DebugSphere));
}
HW_EXPORT(8c03fb62, lane_racer_draw_lane(ARG0))

/* the original's frame (0x24 bytes): the number, then the sprite */
typedef struct NumberSpriteFrame {
  int32_t number;    /* 0x00 */
  NjSprite sprite;   /* 0x04 */
} NumberSpriteFrame;
HW_SIZE(NumberSpriteFrame, 0x24);

#define DEBUG_SPRITE_TEXLIST 0x8C09703Cu
#define DEBUG_SPRITE_TEXANIM 0x8C097044u
#define DEBUG_SPRITE_SCALE   F32(3.0f)
#define DEBUG_SPRITE_FIRST   5    /* the texanim's cell of number 0 */
#define DEBUG_SPRITE_ATTR    0x30u

/* was func_8C03FBFE: draw the sprite of `number` (its cell number + 5) at `position`, scale 3 (`self` unused) */
void lane_racer_draw_number(gaddr self, gaddr position, int32_t number) {
  gaddr frame = hw_stack_alloc(sizeof(NumberSpriteFrame));
  NumberSpriteFrame *f = (NumberSpriteFrame *)hw_ptr(frame);
  const Vec3 *p = vec3_at(position);
  f->number = number;
  f->sprite.tlist = DEBUG_SPRITE_TEXLIST;
  f->sprite.tanim = DEBUG_SPRITE_TEXANIM;
  f->sprite.ang = 0;
  f->sprite.sx = DEBUG_SPRITE_SCALE;
  f->sprite.sy = DEBUG_SPRITE_SCALE;
  f->sprite.p.x = p->x;
  f->sprite.p.y = p->y;
  f->sprite.p.z = p->z;
  njDrawSprite3D(member_addr(frame, NumberSpriteFrame, sprite), f->number + DEBUG_SPRITE_FIRST, DEBUG_SPRITE_ATTR);
  hw_stack_free(sizeof(NumberSpriteFrame));
}
HW_EXPORT(8c03fbfe, lane_racer_draw_number(ARG0, ARG1, (int32_t)ARG2))
