/* Pen Pen port, S13: trivial inline methods (core/inline.c): class-neutral field access, empty bodies. */
#ifndef PENPEN_INLINE_H
#define PENPEN_INLINE_H
#include "vec.h"

uint32_t field_get(gaddr obj, uint32_t offset);
void field_set(gaddr obj, uint32_t offset, uint32_t value);
f32 field_get_f32(gaddr obj, uint32_t offset);
void field_set_f32(gaddr obj, uint32_t offset, f32 value);
gaddr field_addr(gaddr obj, uint32_t offset);
uint32_t field_array_get(gaddr obj, uint32_t offset, int32_t index, uint32_t stride);
int32_t field_table16_get(gaddr obj, uint32_t offset, int32_t index);
gaddr member_array_element(gaddr obj, int32_t index);
void field_set_and_clear(gaddr obj, uint32_t value);
void field_reset_2c(gaddr obj);
void field_set_and_clear2(gaddr obj, uint32_t value);
void method_empty(void);
gaddr method_self(gaddr obj);
gaddr word_construct(gaddr obj, uint32_t value);
void global_color_set(f32 r, f32 g, f32 b, f32 a);

#endif
