/* Pen Pen port, S17 b03: more of the Racer's field accessors (game/racer.h), the small methods the race scene and
 * the HUD call on a racer from outside (core/accessors.c has the first ones: racer_state, racer_action, ...). */
#include <string.h>
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../game/racer.h"

/* was func_8C02912C: the address of its second Body (0x2FC) */
gaddr racer_body2fc(gaddr racer) { return member_addr(racer, Racer, body2fc); }
HW_EXPORT(8c02912c, RET(racer_body2fc(ARG0)))

/* was func_8C02913C / func_8C029144: the value of its spring at 0x33C / its float at 0x434 */
f32 racer_value33c(gaddr racer) { return racer_at(racer)->value33c.value; }
f32 racer_f434(gaddr racer) { return racer_at(racer)->f434; }
HW_EXPORT(8c02913c, RETF(racer_value33c(ARG0)))
HW_EXPORT(8c029144, RETF(racer_f434(ARG0)))

/* was func_8C029150 / func_8C029156: its progress word (0x1A8) / the special ground it touches (0x23C) */
int32_t racer_word1a8(gaddr racer) { return racer_at(racer)->word1a8; }
int32_t racer_ground_kind(gaddr racer) { return racer_at(racer)->ground; }
HW_EXPORT(8c029150, RET(racer_word1a8(ARG0)))
HW_EXPORT(8c029156, RET(racer_ground_kind(ARG0)))

/* was func_8C029168: time i of times298 (no bound check: the original reads any index) */
f32 racer_time(gaddr racer, int32_t i) { return racer_at(racer)->times298[i]; }
HW_EXPORT(8c029168, RETF(racer_time(ARG0, (int32_t)ARG1)))

/* was func_8C029174 / func_8C029180: get / set stick_damped */
int32_t racer_stick_damped(gaddr racer) { return racer_at(racer)->stick_damped; }
void racer_set_stick_damped(gaddr racer, int32_t value) { racer_at(racer)->stick_damped = value; }
HW_EXPORT(8c029174, RET(racer_stick_damped(ARG0)))
HW_EXPORT(8c029180, racer_set_stick_damped(ARG0, (int32_t)ARG1))

/* was func_8C02917A / func_8C029186: get / set the float at 0x188 */
f32 racer_f188(gaddr racer) { return racer_at(racer)->f188; }
void racer_set_f188(gaddr racer, f32 value) { racer_at(racer)->f188 = value; }
HW_EXPORT(8c02917a, RETF(racer_f188(ARG0)))
HW_EXPORT(8c029186, racer_set_f188(ARG0, FARG0))

/* was func_8C02918C: whether racer_handle_contacts saw contacts this frame */
int32_t racer_had_contacts(gaddr racer) { return racer_at(racer)->had_contacts; }
HW_EXPORT(8c02918c, RET(racer_had_contacts(ARG0)))

/* was func_8C0291AC / func_8C0291C2: copy a Vec3 into its moving frame's position / velocity */
void racer_put_position(gaddr racer, gaddr v) {
  memmove(&racer_at(racer)->body.body.position, hw_ptr(v), sizeof(Vec3));
}
void racer_put_velocity(gaddr racer, gaddr v) {
  memmove(&racer_at(racer)->body.body.velocity, hw_ptr(v), sizeof(Vec3));
}
HW_EXPORT(8c0291ac, racer_put_position(ARG0, ARG1))
HW_EXPORT(8c0291c2, racer_put_velocity(ARG0, ARG1))

/* was func_8C0291DA: set its heading (the spring's value, Ninja angle units) */
void racer_put_heading(gaddr racer, f32 heading) { racer_at(racer)->heading.value = heading; }
HW_EXPORT(8c0291da, racer_put_heading(ARG0, FARG0))
