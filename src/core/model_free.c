/* Pen Pen port, S15 (b01): freeing a model built in memory (core/model_build.c's arrays). */
#include "hw.h"
#include "model_build.h"

/* was func_8C014FAC: frees the model's points and normals, its mesh sets (with their arrays), then its materials */
void model_free_all(gaddr model) {
  model_free_points(model);
  model_free_meshsets(model);
  model_free_materials(model);
}
HW_EXPORT(8c014fac, model_free_all(ARG0))
