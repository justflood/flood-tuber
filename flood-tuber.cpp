#include "flood-tuber.h"
#include "flood-tuber-props.h"
#include <util/dstr.h>
#include <math.h>
#include <util/platform.h>

#ifdef _WIN32
#define STRCMPI _strcmpi
#else
#include <strings.h>
#define STRCMPI strcasecmp
#endif



// Validate file headers to prevent crashes (e.g. renamed .txt files)
static bool check_file_signature(const char *path) {
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
    if (ext && STRCMPI(ext, ".gif") == 0) {
        return false; // Extension says GIF but header doesn't match
    }

    return true; // Let OBS try other formats (BMP, TGA etc) if not explicitly suspicious
}

// Loads an image file and initializes its texture
static void update_image(FloodImage *image, const char *path)
{
	obs_enter_graphics();
	image->Free();
    
    if (path && *path) {
        if (!check_file_signature(path)) {
            blog(LOG_WARNING, "Invalid file signature (corrupt or fake file?): %s", path);
        } else {
            const char *ext = strrchr(path, '.');
            bool is_webp = (ext && (STRCMPI(ext, ".webp") == 0));
            
            if (is_webp) {
                image->type = FloodImage::CUSTOM_WEBP;
                image->webp_decoder = new WebPDecoder();
                if (!image->webp_decoder->Load(path)) {
                     blog(LOG_WARNING, "Failed to load WebP: %s", path);
                     image->Free();
                } else {
                     blog(LOG_INFO, "Loaded WebP: %s", path);
                }
            } else if (ext && (STRCMPI(ext, ".apng") == 0 || STRCMPI(ext, ".png") == 0)) {
                // Check if it's an animated PNG
                APNGDecoder *temp_decoder = new APNGDecoder();
                if (temp_decoder->Load(path) && temp_decoder->IsAnimated()) {
                     image->type = FloodImage::CUSTOM_APNG;
                     image->apng_decoder = temp_decoder;
                     blog(LOG_INFO, "Loaded Animated PNG: %s", path);
                } else {
                     if (temp_decoder->IsAnimated())
                         blog(LOG_WARNING, "Failed to load APNG (corrupt?): %s", path);
                     
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

static void flood_image_tick(FloodImage *img, uint64_t elapsed_ns) {
    if (img->type == FloodImage::CUSTOM_WEBP) {
        if (img->webp_decoder) {
            img->anim_time_ns += elapsed_ns;
        }
    } else if (img->type == FloodImage::CUSTOM_APNG) {
        if (img->apng_decoder) {
            img->anim_time_ns += elapsed_ns;
            // Trace animation time occasionally (approx every 1s at 60fps)
            static int log_counter = 0;
            if (++log_counter > 60) {
                 // BLOG(LOG_DEBUG, "APNG Tick: Time=%llu ms", img->anim_time_ns / 1000000);
                 log_counter = 0;
            }
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
    if (img->type == FloodImage::CUSTOM_WEBP && img->webp_decoder) {
        return img->webp_decoder->GetWidth();
    }
    if (img->type == FloodImage::CUSTOM_APNG && img->apng_decoder) {
        return img->apng_decoder->GetWidth();
    }
    return img->obs_image.cx;
}

static uint32_t flood_image_get_height(FloodImage *img) {
    if (img->type == FloodImage::CUSTOM_WEBP && img->webp_decoder) {
        return img->webp_decoder->GetHeight();
    }
    if (img->type == FloodImage::CUSTOM_APNG && img->apng_decoder) {
        return img->apng_decoder->GetHeight();
    }
    return img->obs_image.cy;
}



// Callback: Processes audio data to calculate volume levels (dB)
static void audio_callback(void *data_ptr, obs_source_t *source, const struct audio_data *audio_data, bool muted)
{
	struct flood_tuber_data *data = (struct flood_tuber_data *)data_ptr;
	UNUSED_PARAMETER(source);
	if (muted) {
		data->current_db = -100.0f;
		return;
	}

	float max_sample = 0.0f;
	for (size_t c = 0; c < MAX_AV_PLANES; c++) {
		if (!audio_data->data[c])
			break;
		const float *samples = (const float *)audio_data->data[c];
		for (size_t i = 0; i < audio_data->frames; i++) {
			float sample = fabsf(samples[i]);
			if (sample > max_sample)
				max_sample = sample;
		}
	}
	if (max_sample > 0.0f)
		data->current_db = 20.0f * log10f(max_sample);
	else
		data->current_db = -100.0f;
}



// Plugin Init: Allocates memory and initializes the plugin state
static void *flood_tuber_create(obs_data_t *settings, obs_source_t *source)
{
	struct flood_tuber_data *data = (struct flood_tuber_data *)bzalloc(sizeof(struct flood_tuber_data));
	data->source = source;
	data->current_db = -100.0f;
	data->current_state = AvatarState::IDLE;
	data->talking_frame_index = 0;
	data->timer_release_hold = 0.0f;

	obs_source_update(source, settings);
	return data;
}


// Plugin Cleanup: Frees memory and releases resources
static void flood_tuber_destroy(void *d)
{
	struct flood_tuber_data *data = (struct flood_tuber_data *)d;

	if (data->audio_source) {
		obs_source_remove_audio_capture_callback(data->audio_source, audio_callback, data);
		obs_source_release(data->audio_source);
	}
	obs_enter_graphics();
	data->image_idle.Free();
	data->image_blink.Free();
	data->image_action.Free();
	data->image_talking_1.Free();
	data->image_talking_2.Free();
	data->image_talking_3.Free();
	data->image_talking_1_blink.Free();
	data->image_talking_2_blink.Free();
	data->image_talking_3_blink.Free();
	obs_leave_graphics();
	bfree(data);
}


// Update Settings: Called when user changes property values
static void flood_tuber_update(void *data_ptr, obs_data_t *settings)
{
	struct flood_tuber_data *data = (struct flood_tuber_data *)data_ptr;

	if (data->audio_source) {
		obs_source_remove_audio_capture_callback(data->audio_source, audio_callback, data);
		obs_source_release(data->audio_source);
		data->audio_source = NULL;
	}

	update_image(&data->image_idle, obs_data_get_string(settings, "path_idle"));
	update_image(&data->image_blink, obs_data_get_string(settings, "path_blink"));
	update_image(&data->image_action, obs_data_get_string(settings, "path_action"));
	update_image(&data->image_talking_1, obs_data_get_string(settings, "path_talk_1"));
	update_image(&data->image_talking_2, obs_data_get_string(settings, "path_talk_2"));
	update_image(&data->image_talking_3, obs_data_get_string(settings, "path_talk_3"));
	update_image(&data->image_talking_1_blink, obs_data_get_string(settings, "path_talk_1_blink"));
	update_image(&data->image_talking_2_blink, obs_data_get_string(settings, "path_talk_2_blink"));
	update_image(&data->image_talking_3_blink, obs_data_get_string(settings, "path_talk_3_blink"));

	data->threshold = (float)obs_data_get_double(settings, "threshold");
	data->release_delay = (float)obs_data_get_int(settings, "release_delay") / 1000.0f; // ms to seconds


	data->action_duration = (float)obs_data_get_int(settings, "action_duration") / 1000.0f;
	data->action_interval_min = (float)obs_data_get_int(settings, "action_interval_min") / 1000.0f;
	data->action_interval_max = (float)obs_data_get_int(settings, "action_interval_max") / 1000.0f;

	data->blink_duration = (float)obs_data_get_int(settings, "blink_duration") / 1000.0f;
	data->blink_interval_min = (float)obs_data_get_int(settings, "blink_interval_min") / 1000.0f;
	data->blink_interval_max = (float)obs_data_get_int(settings, "blink_interval_max") / 1000.0f;
	
	const char *motion_type = obs_data_get_string(settings, "motion_type");
	if (strcmp(motion_type, "Bounce") == 0)
		data->talk_effect = TalkingEffect::BOUNCE;
	else if (strcmp(motion_type, "Shake") == 0)
		data->talk_effect = TalkingEffect::SHAKE;
	else
		data->talk_effect = TalkingEffect::NONE;

	data->effect_speed = (float)obs_data_get_int(settings, "motion_speed") / 100.0f;
	data->effect_strength = (float)obs_data_get_int(settings, "motion_strength");
	data->mirror = obs_data_get_bool(settings, "mirror");
	data->talk_interval = (float)obs_data_get_double(settings, "talking_speed");
	if (data->talk_interval < 0.01f) data->talk_interval = 0.01f;

	const char *audio_source_name = obs_data_get_string(settings, "audio_source");
	obs_source_t *new_audio_source = obs_get_source_by_name(audio_source_name);
	if (new_audio_source) {
		data->audio_source = new_audio_source;
		obs_source_add_audio_capture_callback(data->audio_source, audio_callback, data);
	}

	data->time_until_next_action = data->action_interval_min;
	data->time_until_next_blink = data->blink_interval_min;
}

// Main Tick: Handles state logic (Idle/Talking switching, timers for blinking and actions)
static void flood_tuber_tick(void *data_ptr, float seconds)
{
	struct flood_tuber_data *data = (struct flood_tuber_data *)data_ptr;

	// Tick animations for all images
	// Convert seconds to nanoseconds for gs_image_file_tick
	uint64_t elapsed_ns = (uint64_t)(seconds * 1000000000.0f);

    obs_enter_graphics(); // Required for standard texture updates
	
    flood_image_tick(&data->image_idle, elapsed_ns);
	flood_image_tick(&data->image_blink, elapsed_ns);
	flood_image_tick(&data->image_action, elapsed_ns);
	flood_image_tick(&data->image_talking_1, elapsed_ns);
	flood_image_tick(&data->image_talking_2, elapsed_ns);
	flood_image_tick(&data->image_talking_3, elapsed_ns);
	flood_image_tick(&data->image_talking_1_blink, elapsed_ns);
	flood_image_tick(&data->image_talking_2_blink, elapsed_ns);
	flood_image_tick(&data->image_talking_3_blink, elapsed_ns);
    
    obs_leave_graphics();
	
	float magnitude = data->current_db;
	bool raw_talking = (magnitude > data->threshold) && (magnitude > -95.0f);

	if (raw_talking) {
		data->timer_release_hold = 0.0f;
		data->current_state = AvatarState::TALKING;
	} else {
		if (data->current_state == AvatarState::TALKING) {
			data->timer_release_hold += seconds;
			if (data->timer_release_hold > data->release_delay) {
				data->current_state = AvatarState::IDLE;
			}
		}
	}

	if (data->current_state == AvatarState::TALKING) {
		data->timer_talk_anim += seconds;
		if (data->timer_talk_anim > data->talk_interval) {
			data->talking_frame_index = (data->talking_frame_index + 1) % 3;
			data->timer_talk_anim = 0.0f;
		}
	}

	if (data->current_state != AvatarState::TALKING) {
		if (data->current_state == AvatarState::ACTION) {
			data->timer_action += seconds;
			if (data->timer_action >= data->action_duration) {
				data->current_state = AvatarState::IDLE;
				data->timer_action = 0.0f;
				int range = (int)(data->action_interval_max - data->action_interval_min);
				if (range <= 0)
					range = 1;
				data->time_until_next_action = data->action_interval_min + (rand() % range);
			}
		} else {
			data->timer_action += seconds;
			if (data->timer_action >= data->time_until_next_action) {
				data->current_state = AvatarState::ACTION;
				data->timer_action = 0.0f;
			}
		}
	}

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
			if (range <= 0)
				range = 1;
			data->time_until_next_blink = data->blink_interval_min + (rand() % range);
		}
	} else {
		data->is_blinking_now = false;
		data->timer_blink = 0.0f;
	}

	data->offset_x = 0.0f;
	data->offset_y = 0.0f;

	if (data->current_state == AvatarState::TALKING && data->talk_effect != TalkingEffect::NONE) {
		data->timer_effect += seconds * data->effect_speed;
		
		if (data->talk_effect == TalkingEffect::BOUNCE) {
			data->offset_y = -fabsf(sinf(data->timer_effect * 5.0f)) * data->effect_strength;
		} 
		else if (data->talk_effect == TalkingEffect::SHAKE) {
			data->offset_x = sinf(data->timer_effect * 25.0f) * data->effect_strength * 0.8f;
			data->offset_y = cosf(data->timer_effect * 20.0f) * data->effect_strength * 0.8f;
		}
	} else {
		data->timer_effect = 0.0f;
	}
}

// Render: Selects and draws the correct texture based on current state (Talking, Idle, Blink, Action)
static void flood_tuber_render(void *data_ptr, gs_effect_t *effect)
{
	struct flood_tuber_data *data = (struct flood_tuber_data *)data_ptr;
	UNUSED_PARAMETER(effect);
	gs_texture_t *tex = flood_image_get_texture(&data->image_idle);

	if (data->current_state == AvatarState::ACTION && flood_image_get_texture(&data->image_action)) {
		tex = flood_image_get_texture(&data->image_action);
	} 
	else if (data->current_state == AvatarState::TALKING) {
		bool blink = data->is_blinking_now;
		
		int idx = data->talking_frame_index;
		FloodImage *talk_img = nullptr;
		FloodImage *talk_blink_img = nullptr;

		// Select based on index
		if (idx == 0) {
			talk_img = &data->image_talking_1;
			talk_blink_img = &data->image_talking_1_blink;
		} else if (idx == 1) {
			talk_img = &data->image_talking_2;
			talk_blink_img = &data->image_talking_2_blink;
		} else {
			talk_img = &data->image_talking_3;
			talk_blink_img = &data->image_talking_3_blink;
		}

		// Fallbacks
		if (!flood_image_get_texture(talk_img) && idx == 2) {
             talk_img = &data->image_talking_2;
             talk_blink_img = &data->image_talking_2_blink;
             idx = 1;
        }
		if (!flood_image_get_texture(talk_img) && idx == 1) {
             talk_img = &data->image_talking_1;
             talk_blink_img = &data->image_talking_1_blink;
        }

		// Determine final texture
		if (blink && talk_blink_img && flood_image_get_texture(talk_blink_img)) {
			tex = flood_image_get_texture(talk_blink_img);
		} else if (talk_img && flood_image_get_texture(talk_img)) {
			tex = flood_image_get_texture(talk_img);
		}

		// Final fallback for blinking if specific talk-blink is missing
		if (blink && (!talk_blink_img || !flood_image_get_texture(talk_blink_img)) && flood_image_get_texture(&data->image_blink)) {
			tex = flood_image_get_texture(&data->image_blink);
		}
	} 
	else {
		if (data->is_blinking_now && flood_image_get_texture(&data->image_blink)) {
			tex = flood_image_get_texture(&data->image_blink);
		} else {
			tex = flood_image_get_texture(&data->image_idle);
		}
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

static uint32_t flood_tuber_get_width(void *data_ptr)
{
	struct flood_tuber_data *data = (struct flood_tuber_data *)data_ptr;
    uint32_t w = flood_image_get_width(&data->image_idle);
	return w ? w : 500;
}
static uint32_t flood_tuber_get_height(void *data_ptr)
{
	struct flood_tuber_data *data = (struct flood_tuber_data *)data_ptr;
    uint32_t h = flood_image_get_height(&data->image_idle);
	return h ? h : 500;
}
static const char *flood_tuber_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Flood Tuber Avatar";
}



struct obs_source_info flood_tuber_info = {};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("flood-tuber", "en-US")

bool obs_module_load(void)
{
	flood_tuber_info.id = "flood_tuber_source";
	flood_tuber_info.type = OBS_SOURCE_TYPE_INPUT;
	flood_tuber_info.output_flags = OBS_SOURCE_VIDEO;
	flood_tuber_info.get_name = flood_tuber_get_name;
	flood_tuber_info.create = flood_tuber_create;
	flood_tuber_info.destroy = flood_tuber_destroy;
	flood_tuber_info.update = flood_tuber_update;
	flood_tuber_info.video_render = flood_tuber_render;
	flood_tuber_info.video_tick = flood_tuber_tick;
	flood_tuber_info.get_width = flood_tuber_get_width;
	flood_tuber_info.get_height = flood_tuber_get_height;
	flood_tuber_info.get_properties = flood_tuber_properties;
	flood_tuber_info.get_defaults = flood_tuber_defaults;
	flood_tuber_info.icon_type = OBS_ICON_TYPE_AUDIO_INPUT;

	obs_register_source(&flood_tuber_info);
	blog(LOG_INFO, "[Flood-Tuber] v" FLOOD_TUBER_VERSION " loaded. (Build: " __DATE__ " " __TIME__ ")");
	return true;
}
//.obs_module_unload
void obs_module_unload(void) {}
