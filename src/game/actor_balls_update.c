/* Pen Pen port, S19 (u2): the per-frame method of the big body with eight spheres (type 0x1E, Balls in
 * game/actor.h; vtable 0x8C23E09C, slot 0x10): it waits for a player to come far enough along the course, then swings
 * through the angles of its static table; its spheres follow it around. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "game.h"
#include "actor.h"
#include "race_scene.h"
#include "scenery.h"

#define BALLS_WAKE_CELL 0x78                    /* a player on course cell 120 or beyond starts the swing */
#define BALLS_STEP_UPDATES 0x78                 /* updates between two table entries (word44 counts past 120) */
#define BALLS_TABLE_ZERO 0x63                   /* a table entry of 99 means angle 0 */
#define BALLS_DEGREE F32_BITS(0x42B60B60u)      /* 91.022217: one degree in Ninja units */
#define BALLS_SWING_RATE F32_BITS(0x3C23D70Au)  /* 0.01 */
#define BALLS_SPHERE_PLACES 0x8C0882A8u         /* f32 (x, z) pairs: one per two spheres */
#define BALLS_SPHERE_LOW F32(4.5f)              /* an even sphere's height */
#define BALLS_SPHERE_HIGH F32_BITS(0x412E6666u) /* 10.9: an odd sphere's height */
#define BALLS_SLOT 8                            /* the race scene's slot for the contacts */

enum { BALLS_IDLE = 0, BALLS_WAITING = 1, BALLS_SWINGING = 2 };

/* the update's frame on the guest stack (the original's: 0x64 bytes under its saved PR and r8): Ninja works on the
 * matrix and the sphere's offset by address */
typedef struct BallsUpdateFrame {
  Vec3 offset;               /* 0x00: a sphere's place on the body */
  uint8_t pad_0c[0x20 - 0x0C];
  Matrix m;                  /* 0x20 */
  gaddr self;                /* 0x60 */
  uint32_t saved[2];         /* 0x64: the original's PR and r8 slots */
} BallsUpdateFrame;
HW_OFFSET(BallsUpdateFrame, m, 0x20);
HW_OFFSET(BallsUpdateFrame, self, 0x60);
HW_SIZE(BallsUpdateFrame, 0x6C);

/* state 1: does a player stand on cell 120 or beyond? then swing */
static void balls_wait(gaddr obj) {
  Balls *b = (Balls *)hw_ptr(obj);
  int32_t found = 0;
  int32_t count = race_setup_at(race_setup_get())->player_count;
  for (int32_t i = 0; i < count; i++) {
    int32_t cell = (int32_t)race_player_cell(panel_get(), i);
    if (cell >= BALLS_WAKE_CELL) {
      found = 1;
      break;
    }
  }
  if (found != 0) b->state++;
}

/* state 2: every 121 updates the next table entry is the target angle (in degrees; 99 = 0); an entry of 0 ends the
 * swing (state 0), else the table moves on */
static void balls_swing(gaddr obj) {
  Balls *b = (Balls *)hw_ptr(obj);
  b->word44++;
  if (!(b->word44 > BALLS_STEP_UPDATES)) return;
  b->word44 = 0;
  int32_t degrees = *(const int32_t *)hw_ptr(b->table);
  if (degrees == BALLS_TABLE_ZERO) degrees = 0;
  b->f48 = fmul(f32_from_int(degrees), BALLS_DEGREE);
  if (*(const int32_t *)hw_ptr(b->table) == 0)
    b->state = BALLS_IDLE;
  else
    b->table += sizeof(int32_t);
}

/* was func_8C04DACA: the update: the state's step; the swing is pulled toward the target (rate 0.01) and steps, the
 * body turns by it; the contacts go into the race scene's slot 8; then each sphere's point is placed on the body
 * turned about y (at x, z of its table pair, 4.5 up when even, 10.9 when odd), its course cell searched from the
 * last one, and its contacts go into slot 8 on that cell */
void balls_update(gaddr obj) {
  gaddr fp = hw_stack_alloc(sizeof(BallsUpdateFrame));
  BallsUpdateFrame *f = (BallsUpdateFrame *)hw_ptr(fp);
  Balls *b = (Balls *)hw_ptr(obj);
  f->self = obj;
  switch (b->state) {
  case BALLS_IDLE: break;
  case BALLS_WAITING: balls_wait(obj); break;
  case BALLS_SWINGING: balls_swing(obj); break;
  default: break;
  }
  gaddr swing = member_addr(obj, Balls, swing);
  body1_pull_angle(b->f48, BALLS_SWING_RATE, swing);
  body1_step(swing);
  f32 turn = b->swing.value;
  b->f34 = fadd(b->f34, turn);
  race_scene_slot_insert(panel_get(), member_addr(obj, Balls, contacts), BALLS_SLOT);

  gaddr m = member_addr(fp, BallsUpdateFrame, m);
  gaddr offset = member_addr(fp, BallsUpdateFrame, offset);
  matrix_identity(m);
  njUnitMatrix(m);
  njTranslateV(m, member_addr(obj, Balls, position));
  njRotateY(m, f32_to_int(b->f34));
  const f32 *place = (const f32 *)hw_ptr(BALLS_SPHERE_PLACES);
  for (int32_t i = 0; i < BALLS_POINTS; i++) {
    f32 x = place[0];
    f32 z = place[1];
    f32 y = BALLS_SPHERE_LOW;
    if (i & 1) {
      y = BALLS_SPHERE_HIGH;
      place += 2;
    }
    vec3_set(x, y, z, offset);
    gaddr point = member_addr(obj, Balls, points[i]);
    njCalcPoint(m, offset, point);
    b->cells[i] = race_scene_find_cell(panel_get(), point, b->cells[i]);
    b->spheres[i].contacts.cell = b->cells[i];
    race_scene_slot_insert(panel_get(), member_addr(obj, Balls, spheres[i]), BALLS_SLOT);
  }
  hw_stack_free(sizeof(BallsUpdateFrame));
}
HW_EXPORT(8c04daca, balls_update(ARG0))
