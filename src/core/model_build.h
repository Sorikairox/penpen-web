/* Pen Pen port, S15 (b01): building a basic Ninja model in memory (core/model_build.c, core/model_free.c), declared
 * for their callers. */
#ifndef PENPEN_MODEL_BUILD_H
#define PENPEN_MODEL_BUILD_H
#include "hw.h"

/* core/model_build.c */
void model_alloc_points(gaddr model, int32_t n);
void model_free_points(gaddr model);
void model_alloc_materials(gaddr model, int32_t n);
void model_free_materials(gaddr model);
void model_alloc_meshsets(gaddr model, int32_t n);
void model_free_meshsets(gaddr model);

/* core/model_free.c (S15 b01) */
void model_free_all(gaddr model);

#endif
