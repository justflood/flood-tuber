#pragma once

#include <obs-module.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <graphics/image-file.h>
#include <math.h>
#include "webp-decoder.h"
#include "apng-decoder.h"

#define FLOOD_TUBER_VERSION "1.0.2"
#define BLOG(level, format, ...) blog(level, "[Flood-Tuber] " format, ##__VA_ARGS__)

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
	SHAKE       // Random jitter/vibration
};

// Wrapper for different image types (OBS standard or Custom)
struct FloodImage {
    enum Type {
        OBS_STANDARD, // Uses gs_image_file_t (GIF, JPEG, PNG, etc.)
        CUSTOM_WEBP,  // Uses WebPDecoder
        CUSTOM_APNG   // Uses APNGDecoder (Future)
    } type;

    // Standard OBS loader
    gs_image_file_t obs_image;

    // Custom decoders
    WebPDecoder* webp_decoder = nullptr;
    APNGDecoder* apng_decoder = nullptr;
    
    // Animation state for custom decoders
    uint64_t anim_time_ns = 0;

    FloodImage() {
        type = OBS_STANDARD;
        gs_image_file_init(&obs_image, NULL);
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
        gs_image_file_free(&obs_image);
        type = OBS_STANDARD;
    }
};

// Validate file headers to prevent crashes (e.g. renamed .txt files)
inline bool check_file_signature(const char *path) {
    if (!path || !*path) return false;
    
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    
    unsigned char sig[12] = {0};
    size_t len = fread(sig, 1, 12, f);
    fclose(f);
    
    if (len < 4) return false; // Too small to be valid image

    // Check GIF: "GIF87a" or "GIF89a"
    if (len >= 6 && sig[0] == 'G' && sig[1] == 'I' && sig[2] == 'F' && 
        sig[3] == '8' && (sig[4] == '7' || sig[4] == '9') && sig[5] == 'a') {
        return true; 
    }

    // Check PNG: 89 50 4E 47 0D 0A 1A 0A
    if (len >= 8 && sig[0] == 0x89 && sig[1] == 0x50 && sig[2] == 0x4E && sig[3] == 0x47 &&
        sig[4] == 0x0D && sig[5] == 0x0A && sig[6] == 0x1A && sig[7] == 0x0A) {
        return true;
    }

    // Check WebP: RIFF .... WEBP
    if (len >= 12 && sig[0] == 'R' && sig[1] == 'I' && sig[2] == 'F' && sig[3] == 'F' &&
        sig[8] == 'W' && sig[9] == 'E' && sig[10] == 'B' && sig[11] == 'P') {
        return true;
    }

    // Check JPEG: FF D8 FF
    if (len >= 3 && sig[0] == 0xFF && sig[1] == 0xD8 && sig[2] == 0xFF) {
        return true;
    }

    // Allow others if we are brave, but for GIF specifically we MUST validate 
    // because OBS crashes on corrupt GIFs.
    const char *ext = strrchr(path, '.');
    if (ext && _strcmpi(ext, ".gif") == 0) {
        return false; // Extension says GIF but header doesn't match
    }

    return true; // Let OBS try other formats (BMP, TGA etc) if not explicitly suspicious
}

// Loads an image file and initializes its texture
inline void update_image(FloodImage *image, const char *path)
{
	obs_enter_graphics();
	image->Free();
    
    if (path && *path) {
        if (!check_file_signature(path)) {
            BLOG(LOG_WARNING, "Invalid file signature (corrupt or fake file?): %s", path);
        } else {
            const char *ext = strrchr(path, '.');
            bool is_webp = (ext && (_strcmpi(ext, ".webp") == 0));
            
            if (is_webp) {
                image->type = FloodImage::CUSTOM_WEBP;
                image->webp_decoder = new WebPDecoder();
                if (!image->webp_decoder->Load(path)) {
                     BLOG(LOG_WARNING, "Failed to load WebP: %s", path);
                     image->Free();
                } else {
                     BLOG(LOG_INFO, "Loaded WebP: %s", path);
                }
            } else if (ext && (_strcmpi(ext, ".apng") == 0 || _strcmpi(ext, ".png") == 0)) {
                // Check if it's an animated PNG
                APNGDecoder *temp_decoder = new APNGDecoder();
                if (temp_decoder->Load(path) && temp_decoder->IsAnimated()) {
                     image->type = FloodImage::CUSTOM_APNG;
                     image->apng_decoder = temp_decoder;
                     BLOG(LOG_INFO, "Loaded Animated PNG: %s", path);
                } else {
                     if (temp_decoder->IsAnimated())
                         BLOG(LOG_WARNING, "Failed to load APNG (corrupt?): %s", path);
                     
                     // Clean up checks
                     delete temp_decoder;
                     
                     // Fallback to standard OBS loader for static PNGs or if APNG load failed
                     image->type = FloodImage::OBS_STANDARD;
                     gs_image_file_init(&image->obs_image, path);
                     gs_image_file_init_texture(&image->obs_image);
                }
            } else {
                image->type = FloodImage::OBS_STANDARD;
                gs_image_file_init(&image->obs_image, path);
                gs_image_file_init_texture(&image->obs_image);
            }
        }
    }
    image->anim_time_ns = 0;
	obs_leave_graphics();
}

struct flood_tuber_data {
	obs_source_t *source;       // The OBS source instance for this plugin
	obs_source_t *audio_source; // The external audio source we are monitoring

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
};
