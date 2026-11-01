#pragma once

#include <obs-module.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <graphics/image-file.h>
#include <math.h>
#include "webp-decoder.h"
#include "apng-decoder.h"

// FLOOD_TUBER_VERSION is defined by CMake via target_compile_definitions
#ifndef FLOOD_TUBER_VERSION
#define FLOOD_TUBER_VERSION "0.0.0"
#endif
#define BLOG(level, format, ...) blog(level, "[Flood-Tuber] " format, ##__VA_ARGS__)

// OBS 33 removed the gs_image_file_* API in favor of gs_image_file_ex_*,
// which takes an explicit alpha mode. Pick the right one at compile time.
#if LIBOBS_API_MAJOR_VER >= 33
typedef gs_image_file_ex_t flood_image_file_t;
#define flood_image_file_init(image, file) gs_image_file_ex_init(image, file, GS_IMAGE_ALPHA_STRAIGHT)
#define flood_image_file_free gs_image_file_ex_free
#define flood_image_file_init_texture gs_image_file_ex_init_texture
#define flood_image_file_tick gs_image_file_ex_tick
#define flood_image_file_update_texture gs_image_file_ex_update_texture
#else
typedef gs_image_file_t flood_image_file_t;
#define flood_image_file_init gs_image_file_init
#define flood_image_file_free gs_image_file_free
#define flood_image_file_init_texture gs_image_file_init_texture
#define flood_image_file_tick gs_image_file_tick
#define flood_image_file_update_texture gs_image_file_update_texture
#endif

// Represents the current animation state of the avatar
enum class AvatarState {
	IDLE,       // Default state, no sound
	TALKING,    // Sound detected, mouth open/animating
	BLINKING,   // Temporary state for eye blink animation
	ACTION      // Random action animation triggered periodically
};

// Types of motion effects applied when talking
enum class TalkingEffect {
	NONE,
	BOUNCE,     // Vertical jumping motion
	SHAKE,      // Random jitter/vibration
	OSCILLATING      // Smooth figure-eight drift
};

// Wrapper for different image types (OBS standard or Custom)
struct FloodImage {
    enum Type {
        OBS_STANDARD, // Uses flood_image_file_t (GIF, JPEG, PNG, etc.)
        CUSTOM_WEBP,  // Uses WebPDecoder
        CUSTOM_APNG   // Uses APNGDecoder (Future)
    } type;

    // Standard OBS loader
    flood_image_file_t obs_image;

    // Custom decoders
    WebPDecoder* webp_decoder = nullptr;
    APNGDecoder* apng_decoder = nullptr;

    // Animation state for custom decoders
    uint64_t anim_time_ns = 0;

    FloodImage() {
        type = OBS_STANDARD;
        flood_image_file_init(&obs_image, NULL);
    }

    // Helper: Free resources
    void Free() {
        if (webp_decoder) {
            delete webp_decoder;
            webp_decoder = nullptr;
        }
        if (apng_decoder) {
            delete apng_decoder;
            apng_decoder = nullptr;
        }
        flood_image_file_free(&obs_image);
        type = OBS_STANDARD;
    }
};

#define FLOOD_SLOT_COUNT 10

struct flood_tuber_data;

// Raccourci « emplacement » : retient son instance et son index (1..10)
struct flood_slot_hotkey {
	struct flood_tuber_data *tuber;
	int index;
	obs_hotkey_id id;
};

struct flood_tuber_data {
	obs_source_t *source;             // The OBS source instance for this plugin
    obs_weak_source_t *audio_source; // Mic source, resolved by name after startup
    // -- Hotkey --
   	obs_hotkey_id hotkey_next;   // Avatar Next
	obs_hotkey_id hotkey_prev;   // Avatar Prev
	struct flood_slot_hotkey slot_hotkeys[FLOOD_SLOT_COUNT];
    char audio_source_name[256];
    uint64_t capture_check_time;

	// -- Image Assets (Using Wrapper) --
	FloodImage image_idle;
	FloodImage image_blink;
	FloodImage image_action;
	FloodImage image_talking_1;
	FloodImage image_talking_2;
	FloodImage image_talking_3;

	// Variants for blinking while talking
	FloodImage image_talking_1_blink;
	FloodImage image_talking_2_blink;
	FloodImage image_talking_3_blink;

	// -- User Configuration --
	float threshold;           // Audio dB threshold to trigger talking state
	float release_delay;       // Time in seconds to hold talking state after silence
	float action_interval_min; // Min time between actions
	float action_interval_max; // Max time between actions
	float action_duration;     // Duration of the action animation
	float blink_interval_min;  // Min time between blinks
	float blink_interval_max;  // Max time between blinks
	float blink_duration;      // Duration of a blink

	// -- Runtime State --
	AvatarState current_state;

	float timer_release_hold;  // Timer for keeping mouth open briefly after sound stops
	float timer_action;        // Timer for tracking action intervals
	float timer_blink;         // Timer for tracking blink intervals
	float timer_talk_anim;     // Timer for switching talking frames

	float time_until_next_action; // Randomized target time for next action
	float time_until_next_blink;  // Randomized target time for next blink

	int talking_frame_index;   // Current talking frame index (0, 1, or 2)
	bool is_blinking_now;      // True if currently in a blink phase

	volatile float current_db; // Current audio level in decibels

	// -- Motion Effects --
	TalkingEffect talk_effect;
	bool mirror;
	float talk_interval;    // Time in seconds between talking frames (default 0.10f)
	float effect_speed;
	float effect_strength;
	float timer_effect;        // Continuous timer for sin/cos motion calculations
	float offset_x;            // Calculated X offset for rendering
	float offset_y;            // Calculated Y offset for rendering
		uint32_t reference_height; // Common display height (px); offsets are a % of it
};
