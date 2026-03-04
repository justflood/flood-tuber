#include "flood-tuber-props.h"
#include "flood-tuber.h"
#include <util/dstr.h>
#include <util/config-file.h>
#include <util/platform.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif


// Enumerate audio sources for the dropdown list
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

// Load avatar images and settings.ini into an obs_data_t object.
// as_default=true  → sets *default* values (plugin init)
// as_default=false → sets *current* values (load via UI)
void apply_avatar_to_settings(obs_data_t *settings, const char *avatar_name, bool as_default)
{
	struct dstr avatar_path = {0};
	dstr_catf(&avatar_path, "avatars/%s", avatar_name);

	auto set_str = [&](const char *key, const char *val) {
		if (as_default) obs_data_set_default_string(settings, key, val);
		else obs_data_set_string(settings, key, val);
	};
	auto set_int = [&](const char *key, long long val) {
		if (as_default) obs_data_set_default_int(settings, key, val);
		else obs_data_set_int(settings, key, val);
	};
	auto set_dbl = [&](const char *key, double val) {
		if (as_default) obs_data_set_default_double(settings, key, val);
		else obs_data_set_double(settings, key, val);
	};
	auto set_img = [&](const char *key, const char *file) {
		char *full_path = obs_module_file(avatar_path.array);
		if (full_path) {
			struct dstr final_path = {0};
			dstr_copy(&final_path, full_path);
			dstr_cat(&final_path, "/");
			dstr_cat(&final_path, file);
			set_str(key, os_file_exists(final_path.array) ? final_path.array : "");
			dstr_free(&final_path);
			bfree(full_path);
		}
	};

	set_img("path_idle",         "idle.png");
	set_img("path_blink",        "blink.png");
	set_img("path_action",       "action.png");
	set_img("path_talk_1",       "talk_a.png");
	set_img("path_talk_2",       "talk_b.png");
	set_img("path_talk_3",       "talk_c.png");
	set_img("path_talk_1_blink", "talk_a_blink.png");
	set_img("path_talk_2_blink", "talk_b_blink.png");
	set_img("path_talk_3_blink", "talk_c_blink.png");

	char *ini_path_raw = obs_module_file(avatar_path.array);
	if (ini_path_raw) {
		struct dstr ini_path = {0};
		dstr_copy(&ini_path, ini_path_raw);
		dstr_cat(&ini_path, "/settings.ini");

		config_t *config = NULL;
		if (config_open(&config, ini_path.array, CONFIG_OPEN_EXISTING) == CONFIG_SUCCESS) {
			auto load_dbl = [&](const char *s, const char *n, const char *k) {
				if (config_has_user_value(config, s, n))
					set_dbl(k, config_get_double(config, s, n));
			};
			auto load_int = [&](const char *s, const char *n, const char *k) {
				if (config_has_user_value(config, s, n))
					set_int(k, config_get_int(config, s, n));
			};
			auto load_str = [&](const char *s, const char *n, const char *k) {
				if (config_has_user_value(config, s, n)) {
					const char *v = config_get_string(config, s, n);
					if (v) set_str(k, v);
				}
			};

			load_dbl("Audio",  "threshold",    "threshold");
			load_int("Audio",  "release_delay","release_delay");
			load_int("Blink",  "duration",     "blink_duration");
			load_int("Blink",  "interval_min", "blink_interval_min");
			load_int("Blink",  "interval_max", "blink_interval_max");
			load_int("Action", "duration",     "action_duration");
			load_int("Action", "interval_min", "action_interval_min");
			load_int("Action", "interval_max", "action_interval_max");
			load_str("Motion", "type",         "motion_type");
			load_int("Motion", "speed",        "motion_speed");
			load_int("Motion", "strength",     "motion_strength");

			if (config_has_user_value(config, "General", "mirror"))
				obs_data_set_bool(settings, "mirror",
					config_get_bool(config, "General", "mirror"));

			config_close(config);
		}
		dstr_free(&ini_path);
		bfree(ini_path_raw);
	}
	dstr_free(&avatar_path);
}

// Set plugin defaults on first load
void flood_tuber_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, "threshold",          -30.0);
	obs_data_set_default_int(settings,    "release_delay",       200);
	obs_data_set_default_double(settings, "talking_speed",        0.10);

	obs_data_set_default_int(settings, "action_duration",        4000);
	obs_data_set_default_int(settings, "action_interval_min",   60000);
	obs_data_set_default_int(settings, "action_interval_max",  120000);
	obs_data_set_default_int(settings, "blink_duration",          150);
	obs_data_set_default_int(settings, "blink_interval_min",     2000);
	obs_data_set_default_int(settings, "blink_interval_max",     8000);

	obs_data_set_default_string(settings, "motion_type",    "Shake");
	obs_data_set_default_int(settings,    "motion_speed",     100);
	obs_data_set_default_int(settings,    "motion_strength",    3);
	obs_data_set_default_bool(settings,   "mirror",          false);

	apply_avatar_to_settings(settings, "Flood Tuber Avatar", true);
	obs_data_set_default_string(settings, "avatar_list", "Flood Tuber Avatar");

	// Group hint text (always-visible info boxes in the properties panel)
	obs_data_set_default_string(settings, "hint_images", obs_module_text("hint_images"));
	obs_data_set_default_string(settings, "hint_audio",  obs_module_text("hint_audio"));
	obs_data_set_default_string(settings, "hint_blink",  obs_module_text("hint_blink"));
	obs_data_set_default_string(settings, "hint_action", obs_module_text("hint_action"));
	obs_data_set_default_string(settings, "hint_motion", obs_module_text("hint_motion"));

	// About info boxes
	obs_data_set_default_string(settings, "about_version", obs_module_text("about_version"));
	obs_data_set_default_string(settings, "about_author",  obs_module_text("about_author"));
}


// --- Button callbacks ---

#define IMPLEMENT_CLEAR_CALLBACK(func_name, setting_name)                         \
static bool func_name(obs_properties_t *props, obs_property_t *p, void *data)    \
{                                                                                  \
	(void)props;                                                               \
	(void)p;                                                                   \
	struct flood_tuber_data *tuber = (struct flood_tuber_data *)data;          \
	if (!tuber || !tuber->source) return false;                                \
	obs_data_t *settings = obs_source_get_settings(tuber->source);             \
	if (!settings) return false;                                               \
	obs_data_set_string(settings, setting_name, "");                           \
	obs_source_update(tuber->source, settings);                                \
	obs_data_release(settings);                                                \
	return true;                                                               \
}

IMPLEMENT_CLEAR_CALLBACK(clear_idle,          "path_idle")
IMPLEMENT_CLEAR_CALLBACK(clear_blink,         "path_blink")
IMPLEMENT_CLEAR_CALLBACK(clear_action,        "path_action")
IMPLEMENT_CLEAR_CALLBACK(clear_talk_1,        "path_talk_1")
IMPLEMENT_CLEAR_CALLBACK(clear_talk_2,        "path_talk_2")
IMPLEMENT_CLEAR_CALLBACK(clear_talk_1_blink,  "path_talk_1_blink")
IMPLEMENT_CLEAR_CALLBACK(clear_talk_2_blink,  "path_talk_2_blink")
IMPLEMENT_CLEAR_CALLBACK(clear_talk_3,        "path_talk_3")
IMPLEMENT_CLEAR_CALLBACK(clear_talk_3_blink,  "path_talk_3_blink")

// Opens the avatars folder in Windows Explorer
static bool open_library_folder(obs_properties_t *props, obs_property_t *p, void *data)
{
	(void)props; (void)p; (void)data;
	char path[512];
	char *module_data_path = obs_module_get_config_path(obs_current_module(), "");
	if (module_data_path) {
		char *data_dir = obs_module_file("avatars");
		if (data_dir) {
			os_get_abs_path(data_dir, path, 512);
			bfree(data_dir);
#ifdef _WIN32
			ShellExecuteA(NULL, "open", path, NULL, NULL, SW_SHOWDEFAULT);
#endif
		}
		bfree(module_data_path);
	}
	return false;
}

// Loads the selected avatar from the library dropdown
static bool load_avatar(obs_properties_t *props, obs_property_t *p, void *data)
{
	(void)props; (void)p;
	struct flood_tuber_data *tuber = (struct flood_tuber_data *)data;
	if (!tuber || !tuber->source) return false;

	obs_data_t *settings = obs_source_get_settings(tuber->source);
	if (!settings) return false;

	const char *selected = obs_data_get_string(settings, "avatar_list");
	if (!selected || !*selected) {
		obs_data_release(settings);
		return false;
	}
	apply_avatar_to_settings(settings, selected, false);
	obs_source_update(tuber->source, settings);
	obs_data_release(settings);
	return true;
}

// Helper: adds a file-picker property for an image slot (tooltip optional)
static void add_file_prop(obs_properties_t *props, const char *name, const char *label,
                          const char *tooltip = nullptr)
{
	obs_property_t *p = obs_properties_add_path(props, name, label, OBS_PATH_FILE,
		"Image Files (*.bmp *.jpg *.jpeg *.tga *.png *.gif *.webp *.apng);;All Files (*.*)", NULL);
	if (tooltip)
		obs_property_set_long_description(p, tooltip);
}

// Opens the Flood Tuber GitHub page
static bool open_github_page(obs_properties_t *props, obs_property_t *p, void *data)
{
	(void)props; (void)p; (void)data;
#ifdef _WIN32
	ShellExecuteA(NULL, "open", "https://github.com/justflood/flood-tuber",
	              NULL, NULL, SW_SHOWNORMAL);
#endif
	return false;
}

// Hides speed/strength sliders when motion effect is "None"
static bool on_motion_type_changed(obs_properties_t *props, obs_property_t *p,
                                   obs_data_t *settings)
{
	(void)p;
	const char *type = obs_data_get_string(settings, "motion_type");
	bool show = type && strcmp(type, "None") != 0;
	obs_property_set_visible(obs_properties_get(props, "motion_speed"),    show);
	obs_property_set_visible(obs_properties_get(props, "motion_strength"), show);
	return true;
}


// Build the OBS properties panel
obs_properties_t *flood_tuber_properties(void *data)
{
	struct flood_tuber_data *tuber = (struct flood_tuber_data *)data;
	obs_properties_t *props = obs_properties_create();

	// ── 1. Avatar Library  (first — typical first step) ───────────────────
	obs_properties_t *lib = obs_properties_create();
	obs_properties_add_group(props, "library_group",
		obs_module_text("library_group"), OBS_GROUP_NORMAL, lib);

	obs_property_t *list = obs_properties_add_list(lib, "avatar_list",
		obs_module_text("avatar_list"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	char *avatars_dir = obs_module_file("avatars");
	if (avatars_dir) {
		os_dir_t *dir = os_opendir(avatars_dir);
		if (dir) {
			struct os_dirent *ent;
			while ((ent = os_readdir(dir)) != NULL)
				if (ent->directory && ent->d_name[0] != '.')
					obs_property_list_add_string(list, ent->d_name, ent->d_name);
			os_closedir(dir);
		}
		bfree(avatars_dir);
	}
	obs_properties_add_button(lib, "load_avatar_btn",
		obs_module_text("load_avatar_btn"), load_avatar);
	obs_properties_add_button(lib, "open_folder_btn",
		obs_module_text("open_folder_btn"), open_library_folder);

	// ── 2. Avatar Images ───────────────────────────────────────────────────
	obs_properties_t *img = obs_properties_create();
	obs_properties_add_group(props, "images_group",
		obs_module_text("images_group"), OBS_GROUP_NORMAL, img);

	const char *clear_txt = obs_module_text("clear_image");

	obs_properties_add_text(img, "hint_images", NULL, OBS_TEXT_INFO);

	// Base states
	add_file_prop(img, "path_idle",   obs_module_text("path_idle"),   obs_module_text("path_idle_tooltip"));
	obs_properties_add_button(img, "clear_idle",   clear_txt, clear_idle);
	add_file_prop(img, "path_blink",  obs_module_text("path_blink"),  obs_module_text("path_blink_tooltip"));
	obs_properties_add_button(img, "clear_blink",  clear_txt, clear_blink);
	add_file_prop(img, "path_action", obs_module_text("path_action"), obs_module_text("path_action_tooltip"));
	obs_properties_add_button(img, "clear_action", clear_txt, clear_action);

	// Talking frames — each frame immediately followed by its blink variant
	add_file_prop(img, "path_talk_1",       obs_module_text("path_talk_1"),       obs_module_text("path_talk_1_tooltip"));
	obs_properties_add_button(img, "clear_talk_1",       clear_txt, clear_talk_1);
	add_file_prop(img, "path_talk_1_blink", obs_module_text("path_talk_1_blink"), obs_module_text("path_talk_1_blink_tooltip"));
	obs_properties_add_button(img, "clear_talk_1_blink", clear_txt, clear_talk_1_blink);

	add_file_prop(img, "path_talk_2",       obs_module_text("path_talk_2"),       obs_module_text("path_talk_2_tooltip"));
	obs_properties_add_button(img, "clear_talk_2",       clear_txt, clear_talk_2);
	add_file_prop(img, "path_talk_2_blink", obs_module_text("path_talk_2_blink"), obs_module_text("path_talk_2_blink_tooltip"));
	obs_properties_add_button(img, "clear_talk_2_blink", clear_txt, clear_talk_2_blink);

	add_file_prop(img, "path_talk_3",       obs_module_text("path_talk_3"),       obs_module_text("path_talk_3_tooltip"));
	obs_properties_add_button(img, "clear_talk_3",       clear_txt, clear_talk_3);
	add_file_prop(img, "path_talk_3_blink", obs_module_text("path_talk_3_blink"), obs_module_text("path_talk_3_blink_tooltip"));
	obs_properties_add_button(img, "clear_talk_3_blink", clear_txt, clear_talk_3_blink);

	// ── 3. Audio & Trigger ─────────────────────────────────────────────────
	obs_properties_t *audio = obs_properties_create();
	obs_properties_add_group(props, "audio_settings",
		obs_module_text("audio_settings"), OBS_GROUP_NORMAL, audio);

	obs_properties_add_text(audio, "hint_audio", NULL, OBS_TEXT_INFO);

	obs_property_t *p_src = obs_properties_add_list(audio, "audio_source",
		obs_module_text("audio_source"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_enum_sources(enum_audio_sources, p_src);

	obs_property_t *p_thresh = obs_properties_add_float_slider(audio,
		"threshold", obs_module_text("threshold"), -60.0f, 0.0f, 0.1f);
	obs_property_set_long_description(p_thresh, obs_module_text("threshold_tooltip"));

	obs_property_t *p_release = obs_properties_add_int_slider(audio,
		"release_delay", obs_module_text("release_delay"), 0, 2000, 10);
	obs_property_set_long_description(p_release, obs_module_text("release_delay_tooltip"));

	obs_property_t *p_spd = obs_properties_add_float_slider(audio,
		"talking_speed", obs_module_text("talking_speed"), 0.01, 1.0, 0.01);
	obs_property_set_long_description(p_spd, obs_module_text("talking_speed_tooltip"));

	// ── 4. Blink Timings ───────────────────────────────────────────────────
	obs_properties_t *blink = obs_properties_create();
	obs_properties_add_group(props, "blink_settings",
		obs_module_text("blink_settings"), OBS_GROUP_NORMAL, blink);

	obs_properties_add_text(blink, "hint_blink", NULL, OBS_TEXT_INFO);

	obs_property_t *p_bd = obs_properties_add_int(blink,
		"blink_duration", obs_module_text("blink_duration"), 0, 100000, 1);
	obs_property_set_long_description(p_bd, obs_module_text("blink_duration_tooltip"));

	obs_property_t *p_bmin = obs_properties_add_int(blink,
		"blink_interval_min", obs_module_text("blink_interval_min"), 0, 600000, 100);
	obs_property_set_long_description(p_bmin, obs_module_text("blink_interval_min_tooltip"));

	obs_property_t *p_bmax = obs_properties_add_int(blink,
		"blink_interval_max", obs_module_text("blink_interval_max"), 0, 600000, 100);
	obs_property_set_long_description(p_bmax, obs_module_text("blink_interval_max_tooltip"));

	// ── 5. Idle Action Timings ─────────────────────────────────────────────
	obs_properties_t *action = obs_properties_create();
	obs_properties_add_group(props, "action_settings",
		obs_module_text("action_settings"), OBS_GROUP_NORMAL, action);

	obs_properties_add_text(action, "hint_action", NULL, OBS_TEXT_INFO);

	obs_property_t *p_adur = obs_properties_add_int(action,
		"action_duration", obs_module_text("action_duration"), 0, 600000, 100);
	obs_property_set_long_description(p_adur, obs_module_text("action_duration_tooltip"));

	obs_property_t *p_amin = obs_properties_add_int(action,
		"action_interval_min", obs_module_text("action_interval_min"), 0, 3600000, 1000);
	obs_property_set_long_description(p_amin, obs_module_text("action_interval_min_tooltip"));

	obs_property_t *p_amax = obs_properties_add_int(action,
		"action_interval_max", obs_module_text("action_interval_max"), 0, 3600000, 1000);
	obs_property_set_long_description(p_amax, obs_module_text("action_interval_max_tooltip"));

	// ── 6. Talking Motion ─────────────────────────────────────────────────
	obs_properties_t *motion = obs_properties_create();
	obs_properties_add_group(props, "motion_settings",
		obs_module_text("motion_settings"), OBS_GROUP_NORMAL, motion);

	obs_properties_add_text(motion, "hint_motion", NULL, OBS_TEXT_INFO);

	obs_property_t *p_mtype = obs_properties_add_list(motion, "motion_type",
		obs_module_text("motion_type"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p_mtype, "None",   "None");
	obs_property_list_add_string(p_mtype, "Bounce", "Bounce");
	obs_property_list_add_string(p_mtype, "Shake",  "Shake");
	obs_property_set_long_description(p_mtype, obs_module_text("motion_type_tooltip"));
	obs_property_set_modified_callback(p_mtype, on_motion_type_changed);

	obs_property_t *p_mspeed = obs_properties_add_int_slider(motion,
		"motion_speed", obs_module_text("motion_speed"), 0, 500, 10);
	obs_property_set_long_description(p_mspeed, obs_module_text("motion_speed_tooltip"));

	obs_property_t *p_mstrength = obs_properties_add_int_slider(motion,
		"motion_strength", obs_module_text("motion_strength"), 0, 200, 1);
	obs_property_set_long_description(p_mstrength, obs_module_text("motion_strength_tooltip"));

	obs_properties_add_bool(motion, "mirror", obs_module_text("mirror_label"));

	// Apply initial visibility for speed/strength based on current settings
	if (tuber && tuber->source) {
		obs_data_t *settings = obs_source_get_settings(tuber->source);
		if (settings) {
			on_motion_type_changed(motion, p_mtype, settings);
			obs_data_release(settings);
		}
	}

	// ── 7. About ──────────────────────────────────────────────────────────
	obs_properties_t *about = obs_properties_create();
	obs_properties_add_group(props, "about_group",
		obs_module_text("about_group"), OBS_GROUP_NORMAL, about);

	obs_properties_add_text(about, "about_version", NULL, OBS_TEXT_INFO);
	obs_properties_add_text(about, "about_author",  NULL, OBS_TEXT_INFO);
	obs_properties_add_button(about, "about_github_btn",
		obs_module_text("about_github_btn"), open_github_page);

	return props;
}
