#pragma once

#include <obs-module.h>
#include "flood-tuber.h" // We'll reuse the FloodImage structs from here

// Our Custom 6-Frame Animation Targets (Plugin)
enum MouthFrame {
    MF_Closed = 0,
    MF_A,
    MF_E,
    MF_OU,
    MF_I,
    MF_FV,
    MF_Count
};

#define OVR_CHUNK_SIZE 1024
#define OVR_RING_CAPACITY (48000 * 2)  // 2-second ring buffer at 48kHz

// LipSync specific data structure
struct flood_tuber_lipsync_data {
	obs_source_t *source = nullptr;
    obs_source_t *audio_source = nullptr;

    // Visuals
	FloodImage image_closed;
	FloodImage image_a;
	FloodImage image_e;
	FloodImage image_ou;
	FloodImage image_i;
	FloodImage image_fv;

    // Optional blink image from original
    FloodImage image_blink;

    // Audio State
    float current_rms = 0.0f;

    // LipSync State
    unsigned int ovr_context = 0;
    bool context_initialized = false;
    
    // Smoothing Data
    float smoothedFrames[MF_Count] = {0.0f};
    int current_dominant_idx = 0;

    // User Tweakable Parameters (Phase 4)
    float param_lerp_attack = 0.8f;
    float param_lerp_decay = 0.3f;
    float param_gate_lower = 0.0005f;
    float param_gate_upper = 0.003f;

    // Additional blinking logic
    float timer_blink = 0.0f;
    float time_until_next_blink = 3.0f;
    bool is_blinking_now = false;
    float blink_duration = 0.15f;
    float blink_interval_min = 2.0f;
    float blink_interval_max = 6.0f;
    
    // Audio ring buffer for OVR (OVR needs EXACTLY 1024 samples per call)
    // OBS sends variable-size chunks, so we accumulate them here first
    float *ring_buffer = nullptr; // Heap-allocated: OVR_RING_CAPACITY floats
    int ring_write_pos = 0;
    int ring_read_pos  = 0;
    int ring_count     = 0;

    // Persistent viseme and laughter buffers (never on stack of audio thread)
    float ovr_viseme_buf[16]  = {0.0f}; // 15 visemes + 1 safety
    float ovr_laugh_buf[8]    = {0.0f};
    
    // Position/Transform
	float offset_x = 0.0f;
	float offset_y = 0.0f;
	bool mirror = false;
};

// Properties for UI
obs_properties_t *flood_tuber_lipsync_properties(void *data);
void flood_tuber_lipsync_defaults(obs_data_t *settings);
