#ifndef __RECOMP_API_H__
#define __RECOMP_API_H__

#include <cstdint>

#include "recomp.h"

extern "C" void recomp_update_inputs(uint8_t* rdram, recomp_context* ctx);
extern "C" void sqrtf_recomp(uint8_t* rdram, recomp_context* ctx);
extern "C" void __ll_rshift_recomp(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_puts(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_exit(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_get_gyro_deltas(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_get_mouse_deltas(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_powf(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_get_target_framerate(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_get_window_resolution(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_get_target_aspect_ratio(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_get_targeting_mode(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_get_bgm_volume(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_get_sfx_volume(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_get_voice_volume(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_get_low_health_beeps_enabled(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_time_us(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_get_film_grain_enabled(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_load_overlays(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_high_precision_fb_enabled(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_get_resolution_scale(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_get_inverted_axes(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_get_radio_comm_box_mode(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_get_analog_inverted_axes(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_get_invert_y_axis_mode(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_get_camera_inputs(uint8_t* rdram, recomp_context* ctx);
extern "C" void recomp_set_right_analog_suppressed(uint8_t* rdram, recomp_context* ctx);

#endif
