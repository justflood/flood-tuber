#include "flood-tuber-props.h"
#include "flood-tuber.h"
#include <util/dstr.h>
#include <util/config-file.h>
#include <util/platform.h>
#include <string.h>
#include <cstdio>
#include <vector>
#include <string>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include <spawn.h>
#include <unistd.h>
#include <sys/wait.h>
extern char **environ;
#endif

// Portable helper to open folders or URLs in the OS default handler
static void os_open_url_or_path(const char *target)
{
	if (!target || !*target) return;
#if defined(_WIN32)
	// Wide-character call: the ANSI one mangles UTF-8 paths (accents, non-Latin names)
	wchar_t *wide = NULL;
	os_utf8_to_wcs_ptr(target, 0, &wide);
	if (wide) {
		ShellExecuteW(NULL, L"open", wide, NULL, NULL, SW_SHOWNORMAL);
		bfree(wide);
	}
#elif defined(__APPLE__)
	pid_t pid;
	char *argv[] = {(char *)"open", (char *)target, NULL};
	if (posix_spawnp(&pid, "open", NULL, NULL, argv, environ) == 0) {
		int status;
		waitpid(pid, &status, WNOHANG);
	}
#else
	pid_t pid;
	char *argv[] = {(char *)"xdg-open", (char *)target, NULL};
	if (posix_spawnp(&pid, "xdg-open", NULL, NULL, argv, environ) == 0) {
		int status;
		waitpid(pid, &status, WNOHANG);
	}
#endif
}


// Image formats an avatar folder may use, in lookup order.
// ".png" comes first, so avatars made before other formats were accepted load exactly as before.
static const char *const AVATAR_IMAGE_EXTS[] = {".png", ".apng", ".gif", ".webp"};
static const size_t AVATAR_IMAGE_EXT_COUNT = sizeof(AVATAR_IMAGE_EXTS) / sizeof(AVATAR_IMAGE_EXTS[0]);

// The image slots of an avatar: settings key and file name without extension
static const struct {
	const char *key;
	const char *stem;
} AVATAR_IMAGES[] = {
	{"path_idle",         "idle"},
	{"path_blink",        "blink"},
	{"path_action",       "action"},
	{"path_talk_1",       "talk_a"},
	{"path_talk_2",       "talk_b"},
	{"path_talk_3",       "talk_c"},
	{"path_talk_1_blink", "talk_a_blink"},
	{"path_talk_2_blink", "talk_b_blink"},
	{"path_talk_3_blink", "talk_c_blink"},
	{"path_talk_4",       "talk_d"},
	{"path_talk_5",       "talk_e"},
	{"path_talk_6",       "talk_f"},
	{"path_talk_4_blink", "talk_d_blink"},
	{"path_talk_5_blink", "talk_e_blink"},
	{"path_talk_6_blink", "talk_f_blink"},
};
static const size_t AVATAR_IMAGE_COUNT = sizeof(AVATAR_IMAGES) / sizeof(AVATAR_IMAGES[0]);

// Returns "<dir>/<stem><ext>" for the first supported extension that exists, or "" if there is none
static std::string find_avatar_image(const char *dir, const char *stem)
{
	for (size_t i = 0; i < AVATAR_IMAGE_EXT_COUNT; i++) {
		std::string path = std::string(dir) + "/" + stem + AVATAR_IMAGE_EXTS[i];
		if (os_file_exists(path.c_str()))
			return path;
	}
	return std::string();
}

// Extension an image keeps when it is copied into an avatar folder: its own when the format
// is one of AVATAR_IMAGE_EXTS (so a GIF stays a GIF and keeps its animation), ".png" otherwise.
static const char *avatar_image_ext(const char *path)
{
	const char *dot = strrchr(path, '.');
	if (dot) {
		for (size_t i = 0; i < AVATAR_IMAGE_EXT_COUNT; i++)
			if (astrcmpi(dot, AVATAR_IMAGE_EXTS[i]) == 0)
				return AVATAR_IMAGE_EXTS[i];
	}
	return AVATAR_IMAGE_EXTS[0];
}

// True when both paths name the same file (slash direction ignored; case ignored on Windows)
static bool same_file_path(const std::string &a, const std::string &b)
{
	std::string x = a, y = b;
	for (char &c : x) if (c == '\\') c = '/';
	for (char &c : y) if (c == '\\') c = '/';
#ifdef _WIN32
	return astrcmpi(x.c_str(), y.c_str()) == 0;
#else
	return x == y;
#endif
}

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

		// Only use custom dir if it has an idle image, in any supported format
		bool has_images = !find_avatar_image(user_path.array, "idle").empty();

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

// Lists the avatars: built-in folder followed by custom folder, without duplicates.
void flood_tuber_list_avatars(obs_data_t *settings, std::vector<std::string> &out)
{
	auto add_unique = [&](const char *name) {
		for (const auto &n : out)
			if (n == name) return;
		out.emplace_back(name);
	};

	auto scan = [&](const char *dir) {
		if (!dir || !*dir) return;
		os_dir_t *d = os_opendir(dir);
		if (!d) return;
		struct os_dirent *ent;
		while ((ent = os_readdir(d)) != NULL)
			if (ent->directory && ent->d_name[0] != '.')
				add_unique(ent->d_name);
		os_closedir(d);
	};

	char *builtin = obs_module_file("avatars");
	scan(builtin);
	if (builtin) bfree(builtin);

	const char *custom = settings ? get_custom_dir(settings) : NULL;
	if (custom && os_file_exists(custom))
		scan(custom);
}

static void fill_avatar_list(obs_property_t *list, const std::vector<std::string> &names, bool with_none)
{
	if (!list) return;
	obs_property_list_clear(list);
	if (with_none)
		obs_property_list_add_string(list, obs_module_text("slot_none"), "");
	for (const auto &n : names)
		obs_property_list_add_string(list, n.c_str(), n.c_str());
}

// Fills avatar_list and the slot lists. props = the top-level properties:
// obs_properties_get() also searches inside the groups.
static void fill_all_lists(obs_properties_t *props, obs_data_t *settings)
{
	std::vector<std::string> names;
	flood_tuber_list_avatars(settings, names);

	fill_avatar_list(obs_properties_get(props, "avatar_list"), names, false);
	for (int i = 1; i <= FLOOD_SLOT_COUNT; i++) {
		char key[16];
		snprintf(key, sizeof(key), "slot_%d", i);
		fill_avatar_list(obs_properties_get(props, key), names, true);
	}
}

// “Refresh” button: reads the current source settings
static bool refresh_avatar_lists(obs_properties_t *props, obs_property_t *p, void *data)
{
	(void)p;
	struct flood_tuber_data *tuber = (struct flood_tuber_data *)data;
	if (!tuber || !tuber->source) return false;

	obs_data_t *settings = obs_source_get_settings(tuber->source);
	fill_all_lists(props, settings);
	obs_data_release(settings);
	return true; // redessine les propriétés
}

// Load avatar images and settings.ini into an obs_data_t object.
// as_default=true  → sets *default* values (plugin init)
// as_default=false → sets *current* values (load via UI)
bool apply_avatar_to_settings(obs_data_t *settings, const char *avatar_name, bool as_default)
{
	char *base_dir = resolve_avatar_dir(avatar_name, settings);
	if (!base_dir) return false;

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
	auto set_bool = [&](const char *key, bool val) {
		if (as_default) obs_data_set_default_bool(settings, key, val);
		else obs_data_set_bool(settings, key, val);
	};
	// Each image is looked up under every supported extension; a missing image gives an empty path
	for (size_t i = 0; i < AVATAR_IMAGE_COUNT; i++)
		set_str(AVATAR_IMAGES[i].key, find_avatar_image(base_dir, AVATAR_IMAGES[i].stem).c_str());

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
		load_dbl("Audio",  "talking_speed","talking_speed");
		load_str("Audio",  "mouth_mode",   "mouth_mode");
		load_int("Audio",  "mouth_range",  "mouth_range");
		load_int("Blink",  "duration",     "blink_duration");
		load_int("Blink",  "interval_min", "blink_interval_min");
		load_int("Blink",  "interval_max", "blink_interval_max");
		load_int("Action", "duration",     "action_duration");
		load_int("Action", "interval_min", "action_interval_min");
		load_int("Action", "interval_max", "action_interval_max");
		load_str("Motion", "type",         "motion_type");
		load_int("Motion", "speed",        "motion_speed");
		load_int("Motion", "strength",     "motion_strength");
		load_int("Motion", "reference_height", "reference_height");

		if (config_has_user_value(config, "General", "mirror"))
			set_bool("mirror", config_get_bool(config, "General", "mirror"));

		config_close(config);
	}
	dstr_free(&ini_path);
	bfree(base_dir);
	return true;
}

// Set plugin defaults on first load
void flood_tuber_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "audio_source", "");
	obs_data_set_default_double(settings, "threshold",          -30.0);
	obs_data_set_default_int(settings,    "release_delay",       200);
	obs_data_set_default_double(settings, "talking_speed",        0.10);
	obs_data_set_default_string(settings, "mouth_mode",         "Cycle");
	obs_data_set_default_int(settings,    "mouth_range",          20);

	obs_data_set_default_int(settings, "action_duration",        4000);
	obs_data_set_default_int(settings, "action_interval_min",   60000);
	obs_data_set_default_int(settings, "action_interval_max",  120000);
	obs_data_set_default_int(settings, "blink_duration",          150);
	obs_data_set_default_int(settings, "blink_interval_min",     2000);
	obs_data_set_default_int(settings, "blink_interval_max",     8000);

	obs_data_set_default_string(settings, "motion_type",    "Shake");
	obs_data_set_default_int(settings,    "motion_speed",     100);
	obs_data_set_default_int(settings,    "motion_strength",    1);
	obs_data_set_default_int(settings, "reference_height", 512);
	obs_data_set_default_bool(settings, "keep_audio_on_switch", false);
	obs_data_set_default_bool(settings, "show_extra_frames", false);
	obs_data_set_default_bool(settings, "show_slots", false);
	// Avatar transition (crossfade between avatars)
	obs_data_set_default_bool(settings, "avatar_fade_enabled", true);
	obs_data_set_default_int(settings, "avatar_fade_duration", 300);
	obs_data_set_default_string(settings, "avatar_fade_easing", "Smooth");
	obs_data_set_default_string(settings, "hint_fade", obs_module_text("hint_fade"));

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
IMPLEMENT_CLEAR_CALLBACK(clear_talk_4,        "path_talk_4")
IMPLEMENT_CLEAR_CALLBACK(clear_talk_4_blink,  "path_talk_4_blink")
IMPLEMENT_CLEAR_CALLBACK(clear_talk_5,        "path_talk_5")
IMPLEMENT_CLEAR_CALLBACK(clear_talk_5_blink,  "path_talk_5_blink")
IMPLEMENT_CLEAR_CALLBACK(clear_talk_6,        "path_talk_6")
IMPLEMENT_CLEAR_CALLBACK(clear_talk_6_blink,  "path_talk_6_blink")

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
		config_set_double(config, "Audio", "talking_speed",
			obs_data_get_double(settings, "talking_speed"));
		config_set_string(config, "Audio", "mouth_mode",
			obs_data_get_string(settings, "mouth_mode"));
		config_set_int(config, "Audio", "mouth_range",
			(int)obs_data_get_int(settings, "mouth_range"));

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
		config_set_int(config, "Motion", "reference_height",
			(int)obs_data_get_int(settings, "reference_height"));

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

	// os_fopen() takes UTF-8 paths on every platform; plain fopen() fails on Windows
	// as soon as a folder name has an accent
	FILE *in = os_fopen(src, "rb");
	if (!in) return false;

	FILE *out = os_fopen(dst, "wb");
	if (!out) { fclose(in); return false; }

	bool ok = true;
	char buf[8192];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
		if (fwrite(buf, 1, n, out) != n) { ok = false; break; } // disk full, for instance
	}

	fclose(in);
	if (fclose(out) != 0) ok = false;
	if (!ok) os_unlink(dst); // do not leave a truncated image behind
	return ok;
}

// Copies one image into an avatar folder as "<stem><ext>", keeping the format of the source.
// Returns true when the image is in place.
static bool copy_avatar_image(const char *src, const char *dst_dir, const char *stem)
{
	if (!src || !*src || !os_file_exists(src)) return false;

	const char *ext = avatar_image_ext(src);
	std::string dst = std::string(dst_dir) + "/" + stem + ext;

	// Already there: copying a file onto itself would empty it
	if (same_file_path(src, dst)) return true;

	if (!copy_file_safe(src, dst.c_str())) return false;

	// Remove a copy of this image left in another format: the lookup would find it first
	for (size_t i = 0; i < AVATAR_IMAGE_EXT_COUNT; i++) {
		if (strcmp(AVATAR_IMAGE_EXTS[i], ext) == 0) continue;
		std::string old = std::string(dst_dir) + "/" + stem + AVATAR_IMAGE_EXTS[i];
		if (!same_file_path(src, old) && os_file_exists(old.c_str()))
			os_unlink(old.c_str());
	}
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
			for (size_t i = 0; i < AVATAR_IMAGE_COUNT; i++) {
				std::string src = find_avatar_image(source_dir, AVATAR_IMAGES[i].stem);
				if (!src.empty())
					copy_avatar_image(src.c_str(), target_dir.array, AVATAR_IMAGES[i].stem);
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

// Turns what the user typed into a safe folder name: no path separators, no characters Windows
// refuses, no leading or trailing spaces or dots (a leading dot would also hide the avatar from
// the list). Returns "" when nothing usable is left.
static std::string sanitize_avatar_name(const char *raw)
{
	std::string name;
	for (const char *c = raw; c && *c; c++) {
		unsigned char ch = (unsigned char)*c;
		if (ch < 0x20 || ch == 0x7F) continue;
		if (ch < 0x80 && strchr("\\/:*?\"<>|", *c)) continue;
		name += *c;
	}

	size_t first = name.find_first_not_of(" .");
	if (first == std::string::npos) return std::string();
	size_t last = name.find_last_not_of(" .");
	return name.substr(first, last - first + 1);
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

	// Folder-safe copy of the name (the settings are modified before it is used)
	std::string clean_name = sanitize_avatar_name(raw_name);
	if (clean_name.empty()) {
		BLOG(LOG_WARNING, "save_as_new: '%s' is not a usable avatar name", raw_name);
		obs_data_release(settings);
		return false;
	}
	char *new_name = bstrdup(clean_name.c_str());

	const char *custom = get_custom_dir(settings);
	if (!custom) {
		BLOG(LOG_WARNING, "save_as_new: no custom folder set");
		bfree(new_name);
		obs_data_release(settings);
		return false;
	}

	// Never write over another avatar. If the folder already exists and is not the avatar being
	// edited, the new one gets a numbered name instead: "Name (2)", "Name (3)"...
	const char *selected = obs_data_get_string(settings, "avatar_list");
	if (!(selected && same_file_path(selected, new_name))) {
		std::string base = new_name;
		std::string candidate = base;
		int number = 1;
		while (os_file_exists((std::string(custom) + "/" + candidate).c_str())) {
			if (++number > 99) {
				BLOG(LOG_WARNING, "save_as_new: too many avatars named '%s'", base.c_str());
				bfree(new_name);
				obs_data_release(settings);
				return false;
			}
			candidate = base + " (" + std::to_string(number) + ")";
		}
		if (candidate != base) {
			BLOG(LOG_INFO, "save_as_new: '%s' already exists, saving as '%s'", base.c_str(), candidate.c_str());
			bfree(new_name);
			new_name = bstrdup(candidate.c_str());
		}
	}

	BLOG(LOG_INFO, "save_as_new: saving '%s' to '%s'", new_name, custom);

	struct dstr avatar_dir = {0};
	dstr_copy(&avatar_dir, custom);
	dstr_cat(&avatar_dir, "/");
	dstr_cat(&avatar_dir, new_name);
	os_mkdirs(avatar_dir.array);

	// Copy current images to the new avatar folder, each in its own format (GIF, WebP, APNG or PNG)
	for (size_t i = 0; i < AVATAR_IMAGE_COUNT; i++) {
		const char *src = obs_data_get_string(settings, AVATAR_IMAGES[i].key);
		if (!src || !*src) continue;

		if (copy_avatar_image(src, avatar_dir.array, AVATAR_IMAGES[i].stem))
			BLOG(LOG_DEBUG, "save_as_new: copied %s%s", AVATAR_IMAGES[i].stem, avatar_image_ext(src));
		else
			BLOG(LOG_WARNING, "save_as_new: failed to copy %s", AVATAR_IMAGES[i].stem);
	}

	// Save settings.ini, select the new avatar, and clear the name input
	write_settings_ini(avatar_dir.array, settings);

	// Recharge avatar_list et les emplacements (le dossier existe déjà)
	fill_all_lists(props, settings);

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

// Opens the custom avatars folder in OS default file manager
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
		os_open_url_or_path(custom);
	} else {
		// No custom folder set — open built-in avatars dir
		char *data_dir = obs_module_file("avatars");
		if (data_dir) {
			BLOG(LOG_INFO, "open_folder: no custom dir, opening built-in: %s", data_dir);
			os_open_url_or_path(data_dir);
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
	const char *selected = obs_data_get_string(settings, "avatar_list");
	bool ok = selected && *selected && switch_to_avatar(tuber, selected);
	obs_data_release(settings);
	return ok;
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
	os_open_url_or_path("https://github.com/justflood/flood-tuber");
	return false;
}

// Opens the Flood Tuber website
static bool open_website_page(obs_properties_t *props, obs_property_t *p, void *data)
{
	(void)props; (void)p; (void)data;
	os_open_url_or_path("https://floodtechlab.com/floodtuber/");
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

// Shows or hides the "Avatar slots" group, following the "show_slots" box.
// The 20 lists stay out of the way for users who only switch avatars from the library.
static bool on_show_slots_changed(obs_properties_t *props, obs_property_t *p, obs_data_t *settings)
{
	(void)p;
	bool show = obs_data_get_bool(settings, "show_slots");
	if (!obs_data_has_user_value(settings, "show_slots")) {
		// Box never touched: show the group if a slot is already assigned
		for (int i = 1; i <= FLOOD_SLOT_COUNT && !show; i++) {
			char key[16];
			snprintf(key, sizeof(key), "slot_%d", i);
			const char *name = obs_data_get_string(settings, key);
			if (name && *name) show = true;
		}
	}
	obs_property_set_visible(obs_properties_get(props, "slots_group"), show);
	return true;
}

// Shows or hides the extra talking frames (D, E, F), following the "show_extra_frames" box.
// Most avatars only have A, B, C and the image list is long enough without them.
static bool on_show_extra_frames_changed(obs_properties_t *props, obs_property_t *p,
                                         obs_data_t *settings)
{
	(void)p;
	static const char *const paths[] = {
		"path_talk_4", "path_talk_4_blink", "path_talk_5",
		"path_talk_5_blink", "path_talk_6", "path_talk_6_blink",
	};
	static const char *const buttons[] = {
		"clear_talk_4", "clear_talk_4_blink", "clear_talk_5",
		"clear_talk_5_blink", "clear_talk_6", "clear_talk_6_blink",
	};

	bool show = obs_data_get_bool(settings, "show_extra_frames");
	if (!obs_data_has_user_value(settings, "show_extra_frames")) {
		// Box never touched: show the frames if the avatar already uses one
		for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
			const char *path = obs_data_get_string(settings, paths[i]);
			if (path && *path) show = true;
		}
	}
	for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
		obs_property_set_visible(obs_properties_get(props, paths[i]), show);
		obs_property_set_visible(obs_properties_get(props, buttons[i]), show);
	}
	return true;
}

// Called when the custom avatars folder path changes — rebuilds the dropdown
static bool on_custom_path_changed(obs_properties_t *props, obs_property_t *p, obs_data_t *settings)
{
	(void)p;
	fill_all_lists(props, settings);
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

	obs_properties_add_list(lib, "avatar_list",
		obs_module_text("avatar_list"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_properties_add_button2(lib, "refresh_avatars_btn",
		obs_module_text("refresh_avatars_btn"), refresh_avatar_lists, data);
	obs_properties_add_button2(lib, "load_avatar_btn",
		obs_module_text("load_avatar_btn"), load_avatar, data);
	obs_properties_add_button2(lib, "save_settings_btn",
		obs_module_text("save_settings_btn"), save_settings_to_avatar, data);

	// Shows or hides the "Avatar slots" group below
	obs_property_t *p_show_slots = obs_properties_add_bool(lib, "show_slots",
		obs_module_text("show_slots"));
	obs_property_set_modified_callback(p_show_slots, on_show_slots_changed);

	// Dossier personnalisé
	obs_properties_add_text(lib, "hint_custom_folder", NULL, OBS_TEXT_INFO);
	obs_property_t *folder_prop = obs_properties_add_path(lib, "custom_avatars_path",
		obs_module_text("custom_avatars_path"), OBS_PATH_DIRECTORY, NULL, NULL);
	obs_property_set_modified_callback(folder_prop, on_custom_path_changed);
	obs_properties_add_button2(lib, "clear_custom_folder_btn",
		obs_module_text("clear_custom_folder"), clear_custom_folder, data);

	// Nouvel avatar
	obs_properties_add_text(lib, "new_avatar_name",
		obs_module_text("new_avatar_name"), OBS_TEXT_DEFAULT);
	obs_properties_add_button2(lib, "save_as_new_btn",
		obs_module_text("save_as_new_btn"), save_as_new_avatar, data);
	obs_properties_add_button2(lib, "open_folder_btn",
		obs_module_text("open_folder_btn"), open_library_folder, data);

	// ── 1a. Avatar Slots ───────────────────────────────────────────────────
	// One list per slot hotkey (1..20): each one names the avatar that hotkey switches to
	obs_properties_t *slots = obs_properties_create();
	obs_properties_add_group(props, "slots_group",
		obs_module_text("slots_group"), OBS_GROUP_NORMAL, slots);

	for (int i = 1; i <= FLOOD_SLOT_COUNT; i++) {
		char key[16], label[64];
		snprintf(key, sizeof(key), "slot_%d", i);
		snprintf(label, sizeof(label), "%s %d", obs_module_text("hotkey_slot"), i);
		obs_properties_add_list(slots, key, label,
			OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	}

	// Remplissage initial de toutes les listes
	obs_data_t *cur = (tuber && tuber->source) ? obs_source_get_settings(tuber->source) : NULL;
	fill_all_lists(props, cur);
	if (cur) obs_data_release(cur);

	// ── 1b. Avatar Transition ──────────────────────────────────────────────
	obs_properties_t *fade = obs_properties_create();
	obs_properties_add_group(props, "fade_settings",
		obs_module_text("fade_settings"), OBS_GROUP_NORMAL, fade);

	obs_properties_add_text(fade, "hint_fade", NULL, OBS_TEXT_INFO);

	obs_properties_add_bool(fade, "avatar_fade_enabled",
		obs_module_text("avatar_fade_enabled"));

	obs_property_t *p_fade_ms = obs_properties_add_int_slider(fade,
		"avatar_fade_duration", obs_module_text("avatar_fade_duration"), 100, 1500, 50);
	obs_property_int_set_suffix(p_fade_ms, " ms");
	obs_property_set_long_description(p_fade_ms, obs_module_text("avatar_fade_duration_tooltip"));

	obs_property_t *p_fade_easing = obs_properties_add_list(fade, "avatar_fade_easing",
		obs_module_text("avatar_fade_easing"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p_fade_easing, obs_module_text("avatar_fade_easing_linear"), "Linear");
	obs_property_list_add_string(p_fade_easing, obs_module_text("avatar_fade_easing_smooth"), "Smooth");
	obs_property_set_long_description(p_fade_easing, obs_module_text("avatar_fade_easing_tooltip"));

	// ── 2. Avatar Images ───────────────────────────────────────────────────
	obs_properties_t *img = obs_properties_create();
	obs_properties_add_group(props, "images_group",
		obs_module_text("images_group"), OBS_GROUP_NORMAL, img);

	const char *clear_txt = obs_module_text("clear_image");

	obs_properties_add_text(img, "hint_images", NULL, OBS_TEXT_INFO);

	// Base states
	add_file_prop(img, "path_idle",   obs_module_text("path_idle"),   obs_module_text("path_idle_tooltip"));
	obs_properties_add_button2(img, "clear_idle",   clear_txt, clear_idle, data);
	add_file_prop(img, "path_blink",  obs_module_text("path_blink"),  obs_module_text("path_blink_tooltip"));
	obs_properties_add_button2(img, "clear_blink",  clear_txt, clear_blink, data);
	add_file_prop(img, "path_action", obs_module_text("path_action"), obs_module_text("path_action_tooltip"));
	obs_properties_add_button2(img, "clear_action", clear_txt, clear_action, data);

	// Talking frames
	add_file_prop(img, "path_talk_1",       obs_module_text("path_talk_1"),       obs_module_text("path_talk_1_tooltip"));
	obs_properties_add_button2(img, "clear_talk_1",       clear_txt, clear_talk_1, data);
	add_file_prop(img, "path_talk_1_blink", obs_module_text("path_talk_1_blink"), obs_module_text("path_talk_1_blink_tooltip"));
	obs_properties_add_button2(img, "clear_talk_1_blink", clear_txt, clear_talk_1_blink, data);

	add_file_prop(img, "path_talk_2",       obs_module_text("path_talk_2"),       obs_module_text("path_talk_2_tooltip"));
	obs_properties_add_button2(img, "clear_talk_2",       clear_txt, clear_talk_2, data);
	add_file_prop(img, "path_talk_2_blink", obs_module_text("path_talk_2_blink"), obs_module_text("path_talk_2_blink_tooltip"));
	obs_properties_add_button2(img, "clear_talk_2_blink", clear_txt, clear_talk_2_blink, data);

	add_file_prop(img, "path_talk_3",       obs_module_text("path_talk_3"),       obs_module_text("path_talk_3_tooltip"));
	obs_properties_add_button2(img, "clear_talk_3",       clear_txt, clear_talk_3, data);
	add_file_prop(img, "path_talk_3_blink", obs_module_text("path_talk_3_blink"), obs_module_text("path_talk_3_blink_tooltip"));
	obs_properties_add_button2(img, "clear_talk_3_blink", clear_txt, clear_talk_3_blink, data);

	// Extra talking frames D, E, F (hidden until asked for, see on_show_extra_frames_changed)
	obs_property_t *p_extra = obs_properties_add_bool(img, "show_extra_frames",
		obs_module_text("show_extra_frames"));
	obs_property_set_modified_callback(p_extra, on_show_extra_frames_changed);

	add_file_prop(img, "path_talk_4",       obs_module_text("path_talk_4"),       obs_module_text("path_talk_4_tooltip"));
	obs_properties_add_button2(img, "clear_talk_4",       clear_txt, clear_talk_4, data);
	add_file_prop(img, "path_talk_4_blink", obs_module_text("path_talk_4_blink"), obs_module_text("path_talk_4_blink_tooltip"));
	obs_properties_add_button2(img, "clear_talk_4_blink", clear_txt, clear_talk_4_blink, data);

	add_file_prop(img, "path_talk_5",       obs_module_text("path_talk_5"),       obs_module_text("path_talk_5_tooltip"));
	obs_properties_add_button2(img, "clear_talk_5",       clear_txt, clear_talk_5, data);
	add_file_prop(img, "path_talk_5_blink", obs_module_text("path_talk_5_blink"), obs_module_text("path_talk_5_blink_tooltip"));
	obs_properties_add_button2(img, "clear_talk_5_blink", clear_txt, clear_talk_5_blink, data);

	add_file_prop(img, "path_talk_6",       obs_module_text("path_talk_6"),       obs_module_text("path_talk_6_tooltip"));
	obs_properties_add_button2(img, "clear_talk_6",       clear_txt, clear_talk_6, data);
	add_file_prop(img, "path_talk_6_blink", obs_module_text("path_talk_6_blink"), obs_module_text("path_talk_6_blink_tooltip"));
	obs_properties_add_button2(img, "clear_talk_6_blink", clear_txt, clear_talk_6_blink, data);

	// ── 3. Audio & Trigger ─────────────────────────────────────────────────
	obs_properties_t *audio = obs_properties_create();
	obs_properties_add_group(props, "audio_settings",
		obs_module_text("audio_settings"), OBS_GROUP_NORMAL, audio);

	obs_properties_add_text(audio, "hint_audio", NULL, OBS_TEXT_INFO);

	obs_property_t *p_src = obs_properties_add_list(audio, "audio_source",
		obs_module_text("audio_source"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p_src, "", "");
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

	obs_property_t *p_mouth = obs_properties_add_list(audio, "mouth_mode",
		obs_module_text("mouth_mode"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p_mouth, obs_module_text("mouth_mode_cycle"),  "Cycle");
	obs_property_list_add_string(p_mouth, obs_module_text("mouth_mode_volume"), "Volume");
	obs_property_set_long_description(p_mouth, obs_module_text("mouth_mode_tooltip"));

	obs_property_t *p_range = obs_properties_add_int_slider(audio,
		"mouth_range", obs_module_text("mouth_range"), 1, 60, 1);
	obs_property_set_long_description(p_range, obs_module_text("mouth_range_tooltip"));

	obs_properties_add_bool(audio, "keep_audio_on_switch",
		obs_module_text("keep_audio_on_switch"));

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

	obs_property_t *p_ref = obs_properties_add_int(motion, "reference_height",
		obs_module_text("reference_height"), 64, 4096, 1);
	obs_property_set_long_description(p_ref, obs_module_text("reference_height_tooltip"));

	obs_property_t *p_mtype = obs_properties_add_list(motion, "motion_type",
		obs_module_text("motion_type"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	// Translated labels; the stored values stay in English (settings.ini, saved scenes)
	obs_property_list_add_string(p_mtype, obs_module_text("motion_none"),        "None");
	obs_property_list_add_string(p_mtype, obs_module_text("motion_bounce"),      "Bounce");
	obs_property_list_add_string(p_mtype, obs_module_text("motion_shake"),       "Shake");
	obs_property_list_add_string(p_mtype, obs_module_text("motion_oscillating"), "Oscillating");
	obs_property_set_long_description(p_mtype, obs_module_text("motion_type_tooltip"));
	obs_property_set_modified_callback(p_mtype, on_motion_type_changed);

	obs_property_t *p_mspeed = obs_properties_add_int_slider(motion,
		"motion_speed", obs_module_text("motion_speed"), 0, 500, 10);
	obs_property_set_long_description(p_mspeed, obs_module_text("motion_speed_tooltip"));

	obs_property_t *p_mstrength = obs_properties_add_int_slider(motion,
		"motion_strength", obs_module_text("motion_strength"), 0, 50, 1);
	obs_property_set_long_description(p_mstrength, obs_module_text("motion_strength_tooltip"));

	obs_properties_add_bool(motion, "mirror", obs_module_text("mirror_label"));

	// Apply initial visibility for speed/strength based on current settings
	if (tuber && tuber->source) {
		obs_data_t *settings = obs_source_get_settings(tuber->source);
		if (settings) {
			on_motion_type_changed(motion, p_mtype, settings);
			on_show_extra_frames_changed(props, p_extra, settings);
			on_show_slots_changed(props, p_show_slots, settings);
			obs_data_release(settings);
		}
	}

	// ── 7. About ──────────────────────────────────────────────────────────
	obs_properties_t *about = obs_properties_create();
	obs_properties_add_group(props, "about_group",
		obs_module_text("about_group"), OBS_GROUP_NORMAL, about);

	obs_properties_add_text(about, "about_version", NULL, OBS_TEXT_INFO);
	obs_properties_add_text(about, "about_author",  NULL, OBS_TEXT_INFO);
	obs_properties_add_button2(about, "about_website_btn",
		obs_module_text("about_website_btn"), open_website_page, data);
	obs_properties_add_button2(about, "about_github_btn",
		obs_module_text("about_github_btn"), open_github_page, data);

	return props;
}
