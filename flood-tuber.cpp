#include "flood-tuber.h"
#include <util/dstr.h>
#include <math.h>


// Helper: Loads an image file and initializes its texture
static void update_image(gs_image_file_t *image, const char *path)
{
	obs_enter_graphics();
	gs_image_file_free(image);
	gs_image_file_init(image, path);
	gs_image_file_init_texture(image);
	obs_leave_graphics();
}


static bool enum_audio_sources(void *data, obs_source_t *source)
{
	obs_property_t *list = (obs_property_t *)data;
	uint32_t flags = obs_source_get_output_flags(source);
	if (flags & OBS_SOURCE_AUDIO) {
		const char *name = obs_source_get_name(source);
		obs_property_list_add_string(list, name, name);
	}
	return true;
}

// Callback: Processes audio data to calculate volume levels (dB)
static void audio_callback(void *data_ptr, obs_source_t *source, const struct audio_data *audio_data, bool muted)
{
	struct flood_tuber_data *data = (struct flood_tuber_data *)data_ptr;
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

static void flood_tuber_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, "threshold", -30.0);
	obs_data_set_default_int(settings, "release_delay", 200);

	obs_data_set_default_int(settings, "action_duration", 4000);
	obs_data_set_default_int(settings, "action_interval_min", 60000);
	obs_data_set_default_int(settings, "action_interval_max", 120000);
	obs_data_set_default_int(settings, "blink_duration", 150);
	obs_data_set_default_int(settings, "blink_interval_min", 2000);
	obs_data_set_default_int(settings, "blink_interval_max", 8000);

	obs_data_set_default_string(settings, "motion_type", "Shake");
	obs_data_set_default_int(settings, "motion_speed", 100);
	obs_data_set_default_int(settings, "motion_strength", 3);
}


// Plugin Init: Allocates memory and initializes the plugin state
static void *flood_tuber_create(obs_data_t *settings, obs_source_t *source)
{
	struct flood_tuber_data *data = (struct flood_tuber_data *)bzalloc(sizeof(struct flood_tuber_data));
	data->source = source;
	data->current_db = -100.0f;
	data->current_state = AvatarState::IDLE;
	data->is_talking_frame_1 = true;
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
	gs_image_file_free(&data->image_idle);
	gs_image_file_free(&data->image_blink);
	gs_image_file_free(&data->image_action);
	gs_image_file_free(&data->image_talking_1);
	gs_image_file_free(&data->image_talking_2);
	gs_image_file_free(&data->image_talking_1_blink);
	gs_image_file_free(&data->image_talking_2_blink);
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
	update_image(&data->image_talking_1_blink, obs_data_get_string(settings, "path_talk_1_blink"));
	update_image(&data->image_talking_2_blink, obs_data_get_string(settings, "path_talk_2_blink"));

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
		if (data->timer_talk_anim > 0.10f) {
			data->is_talking_frame_1 = !data->is_talking_frame_1;
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

	if (data->image_blink.texture) {
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
	gs_texture_t *tex = data->image_idle.texture;

	if (data->current_state == AvatarState::ACTION && data->image_action.texture) {
		tex = data->image_action.texture;
	} 
	else if (data->current_state == AvatarState::TALKING) {
		bool blink = data->is_blinking_now;
		
		if (data->is_talking_frame_1) {
			if (blink && data->image_talking_1_blink.texture) {
				tex = data->image_talking_1_blink.texture;
			} else if (data->image_talking_1.texture) {
				tex = data->image_talking_1.texture;
			}
			if (blink && !data->image_talking_1_blink.texture && data->image_blink.texture) {
				tex = data->image_blink.texture;
			}
		} else {
			if (blink && data->image_talking_2_blink.texture) {
				tex = data->image_talking_2_blink.texture;
			} else if (data->image_talking_2.texture) {
				tex = data->image_talking_2.texture;
			} else if (data->image_talking_1.texture) {
				tex = data->image_talking_1.texture;
			}

			if (blink && !data->image_talking_2_blink.texture && data->image_blink.texture) {
				tex = data->image_blink.texture;
			}
		}
	} 
	else {
		if (data->is_blinking_now && data->image_blink.texture) {
			tex = data->image_blink.texture;
		} else {
			tex = data->image_idle.texture;
		}
	}

	if (tex) {
		gs_matrix_push();
		gs_matrix_translate3f(data->offset_x, data->offset_y, 0.0f);
		obs_source_draw(tex, 0, 0, 0, 0, false);
		gs_matrix_pop();
	}
}

static uint32_t flood_tuber_get_width(void *data_ptr)
{
	struct flood_tuber_data *data = (struct flood_tuber_data *)data_ptr;
	return data->image_idle.cx ? data->image_idle.cx : 500;
}
static uint32_t flood_tuber_get_height(void *data_ptr)
{
	struct flood_tuber_data *data = (struct flood_tuber_data *)data_ptr;
	return data->image_idle.cy ? data->image_idle.cy : 500;
}
static const char *flood_tuber_get_name(void *unused)
{
	return "Flood Tuber Avatar";
}

// Macro to implement a simple clear-path callback
#define IMPLEMENT_CLEAR_CALLBACK(func_name, setting_name) \
static bool func_name(obs_properties_t *props, obs_property_t *p, void *data) \
{ \
	(void)props; \
	(void)p; \
	struct flood_tuber_data *tuber = (struct flood_tuber_data *)data; \
	if (!tuber || !tuber->source) return false; \
	obs_data_t *settings = obs_source_get_settings(tuber->source); \
	if (!settings) return false; \
	obs_data_set_string(settings, setting_name, ""); \
	obs_source_update(tuber->source, settings); \
	obs_data_release(settings); \
	return true; \
}

IMPLEMENT_CLEAR_CALLBACK(clear_idle,        "path_idle")
IMPLEMENT_CLEAR_CALLBACK(clear_blink,       "path_blink")
IMPLEMENT_CLEAR_CALLBACK(clear_action,      "path_action")
IMPLEMENT_CLEAR_CALLBACK(clear_talk_1,      "path_talk_1")
IMPLEMENT_CLEAR_CALLBACK(clear_talk_2,      "path_talk_2")
IMPLEMENT_CLEAR_CALLBACK(clear_talk_1_blink,"path_talk_1_blink")
IMPLEMENT_CLEAR_CALLBACK(clear_talk_2_blink,"path_talk_2_blink")

static void add_file_prop(obs_properties_t *props, const char *name, const char *desc)
{
	obs_properties_add_path(props, name, desc, OBS_PATH_FILE,
				"Image Files (*.bmp *.jpg *.jpeg *.tga *.png *.gif);;All Files (*.*)", NULL);
}

// Properties: Defines the configuration UI in OBS
static obs_properties_t *flood_tuber_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_group(props, "images_group", obs_module_text("images_group"), OBS_GROUP_NORMAL, props);
	
	const char *clear_txt = obs_module_text("clear_image");

	add_file_prop(props, "path_idle", obs_module_text("path_idle"));
	obs_properties_add_button(props, "clear_idle", clear_txt, clear_idle);

	add_file_prop(props, "path_blink", obs_module_text("path_blink"));
	obs_properties_add_button(props, "clear_blink", clear_txt, clear_blink);

	add_file_prop(props, "path_action", obs_module_text("path_action"));
	obs_properties_add_button(props, "clear_action", clear_txt, clear_action);

	add_file_prop(props, "path_talk_1", obs_module_text("path_talk_1"));
	obs_properties_add_button(props, "clear_talk_1", clear_txt, clear_talk_1);

	add_file_prop(props, "path_talk_2", obs_module_text("path_talk_2"));
	obs_properties_add_button(props, "clear_talk_2", clear_txt, clear_talk_2);

	add_file_prop(props, "path_talk_1_blink", obs_module_text("path_talk_1_blink"));
	obs_properties_add_button(props, "clear_talk_1_blink", clear_txt, clear_talk_1_blink);

	add_file_prop(props, "path_talk_2_blink", obs_module_text("path_talk_2_blink"));
	obs_properties_add_button(props, "clear_talk_2_blink", clear_txt, clear_talk_2_blink);

	obs_properties_t *audio_group = obs_properties_create();
	obs_properties_add_group(props, "audio_settings", obs_module_text("audio_settings"), OBS_GROUP_NORMAL, audio_group);

	obs_property_t *p = obs_properties_add_list(props, "audio_source", obs_module_text("audio_source"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_enum_sources(enum_audio_sources, p);

	obs_property_t *p_thresh = obs_properties_add_float_slider(props, "threshold", obs_module_text("threshold"), -60.0f, 0.0f, 0.1f);
	obs_property_set_long_description(p_thresh, obs_module_text("threshold_tooltip"));

	obs_property_t *p_release = obs_properties_add_int(props, "release_delay", obs_module_text("release_delay"), 0, 10000, 10);
	obs_property_set_long_description(p_release, obs_module_text("release_delay_tooltip"));

	obs_properties_t *blink_group = obs_properties_create();
	obs_properties_add_group(props, "blink_settings", obs_module_text("blink_settings"), OBS_GROUP_NORMAL, blink_group);
	
	obs_property_t *p_bd = obs_properties_add_int(props, "blink_duration", obs_module_text("blink_duration"), 0, 100000, 1);
	obs_property_set_long_description(p_bd, obs_module_text("blink_duration_tooltip"));

	obs_property_t *p_bmin = obs_properties_add_int(props, "blink_interval_min", obs_module_text("blink_interval_min"), 0, 600000, 1);
	obs_property_set_long_description(p_bmin, obs_module_text("blink_interval_min_tooltip"));

	obs_property_t *p_bmax = obs_properties_add_int(props, "blink_interval_max", obs_module_text("blink_interval_max"), 0, 600000, 1);
	obs_property_set_long_description(p_bmax, obs_module_text("blink_interval_max_tooltip"));
	
	obs_properties_t *action_group = obs_properties_create();
	obs_properties_add_group(props, "action_settings", obs_module_text("action_settings"), OBS_GROUP_NORMAL, action_group);
	
	obs_property_t *p_adur = obs_properties_add_int(props, "action_duration", obs_module_text("action_duration"), 0, 600000, 1);
	obs_property_set_long_description(p_adur, obs_module_text("action_duration_tooltip"));

	obs_property_t *p_amin = obs_properties_add_int(props, "action_interval_min", obs_module_text("action_interval_min"), 0, 3600000, 1);
	obs_property_set_long_description(p_amin, obs_module_text("action_interval_min_tooltip"));

	obs_property_t *p_amax = obs_properties_add_int(props, "action_interval_max", obs_module_text("action_interval_max"), 0, 3600000, 1);
	obs_property_set_long_description(p_amax, obs_module_text("action_interval_max_tooltip"));

	obs_properties_t *motion_group = obs_properties_create();
	obs_properties_add_group(props, "motion_settings", obs_module_text("motion_settings"), OBS_GROUP_NORMAL, motion_group);

	obs_property_t *p_mtype = obs_properties_add_list(props, "motion_type", obs_module_text("motion_type"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p_mtype, "None", "None");
	obs_property_list_add_string(p_mtype, "Bounce", "Bounce");
	obs_property_list_add_string(p_mtype, "Shake", "Shake");
	obs_property_set_long_description(p_mtype, obs_module_text("motion_type_tooltip"));

	obs_property_t *p_mspeed = obs_properties_add_int(props, "motion_speed", obs_module_text("motion_speed"), 0, 500, 10);
	obs_property_set_long_description(p_mspeed, obs_module_text("motion_speed_tooltip"));

	obs_property_t *p_mstrength = obs_properties_add_int(props, "motion_strength", obs_module_text("motion_strength"), 0, 200, 1);
	obs_property_set_long_description(p_mstrength, obs_module_text("motion_strength_tooltip"));

	return props;
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
	blog(LOG_INFO, "[Flood-Tuber] Loaded successfully!");
	return true;
}

void obs_module_unload(void) {}
