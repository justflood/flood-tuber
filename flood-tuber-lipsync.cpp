#include "flood-tuber.h"
#include "flood-tuber-lipsync.h"
#include <util/dstr.h>
#include <util/platform.h>
#include <cmath>

// Use the official OVR SDK header and link against OVRLipSyncShim.lib
// The shim handles DLL loading/unloading internally via its own load_symbols()
#include "ovr-test/Include/OVRLipSync.h"

static bool g_OvrInitialized = false;
static bool g_OvrAvailable   = false; // true if Initialize + first CreateContext succeeded

// Common image tick macro from original plugin
static void flood_image_tick(FloodImage *img, uint64_t elapsed_ns) {
    if (img->type == FloodImage::CUSTOM_WEBP) {
        if (img->webp_decoder) {
            img->anim_time_ns += elapsed_ns;
        }
    } else if (img->type == FloodImage::CUSTOM_APNG) {
        if (img->apng_decoder) {
            img->anim_time_ns += elapsed_ns;
        }
    } else {
        gs_image_file_tick(&img->obs_image, elapsed_ns);
        gs_image_file_update_texture(&img->obs_image);
    }
}

static gs_texture_t* flood_image_get_texture(FloodImage *img) {
    if (img->type == FloodImage::CUSTOM_WEBP && img->webp_decoder) {
        return img->webp_decoder->GetTextureForTime(img->anim_time_ns / 1000000ULL);
    }
    if (img->type == FloodImage::CUSTOM_APNG && img->apng_decoder) {
        return img->apng_decoder->GetTextureForTime(img->anim_time_ns / 1000000ULL);
    }
    return img->obs_image.texture;
}

static uint32_t flood_image_get_width(FloodImage *img) {
    if (img->type == FloodImage::CUSTOM_WEBP && img->webp_decoder)
        return img->webp_decoder->GetWidth();
    if (img->type == FloodImage::CUSTOM_APNG && img->apng_decoder)
        return img->apng_decoder->GetWidth();
    return img->obs_image.cx;
}

static uint32_t flood_image_get_height(FloodImage *img) {
    if (img->type == FloodImage::CUSTOM_WEBP && img->webp_decoder)
        return img->webp_decoder->GetHeight();
    if (img->type == FloodImage::CUSTOM_APNG && img->apng_decoder)
        return img->apng_decoder->GetHeight();
    return img->obs_image.cy;
}

// Callback: Processes audio data to feed OVR
static void lipsync_audio_callback(void *data_ptr, obs_source_t *source, const struct audio_data *audio_data, bool muted)
{
	struct flood_tuber_lipsync_data *data = (struct flood_tuber_lipsync_data *)data_ptr;
	
    if (muted || !data->context_initialized) {
        data->current_rms = 0.0f;
		return;
	}

    if (!audio_data->data[0]) return;
    if (!data->ring_buffer) return;

    const float *incoming = (const float *)audio_data->data[0];
    int incoming_count = (int)audio_data->frames;

    // --- Fill the ring buffer with incoming samples ---
    for (int i = 0; i < incoming_count; i++) {
        float s = incoming[i];
        // Clamp and NaN protection
        if (!std::isfinite(s)) s = 0.0f;
        if (s >  1.0f) s =  1.0f;
        if (s < -1.0f) s = -1.0f;

        data->ring_buffer[data->ring_write_pos] = s;
        data->ring_write_pos = (data->ring_write_pos + 1) % OVR_RING_CAPACITY;
        if (data->ring_count < OVR_RING_CAPACITY) data->ring_count++;
    }

    // --- Process as many 1024-sample chunks as available ---
    while (data->ring_count >= OVR_CHUNK_SIZE) {
        float chunk[OVR_CHUNK_SIZE];
        float rms_sum = 0.0f;
        for (int i = 0; i < OVR_CHUNK_SIZE; i++) {
            float s = data->ring_buffer[data->ring_read_pos];
            chunk[i] = s;
            rms_sum += s * s;
            data->ring_read_pos = (data->ring_read_pos + 1) % OVR_RING_CAPACITY;
        }
        data->ring_count -= OVR_CHUNK_SIZE;

        data->current_rms = sqrtf(rms_sum / (float)OVR_CHUNK_SIZE);

        // --- Feed exactly 1024 samples to OVR via official SDK shim ---
        ovrLipSyncFrame frame = {};
        frame.visemes                  = data->ovr_viseme_buf;
        frame.visemesLength            = ovrLipSyncViseme_Count;
        frame.laughterCategories       = data->ovr_laugh_buf;
        frame.laughterCategoriesLength = 0;

        ovrLipSyncResult res = ovrLipSync_ProcessFrameEx(
            data->ovr_context,
            chunk,
            OVR_CHUNK_SIZE,
            ovrLipSyncAudioDataType_F32_Mono,
            &frame
        );

        if (res == ovrLipSyncSuccess) {
            float *visemes = data->ovr_viseme_buf;

            // --- VISEME MAPPING (15 -> 6) ---
            float targetFrames[MF_Count] = {0.0f};
            targetFrames[MF_Closed] = visemes[ovrLipSyncViseme_sil] + visemes[ovrLipSyncViseme_PP];
            targetFrames[MF_A]      = visemes[ovrLipSyncViseme_aa]  + visemes[ovrLipSyncViseme_kk];
            targetFrames[MF_E]      = visemes[ovrLipSyncViseme_E]   + visemes[ovrLipSyncViseme_SS] + visemes[ovrLipSyncViseme_DD] + visemes[ovrLipSyncViseme_TH];
            targetFrames[MF_OU]     = visemes[ovrLipSyncViseme_oh]  + visemes[ovrLipSyncViseme_ou] + visemes[ovrLipSyncViseme_CH];
            targetFrames[MF_I]      = visemes[ovrLipSyncViseme_ih]  + visemes[ovrLipSyncViseme_nn] + visemes[ovrLipSyncViseme_RR];
            targetFrames[MF_FV]     = visemes[ovrLipSyncViseme_FF];
            
            // Clamp
            for (int i = 0; i < MF_Count; ++i) {
                if (targetFrames[i] > 1.0f) targetFrames[i] = 1.0f;
            }

            // --- VOLUME GATING ---
            if (!std::isfinite(data->current_rms) || data->current_rms < data->param_gate_lower) {
                for (int i = 0; i < MF_Count; ++i) targetFrames[i] = 0.0f;
                targetFrames[MF_Closed] = 1.0f;
            } else if (data->current_rms > data->param_gate_upper) {
                if (targetFrames[MF_Closed] > 0.0f) {
                    targetFrames[MF_Closed] = 0.0f;
                    float maxOpen = 0.0f;
                    for (int i = 1; i < MF_Count; ++i) { 
                        if (targetFrames[i] > maxOpen) maxOpen = targetFrames[i];
                    }
                    if (maxOpen < 0.1f) targetFrames[MF_I] = 1.0f;
                }
            }

            // --- EXPONENTIAL SMOOTHING (LERP) ---
            int idx = 0;
            float max_val = 0.0f;
            for (int i = 0; i < MF_Count; ++i) {
                if (targetFrames[i] > data->smoothedFrames[i]) {
                    data->smoothedFrames[i] = data->smoothedFrames[i] * (1.0f - data->param_lerp_attack) + targetFrames[i] * data->param_lerp_attack;
                } else {
                    data->smoothedFrames[i] = data->smoothedFrames[i] * (1.0f - data->param_lerp_decay)  + targetFrames[i] * data->param_lerp_decay;
                }
                if (data->smoothedFrames[i] > max_val) {
                    max_val = data->smoothedFrames[i];
                    idx = i;
                }
            }
            data->current_dominant_idx = idx;
        }
    }
}

static void *lipsync_create(obs_data_t *settings, obs_source_t *source)
{
    // Initialize OVR SDK once (shim handles DLL loading internally)
    if (!g_OvrInitialized) {
        g_OvrInitialized = true;
        
        blog(LOG_INFO, "[Flood-Tuber] LipSync: Initializing OVR LipSync via official SDK shim...");
        ovrLipSyncResult initRes = ovrLipSync_Initialize(48000, OVR_CHUNK_SIZE);
        
        if (initRes == ovrLipSyncSuccess) {
            blog(LOG_INFO, "[Flood-Tuber] LipSync: OVR SDK initialized successfully!");
            g_OvrAvailable = true;
        } else {
            blog(LOG_WARNING, "[Flood-Tuber] LipSync: OVR SDK initialization failed! Error: %d. "
                 "Please ensure OVRLipSync.dll is in the correct location.", initRes);
            g_OvrAvailable = false;
        }
    }

	struct flood_tuber_lipsync_data *data = (struct flood_tuber_lipsync_data *)bzalloc(sizeof(struct flood_tuber_lipsync_data));
	data->source = source;
    
    // Default Settings for Physics
    data->param_lerp_attack = 0.8f;
    data->param_lerp_decay = 0.3f;
    data->param_gate_lower = 0.0005f;
    data->param_gate_upper = 0.003f;
    data->blink_duration = 0.15f;
    data->blink_interval_min = 2.0f;
    data->blink_interval_max = 6.0f;
    data->time_until_next_blink = data->blink_interval_min;

    // Create OVR Context if SDK is available
    if (g_OvrAvailable) {
        ovrLipSyncResult res = ovrLipSync_CreateContext(&data->ovr_context, ovrLipSyncContextProvider_Original);
        if (res == ovrLipSyncSuccess) {
            data->context_initialized = true;
            blog(LOG_INFO, "[Flood-Tuber] LipSync: OVR Context created successfully (ctx=%u).", data->ovr_context);
        } else {
            blog(LOG_WARNING, "[Flood-Tuber] LipSync: Context creation failed! Error %d", res);
        }
    }

    // Allocate ring buffer on heap
    data->ring_buffer = (float*)bzalloc(OVR_RING_CAPACITY * sizeof(float));
    data->ring_write_pos = 0;
    data->ring_read_pos  = 0;
    data->ring_count     = 0;

	obs_source_update(source, settings);
	return data;
}

static void lipsync_destroy(void *d)
{
	struct flood_tuber_lipsync_data *data = (struct flood_tuber_lipsync_data *)d;

    if (data->audio_source) {
		obs_source_remove_audio_capture_callback(data->audio_source, lipsync_audio_callback, data);
		obs_source_release(data->audio_source);
	}

    if (data->context_initialized) {
        ovrLipSync_DestroyContext(data->ovr_context);
    }

	obs_enter_graphics();
	data->image_closed.Free();
	data->image_a.Free();
	data->image_e.Free();
	data->image_ou.Free();
	data->image_i.Free();
	data->image_fv.Free();
    data->image_blink.Free();
	obs_leave_graphics();

    if (data->ring_buffer) {
        bfree(data->ring_buffer);
        data->ring_buffer = nullptr;
    }

	bfree(data);
}

// Update Settings: Called when user changes property values
static void lipsync_update(void *data_ptr, obs_data_t *settings)
{
    struct flood_tuber_lipsync_data *data = (struct flood_tuber_lipsync_data *)data_ptr;

	if (data->audio_source) {
		obs_source_remove_audio_capture_callback(data->audio_source, lipsync_audio_callback, data);
		obs_source_release(data->audio_source);
		data->audio_source = NULL;
	}

    // Load images
    update_image(&data->image_closed, obs_data_get_string(settings, "path_closed"));
    update_image(&data->image_a,      obs_data_get_string(settings, "path_a"));
    update_image(&data->image_e,      obs_data_get_string(settings, "path_e"));
    update_image(&data->image_ou,     obs_data_get_string(settings, "path_ou"));
    update_image(&data->image_i,      obs_data_get_string(settings, "path_i"));
    update_image(&data->image_fv,     obs_data_get_string(settings, "path_fv"));
    update_image(&data->image_blink,  obs_data_get_string(settings, "path_blink"));

    // Audio Binding
    const char *audio_source_name = obs_data_get_string(settings, "audio_source");
	obs_source_t *new_audio_source = obs_get_source_by_name(audio_source_name);
	if (new_audio_source) {
		data->audio_source = new_audio_source;
		obs_source_add_audio_capture_callback(data->audio_source, lipsync_audio_callback, data);
	}

    // Parameters
    data->param_lerp_attack = (float)obs_data_get_int(settings, "param_lerp_attack") / 100.0f;
    data->param_lerp_decay  = (float)obs_data_get_int(settings, "param_lerp_decay") / 100.0f;
    data->param_gate_lower  = (float)obs_data_get_double(settings, "param_gate_lower");
    data->param_gate_upper  = (float)obs_data_get_double(settings, "param_gate_upper");
    
    // Blinking Parameters
    data->blink_duration = (float)obs_data_get_int(settings, "blink_duration") / 1000.0f;
	data->blink_interval_min = (float)obs_data_get_int(settings, "blink_interval_min") / 1000.0f;
	data->blink_interval_max = (float)obs_data_get_int(settings, "blink_interval_max") / 1000.0f;

    // Transform
    data->offset_x = (float)obs_data_get_int(settings, "offset_x");
    data->offset_y = (float)obs_data_get_int(settings, "offset_y");
    data->mirror = obs_data_get_bool(settings, "mirror");
}


static void lipsync_tick(void *data_ptr, float seconds)
{
	struct flood_tuber_lipsync_data *data = (struct flood_tuber_lipsync_data *)data_ptr;
	uint64_t elapsed_ns = (uint64_t)(seconds * 1000000000.0f);

    obs_enter_graphics(); 
    flood_image_tick(&data->image_closed, elapsed_ns);
    flood_image_tick(&data->image_a,      elapsed_ns);
    flood_image_tick(&data->image_e,      elapsed_ns);
    flood_image_tick(&data->image_ou,     elapsed_ns);
    flood_image_tick(&data->image_i,      elapsed_ns);
    flood_image_tick(&data->image_fv,     elapsed_ns);
    flood_image_tick(&data->image_blink,  elapsed_ns);
    obs_leave_graphics();

    // Blinking logic
	if (flood_image_get_texture(&data->image_blink)) {
		data->timer_blink += seconds;
		if (!data->is_blinking_now && data->timer_blink >= data->time_until_next_blink) {
			data->is_blinking_now = true;
			data->timer_blink = 0.0f;
		}
		if (data->is_blinking_now && data->timer_blink >= data->blink_duration) {
			data->is_blinking_now = false;
			data->timer_blink = 0.0f;
			int range = (int)(data->blink_interval_max - data->blink_interval_min);
			if (range <= 0) range = 1;
			data->time_until_next_blink = data->blink_interval_min + (rand() % range);
		}
	} else {
		data->is_blinking_now = false;
		data->timer_blink = 0.0f;
	}
}


static void lipsync_render(void *data_ptr, gs_effect_t *effect)
{
	struct flood_tuber_lipsync_data *data = (struct flood_tuber_lipsync_data *)data_ptr;
	gs_texture_t *tex = flood_image_get_texture(&data->image_closed);

    // Pick active animation based on smoothed dominant index
    int idx = data->current_dominant_idx;
    
    // Blinking overlay / override
    if (data->is_blinking_now && flood_image_get_texture(&data->image_blink)) {
        if (idx == MF_Closed) {
            tex = flood_image_get_texture(&data->image_blink);
        }
    }

    if (!data->is_blinking_now || idx != MF_Closed) {
        if      (idx == MF_A)  tex = flood_image_get_texture(&data->image_a);
        else if (idx == MF_E)  tex = flood_image_get_texture(&data->image_e);
        else if (idx == MF_OU) tex = flood_image_get_texture(&data->image_ou);
        else if (idx == MF_I)  tex = flood_image_get_texture(&data->image_i);
        else if (idx == MF_FV) tex = flood_image_get_texture(&data->image_fv);
        
        // Final fallback: if the specific viseme image isn't loaded, default back to Closed
        if (!tex) tex = flood_image_get_texture(&data->image_closed);
    }

	if (tex) {
		gs_matrix_push();
		gs_matrix_translate3f(data->offset_x, data->offset_y, 0.0f);
		if (data->mirror) {
			gs_matrix_translate3f((float)gs_texture_get_width(tex), 0.0f, 0.0f);
			gs_matrix_scale3f(-1.0f, 1.0f, 1.0f);
		}
		obs_source_draw(tex, 0, 0, 0, 0, false);
		gs_matrix_pop();
	}
}

static uint32_t lipsync_get_width(void *data_ptr)
{
	struct flood_tuber_lipsync_data *data = (struct flood_tuber_lipsync_data *)data_ptr;
    uint32_t w = flood_image_get_width(&data->image_closed);
    return w ? w : 500;
}
static uint32_t lipsync_get_height(void *data_ptr)
{
	struct flood_tuber_lipsync_data *data = (struct flood_tuber_lipsync_data *)data_ptr;
    uint32_t h = flood_image_get_height(&data->image_closed);
    return h ? h : 500;
}

static const char *lipsync_get_name(void *unused)
{
	return "Flood Tuber LipSync";
}

#define FILE_FILTER "Image files (*.png *.webp *.gif *.jpg *.jpeg);;PNG Files (*.png);;WebP Files (*.webp);;GIF Files (*.gif);;JPEG Files (*.jpg *.jpeg);;All Files (*.*)"

// GUI Properties (Phase 4)
obs_properties_t *flood_tuber_lipsync_properties(void *data) {
    obs_properties_t *props = obs_properties_create();

    if (!g_OvrAvailable) {
        obs_properties_add_text(props, "error_msg", 
            "[WARNING] OVRLipSync.dll could not be found or loaded!\nLipSync will not function.\n\nPlease place OVRLipSync.dll into your OBS plugins\\flood-tuber directory or data directory and restart OBS.", 
            OBS_TEXT_INFO);
    }

	// Audio Source
	obs_property_t *p_audio_source = obs_properties_add_list(props, "audio_source", "Audio Source (Microphone)", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	auto add_audio_sources = [](void* data, obs_source_t *source) {
		if (obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO) {
			const char *name = obs_source_get_name(source);
			obs_property_list_add_string((obs_property_t*)data, name, name);
		}
		return true;
	};
	obs_property_list_add_string(p_audio_source, "", "");
	obs_enum_sources(add_audio_sources, p_audio_source);

    // Avatar Groups (Visemes)
    obs_properties_t *grp_images = obs_properties_create();
    obs_properties_add_path(grp_images, "path_closed", "1. Closed / Silent (sil, PP)", OBS_PATH_FILE, FILE_FILTER, NULL);
    obs_properties_add_path(grp_images, "path_a", "2. Open A (aa, kk)", OBS_PATH_FILE, FILE_FILTER, NULL);
    obs_properties_add_path(grp_images, "path_e", "3. Horizontal E (E, SS, DD)", OBS_PATH_FILE, FILE_FILTER, NULL);
    obs_properties_add_path(grp_images, "path_ou", "4. Round O/U (oh, ou)", OBS_PATH_FILE, FILE_FILTER, NULL);
    obs_properties_add_path(grp_images, "path_i", "5. Semi-Round I (ih, nn, RR)", OBS_PATH_FILE, FILE_FILTER, NULL);
    obs_properties_add_path(grp_images, "path_fv", "6. Bite F/V (FF)", OBS_PATH_FILE, FILE_FILTER, NULL);
    obs_properties_add_path(grp_images, "path_blink", "[Optional] Blinking (Override)", OBS_PATH_FILE, FILE_FILTER, NULL);
    obs_properties_add_group(props, "grp_images_tab", "Avatar Frames", OBS_GROUP_NORMAL, grp_images);

    // Blinking Settings
    obs_properties_t *grp_blink = obs_properties_create();
    obs_properties_add_int(grp_blink, "blink_interval_min", "Min Interval (ms)", 500, 60000, 100);
	obs_properties_add_int(grp_blink, "blink_interval_max", "Max Interval (ms)", 500, 60000, 100);
	obs_properties_add_int(grp_blink, "blink_duration", "Duration (ms)", 50, 1000, 10);
    obs_properties_add_group(props, "grp_blink_tab", "Blinking System", OBS_GROUP_NORMAL, grp_blink);

    // Smoothing & Settings
    obs_properties_t *grp_smooth = obs_properties_create();
    obs_properties_add_int_slider(grp_smooth, "param_lerp_attack", "Attack Speed (%) (Fast OVR response)", 5, 100, 1);
    obs_properties_add_int_slider(grp_smooth, "param_lerp_decay",  "Decay Speed (%) (Mouth hold duration)",  1, 100, 1);
    obs_properties_add_float_slider(grp_smooth, "param_gate_lower", "RMS Silence Gate", 0.0, 0.05, 0.0001);
    obs_properties_add_float_slider(grp_smooth, "param_gate_upper", "RMS Shout Gate", 0.0, 0.05, 0.0001);
    obs_properties_add_group(props, "grp_smooth_tab", "AI Algorithm & Smoothing", OBS_GROUP_NORMAL, grp_smooth);

    // Positioning
    obs_properties_t *grp_pos = obs_properties_create();
    obs_properties_add_int(grp_pos, "offset_x", "Offset X", -8000, 8000, 1);
	obs_properties_add_int(grp_pos, "offset_y", "Offset Y", -8000, 8000, 1);
	obs_properties_add_bool(grp_pos, "mirror", "Mirror Horizontal");
    obs_properties_add_group(props, "grp_pos_tab", "Transform", OBS_GROUP_NORMAL, grp_pos);

    return props;
}

void flood_tuber_lipsync_defaults(obs_data_t *settings) {
    obs_data_set_default_int(settings, "param_lerp_attack", 80);
    obs_data_set_default_int(settings, "param_lerp_decay", 30);
    obs_data_set_default_double(settings, "param_gate_lower", 0.0005);
    obs_data_set_default_double(settings, "param_gate_upper", 0.0030);

    obs_data_set_default_int(settings, "blink_interval_min", 2000);
	obs_data_set_default_int(settings, "blink_interval_max", 6000);
	obs_data_set_default_int(settings, "blink_duration", 150);

    obs_data_set_default_int(settings, "offset_x", 0);
	obs_data_set_default_int(settings, "offset_y", 0);
	obs_data_set_default_bool(settings, "mirror", false);
}

struct obs_source_info flood_tuber_lipsync_info = {};

// We initialize this struct cleanly outside of the main module load so it can be called from main flood-tuber.cpp
void register_lipsync_source() {
    flood_tuber_lipsync_info.id = "flood_tuber_lipsync_source";
	flood_tuber_lipsync_info.type = OBS_SOURCE_TYPE_INPUT;
	flood_tuber_lipsync_info.output_flags = OBS_SOURCE_VIDEO;
	flood_tuber_lipsync_info.get_name = lipsync_get_name;
	flood_tuber_lipsync_info.create = lipsync_create;
	flood_tuber_lipsync_info.destroy = lipsync_destroy;
	flood_tuber_lipsync_info.update = lipsync_update;
	flood_tuber_lipsync_info.video_render = lipsync_render;
	flood_tuber_lipsync_info.video_tick = lipsync_tick;
	flood_tuber_lipsync_info.get_width = lipsync_get_width;
	flood_tuber_lipsync_info.get_height = lipsync_get_height;
	flood_tuber_lipsync_info.get_properties = flood_tuber_lipsync_properties;
	flood_tuber_lipsync_info.get_defaults = flood_tuber_lipsync_defaults;
	flood_tuber_lipsync_info.icon_type = OBS_ICON_TYPE_AUDIO_INPUT;

    obs_register_source(&flood_tuber_lipsync_info);
}

void unload_lipsync_source() {
    if (g_OvrAvailable) {
        ovrLipSync_Shutdown();
        g_OvrAvailable = false;
        g_OvrInitialized = false;
    }
}
