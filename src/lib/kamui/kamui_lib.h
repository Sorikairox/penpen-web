/* Pen Pen port, S20: the two Kamui (PowerVR driver) leaf functions Ninja's fog code calls. */
#ifndef PENPEN_KAMUI_LIB_H
#define PENPEN_KAMUI_LIB_H

#include "hw.h"

int32_t km_write_holly_reg(uint32_t offset, uint32_t value);
int32_t km_set_fog_density(uint32_t density);

#endif
