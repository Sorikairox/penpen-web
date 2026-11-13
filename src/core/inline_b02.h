/* Pen Pen port, S17 (b02): core/inline_b02.c's functions, declared for their callers. */
#ifndef PENPEN_INLINE_B02_H
#define PENPEN_INLINE_B02_H
#include "hw.h"

/* was func_8C01B072 / func_8C01E258: the constant material (r, g, b, a) */
void material_set_b072(f32 r, f32 g, f32 b, f32 a);
void material_set_e258(f32 r, f32 g, f32 b, f32 a);
gaddr mode0_scene_destruct(gaddr scene);
gaddr race_setup_destruct(gaddr setup);
gaddr title_destruct(gaddr scene);
gaddr good_night_destruct(gaddr scene);
gaddr empty_destruct(gaddr obj);

#endif
