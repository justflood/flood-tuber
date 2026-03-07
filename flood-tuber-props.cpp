#include "flood-tuber-props.h"
#include "flood-tuber.h"
#include <util/dstr.h>
#include <util/config-file.h>
#include <util/platform.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

// Returns the custom avatars folder path from source settings.
// Returns NULL if not set. Do NOT free the returned pointer.
static const char *get_custom_dir(obs_data_t *settings)
{
	const char *path = obs_data_get_string(settings, "custom_avatars_path");
	return (path && *path) ? path : NULL;
}

// Resolves full path for a given avatar name.
// Checks custom dir first, falls back to built-in data dir.
// Caller must bfree() the returned pointer.
static char *resolve_avatar_dir(const char *avatar_name, obs_data_t *settings)
{
	// 1. Check custom dir first
	const char *custom = get_custom_dir(settings);
	if (custom) {
		struct dstr user_path = {0};
		dstr_copy(&user_path, custom);
		dstr_cat(&user_path, "/");
		dstr_cat(&user_path, avatar_name);

		// Only use custom dir if it has actual image files
		struct dstr idle_check = {0};
		dstr_copy(&idle_check, user_path.array);
		dstr_cat(&idle_check, "/idle.png");
		bool has_images = os_file_exists(idle_check.array);
		dstr_free(&idle_check);

		if (has_images) {
			BLOG(LOG_DEBUG, "resolve: found custom avatar: %s", user_path.array);
			char *result = bstrdup(user_path.array);
			dstr_free(&user_path);
			return result;
		}
		dstr_free(&user_path);
	}

	// 2. Fall back to built-in data dir
	struct dstr rel = {0};
	dstr_catf(&rel, "avatars/%s", avatar_name);
	char *data_path = obs_module_file(rel.array);
	dstr_free(&rel);
	if (data_path)
		BLOG(LOG_DEBUG, "resolve: found built-in avatar: %s", data_path);
	else
		BLOG(LOG_WARNING, "resolve: avatar not found: %s", avatar_name);
	return data_path;
}


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
	char *base_dir = resolve_avatar_dir(avatar_name, settings);
	if (!base_dir) return;

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
		struct dstr final_path = {0};
		dstr_copy(&final_path, base_dir);
		dstr_cat(&final_path, "/");
		dstr_cat(&final_path, file);
		set_str(key, os_file_exists(final_path.array) ? final_path.array : "");
		dstr_free(&final_path);
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

	struct dstr ini_path = {0};
	dstr_copy(&ini_path, base_dir);
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
	bfree(base_dir);
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
	obs_data_set_default_string(settings, "custom_avatars_path", "");
	obs_data_set_default_string(settings, "hint_custom_folder", obs_module_text("hint_custom_folder"));

	// Group hint text (always-visible info boxes in the properties panel)
	obs_data_set_default_string(settings, "hint_images", obs_module_text("hint_images"));
	obs_data_set_default_string(settings, "hint_audio",  obs_module_text("hint_audio"));
	obs_data_set_default_string(settings, "hint_blink",  obs_module_text("hint_blink"));
	obs_data_set_default_string(settings, "hint_action", obs_module_text("hint_action"));
	obs_data_set_default_string(settings, "hint_motion", obs_module_text("hint_motion"));

	// About info boxes — version built at runtime from CMake define
	struct dstr ver_str = {0};
	dstr_catf(&ver_str, "%s  v%s", obs_module_text("about_version"), FLOOD_TUBER_VERSION);
	obs_data_set_default_string(settings, "about_version", ver_str.array);
	dstr_free(&ver_str);
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

// Writes current settings to a settings.ini file at the given directory path
static void write_settings_ini(const char *dir_path, obs_data_t *settings)
{
	struct dstr ini_path = {0};
	dstr_copy(&ini_path, dir_path);
	dstr_cat(&ini_path, "/settings.ini");

	config_t *config = NULL;
	BLOG(LOG_DEBUG, "write_settings_ini: opening %s", ini_path.array);
	if (config_open(&config, ini_path.array, CONFIG_OPEN_ALWAYS) == CONFIG_SUCCESS) {
		const char *avatar_name = obs_data_get_string(settings, "avatar_list");
		if (avatar_name && *avatar_name)
			config_set_string(config, "General", "name", avatar_name);
		config_set_bool(config, "General", "mirror",
			obs_data_get_bool(settings, "mirror"));

		config_set_double(config, "Audio", "threshold",
			obs_data_get_double(settings, "threshold"));
		config_set_int(config, "Audio", "release_delay",
			(int)obs_data_get_int(settings, "release_delay"));

		config_set_int(config, "Blink", "duration",
			(int)obs_data_get_int(settings, "blink_duration"));
		config_set_int(config, "Blink", "interval_min",
			(int)obs_data_get_int(settings, "blink_interval_min"));
		config_set_int(config, "Blink", "interval_max",
			(int)obs_data_get_int(settings, "blink_interval_max"));

		config_set_int(config, "Action", "duration",
			(int)obs_data_get_int(settings, "action_duration"));
		config_set_int(config, "Action", "interval_min",
			(int)obs_data_get_int(settings, "action_interval_min"));
		config_set_int(config, "Action", "interval_max",
			(int)obs_data_get_int(settings, "action_interval_max"));

		config_set_string(config, "Motion", "type",
			obs_data_get_string(settings, "motion_type"));
		config_set_int(config, "Motion", "speed",
			(int)obs_data_get_int(settings, "motion_speed"));
		config_set_int(config, "Motion", "strength",
			(int)obs_data_get_int(settings, "motion_strength"));

		config_save(config);
		config_close(config);
		BLOG(LOG_INFO, "write_settings_ini: saved successfully to %s", ini_path.array);
	} else {
		BLOG(LOG_ERROR, "write_settings_ini: config_open FAILED for %s", ini_path.array);
	}
	dstr_free(&ini_path);
}

// Copies a file from src to dst. Returns true on success.
static bool copy_file_safe(const char *src, const char *dst)
{
	if (!src || !*src || !os_file_exists(src)) return false;

	FILE *in = fopen(src, "rb");
	if (!in) return false;

	FILE *out = fopen(dst, "wb");
	if (!out) { fclose(in); return false; }

	char buf[8192];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
		fwrite(buf, 1, n, out);

	fclose(in);
	fclose(out);
	return true;
}

// Saves current settings to the selected avatar's settings.ini
static bool save_settings_to_avatar(obs_properties_t *props, obs_property_t *p, void *data)
{
	(void)props; (void)p;
	struct flood_tuber_data *tuber = (struct flood_tuber_data *)data;
	if (!tuber || !tuber->source) return false;

	obs_data_t *settings = obs_source_get_settings(tuber->source);
	if (!settings) return false;

	const char *selected = obs_data_get_string(settings, "avatar_list");
	if (!selected || !*selected) {
		BLOG(LOG_WARNING, "save_settings: no avatar selected");
		obs_data_release(settings);
		return false;
	}

	// Resolve where the avatar currently lives
	char *source_dir = resolve_avatar_dir(selected, settings);
	if (!source_dir) {
		BLOG(LOG_ERROR, "save_settings: cannot resolve avatar dir for '%s'", selected);
		obs_data_release(settings);
		return false;
	}

	// Determine target dir: custom dir if set, otherwise save next to source
	const char *custom = get_custom_dir(settings);
	struct dstr target_dir = {0};

	if (custom) {
		// Save to custom dir
		dstr_copy(&target_dir, custom);
		dstr_cat(&target_dir, "/");
		dstr_cat(&target_dir, selected);
		os_mkdirs(target_dir.array);

		// If source is from data dir (built-in), copy images to custom dir
		if (strcmp(source_dir, target_dir.array) != 0) {
			BLOG(LOG_INFO, "save_settings: copying images from %s to %s", source_dir, target_dir.array);
			const char *files[] = {
				"idle.png", "blink.png", "action.png",
				"talk_a.png", "talk_b.png", "talk_c.png",
				"talk_a_blink.png", "talk_b_blink.png", "talk_c_blink.png",
				NULL
			};
			for (int i = 0; files[i]; i++) {
				struct dstr src = {0}, dst = {0};
				dstr_copy(&src, source_dir);
				dstr_cat(&src, "/");
				dstr_cat(&src, files[i]);
				dstr_copy(&dst, target_dir.array);
				dstr_cat(&dst, "/");
				dstr_cat(&dst, files[i]);
				copy_file_safe(src.array, dst.array);
				dstr_free(&src);
				dstr_free(&dst);
			}
		}
	} else {
		// No custom dir: write settings.ini next to source
		dstr_copy(&target_dir, source_dir);
	}

	write_settings_ini(target_dir.array, settings);
	BLOG(LOG_INFO, "save_settings: saved to %s", target_dir.array);

	dstr_free(&target_dir);
	bfree(source_dir);
	obs_data_release(settings);
	return true;
}

// Saves current config as a brand new avatar in the custom directory
static bool save_as_new_avatar(obs_properties_t *props, obs_property_t *p, void *data)
{
	(void)p;
	struct flood_tuber_data *tuber = (struct flood_tuber_data *)data;
	if (!tuber || !tuber->source) return false;

	obs_data_t *settings = obs_source_get_settings(tuber->source);
	if (!settings) return false;

	const char *raw_name = obs_data_get_string(settings, "new_avatar_name");
	if (!raw_name || !*raw_name) {
		BLOG(LOG_WARNING, "save_as_new: avatar name is empty");
		obs_data_release(settings);
		return false;
	}

	// Copy name string since we'll modify settings before using it
	char *new_name = bstrdup(raw_name);

	const char *custom = get_custom_dir(settings);
	if (!custom) {
		BLOG(LOG_WARNING, "save_as_new: no custom folder set");
		bfree(new_name);
		obs_data_release(settings);
		return false;
	}

	BLOG(LOG_INFO, "save_as_new: saving '%s' to '%s'", new_name, custom);

	struct dstr avatar_dir = {0};
	dstr_copy(&avatar_dir, custom);
	dstr_cat(&avatar_dir, "/");
	dstr_cat(&avatar_dir, new_name);
	os_mkdirs(avatar_dir.array);

	// Copy current images to the new avatar folder
	struct { const char *key; const char *filename; } img_map[] = {
		{"path_idle",         "idle.png"},
		{"path_blink",        "blink.png"},
		{"path_action",       "action.png"},
		{"path_talk_1",       "talk_a.png"},
		{"path_talk_2",       "talk_b.png"},
		{"path_talk_3",       "talk_c.png"},
		{"path_talk_1_blink", "talk_a_blink.png"},
		{"path_talk_2_blink", "talk_b_blink.png"},
		{"path_talk_3_blink", "talk_c_blink.png"},
		{NULL, NULL}
	};

	for (int i = 0; img_map[i].key; i++) {
		const char *src = obs_data_get_string(settings, img_map[i].key);
		if (!src || !*src) continue;

		struct dstr dst = {0};
		dstr_copy(&dst, avatar_dir.array);
		dstr_cat(&dst, "/");
		dstr_cat(&dst, img_map[i].filename);
		if (copy_file_safe(src, dst.array))
			BLOG(LOG_DEBUG, "save_as_new: copied %s", img_map[i].filename);
		else
			BLOG(LOG_WARNING, "save_as_new: failed to copy %s", img_map[i].filename);
		dstr_free(&dst);
	}

	// Save settings.ini, select the new avatar, and clear the name input
	write_settings_ini(avatar_dir.array, settings);

	// Add the new avatar to the dropdown list immediately
	obs_property_t *list = obs_properties_get(props, "avatar_list");
	if (list)
		obs_property_list_add_string(list, new_name, new_name);

	obs_data_set_string(settings, "avatar_list", new_name);
	obs_data_set_string(settings, "new_avatar_name", "");
	apply_avatar_to_settings(settings, new_name, false);
	obs_source_update(tuber->source, settings);

	dstr_free(&avatar_dir);
	bfree(new_name);
	obs_data_release(settings);

	BLOG(LOG_INFO, "save_as_new: complete");
	return true;
}

// Opens the custom avatars folder in Windows Explorer
static bool open_library_folder(obs_properties_t *props, obs_property_t *p, void *data)
{
	(void)props; (void)p;
	struct flood_tuber_data *tuber = (struct flood_tuber_data *)data;
	if (!tuber || !tuber->source) return false;

	obs_data_t *settings = obs_source_get_settings(tuber->source);
	if (!settings) return false;

	const char *custom = get_custom_dir(settings);
	if (custom) {
		BLOG(LOG_INFO, "open_folder: opening %s", custom);
#ifdef _WIN32
		ShellExecuteA(NULL, "open", custom, NULL, NULL, SW_SHOWDEFAULT);
#endif
	} else {
		// No custom folder set — open built-in avatars dir
		char *data_dir = obs_module_file("avatars");
		if (data_dir) {
			BLOG(LOG_INFO, "open_folder: no custom dir, opening built-in: %s", data_dir);
#ifdef _WIN32
			ShellExecuteA(NULL, "open", data_dir, NULL, NULL, SW_SHOWDEFAULT);
#endif
			bfree(data_dir);
		}
	}
	obs_data_release(settings);
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

// Opens the Flood Tuber website
static bool open_website_page(obs_properties_t *props, obs_property_t *p, void *data)
{
	(void)props; (void)p; (void)data;
#ifdef _WIN32
	ShellExecuteA(NULL, "open", "https://floodtechlab.com/floodtuber/",
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


// Helper: adds custom dir avatar entries to a list property, avoiding duplicates
static void populate_custom_avatars(obs_property_t *list, const char *custom_path)
{
	if (!custom_path || !*custom_path || !os_file_exists(custom_path)) return;
	os_dir_t *cdir = os_opendir(custom_path);
	if (!cdir) return;
	struct os_dirent *ent;
	while ((ent = os_readdir(cdir)) != NULL) {
		if (ent->directory && ent->d_name[0] != '.') {
			size_t count = obs_property_list_item_count(list);
			bool found = false;
			for (size_t i = 0; i < count; i++) {
				const char *v = obs_property_list_item_string(list, i);
				if (v && strcmp(v, ent->d_name) == 0) { found = true; break; }
			}
			if (!found)
				obs_property_list_add_string(list, ent->d_name, ent->d_name);
		}
	}
	os_closedir(cdir);
}

// Called when the custom avatars folder path changes — rebuilds the dropdown
static bool on_custom_path_changed(obs_properties_t *props, obs_property_t *p,
                                   obs_data_t *settings)
{
	(void)p;
	obs_property_t *list = obs_properties_get(props, "avatar_list");
	if (!list) return false;

	// Clear the entire list and re-add built-in avatars
	obs_property_list_clear(list);

	char *data_dir = obs_module_file("avatars");
	if (data_dir) {
		os_dir_t *dir = os_opendir(data_dir);
		if (dir) {
			struct os_dirent *ent;
			while ((ent = os_readdir(dir)) != NULL)
				if (ent->directory && ent->d_name[0] != '.')
					obs_property_list_add_string(list, ent->d_name, ent->d_name);
			os_closedir(dir);
		}
		bfree(data_dir);
	}

	// Add custom avatars from the new path
	const char *custom = obs_data_get_string(settings, "custom_avatars_path");
	populate_custom_avatars(list, custom);

	return true;
}

// Clears the custom folder path
static bool clear_custom_folder(obs_properties_t *props, obs_property_t *p, void *data)
{
	(void)p;
	struct flood_tuber_data *tuber = (struct flood_tuber_data *)data;
	if (!tuber || !tuber->source) return false;

	obs_data_t *settings = obs_source_get_settings(tuber->source);
	if (!settings) return false;

	obs_data_set_string(settings, "custom_avatars_path", "");
	obs_source_update(tuber->source, settings);

	// Rebuild the dropdown without custom entries
	on_custom_path_changed(props, NULL, settings);

	obs_data_release(settings);
	return true;
}

// Build the OBS properties panel
obs_properties_t *flood_tuber_properties(void *data)
{
	struct flood_tuber_data *tuber = (struct flood_tuber_data *)data;
	obs_properties_t *props = obs_properties_create();

	// ── 1. Avatar Library ───────────────────
	obs_properties_t *lib = obs_properties_create();
	obs_properties_add_group(props, "library_group",
		obs_module_text("library_group"), OBS_GROUP_NORMAL, lib);

	obs_property_t *list = obs_properties_add_list(lib, "avatar_list",
		obs_module_text("avatar_list"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	// Enumerate built-in avatars from data dir
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

	// Enumerate custom avatars from user-specified folder
	if (tuber && tuber->source) {
		obs_data_t *cur_settings = obs_source_get_settings(tuber->source);
		if (cur_settings) {
			const char *custom = get_custom_dir(cur_settings);
			populate_custom_avatars(list, custom);
			obs_data_release(cur_settings);
		}
	}

	obs_properties_add_button(lib, "load_avatar_btn",
		obs_module_text("load_avatar_btn"), load_avatar);
	obs_properties_add_button(lib, "save_settings_btn",
		obs_module_text("save_settings_btn"), save_settings_to_avatar);

	// Custom avatars folder picker with live-refresh callback
	obs_properties_add_text(lib, "hint_custom_folder", NULL, OBS_TEXT_INFO);
	obs_property_t *folder_prop = obs_properties_add_path(lib, "custom_avatars_path",
		obs_module_text("custom_avatars_path"), OBS_PATH_DIRECTORY, NULL, NULL);
	obs_property_set_modified_callback(folder_prop, on_custom_path_changed);
	obs_properties_add_button(lib, "clear_custom_folder_btn",
		obs_module_text("clear_custom_folder"), clear_custom_folder);

	// Save As New Avatar
	obs_properties_add_text(lib, "new_avatar_name",
		obs_module_text("new_avatar_name"), OBS_TEXT_DEFAULT);
	obs_properties_add_button(lib, "save_as_new_btn",
		obs_module_text("save_as_new_btn"), save_as_new_avatar);
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

	// Talking frames
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
	obs_properties_add_button(about, "about_website_btn",
		obs_module_text("about_website_btn"), open_website_page);
	obs_properties_add_button(about, "about_github_btn",
		obs_module_text("about_github_btn"), open_github_page);

	return props;
}
