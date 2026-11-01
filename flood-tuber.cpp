#include "flood-tuber.h"
#include "flood-tuber-props.h"
#include <util/dstr.h>
#include <util/platform.h>
#include <math.h>
#include <cstdio>
#include <string>
#include <vector>
#include <utility>

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
                     flood_image_file_init(&image->obs_image, path);
                     flood_image_file_init_texture(&image->obs_image);
                }
            } else {
                image->type = FloodImage::OBS_STANDARD;
                flood_image_file_init(&image->obs_image, path);
                flood_image_file_init_texture(&image->obs_image);
            }
        }
    }
    image->anim_time_ns = 0;
	obs_leave_graphics();
}

// Loads an image from a setting key; if the stored path was a built-in avatar
// in a legacy installation directory that no longer exists, resolves it via obs_module_file.
static void update_image_setting(FloodImage *image, obs_data_t *settings, const char *setting_key)
{
	const char *path = obs_data_get_string(settings, setting_key);
	if (path && *path && !os_file_exists(path)) {
		struct dstr norm = {0};
		dstr_copy(&norm, path);
		dstr_replace(&norm, "\\", "/");

		const char *marker = "/avatars/";
		const char *found = NULL;
		const char *search = norm.array;
		while ((search = strstr(search, marker)) != NULL) {
			found = search;
			search += strlen(marker);
		}

		if (found) {
			const char *rel_path = found + 1; // "avatars/<AvatarName>/<file>"
			char *new_path = obs_module_file(rel_path);
			if (new_path && os_file_exists(new_path)) {
				BLOG(LOG_INFO, "Migrated legacy avatar path '%s' -> '%s'", path, new_path);
				obs_data_set_string(settings, setting_key, new_path);
				update_image(image, new_path);
				bfree(new_path);
				dstr_free(&norm);
				return;
			}
			if (new_path)
				bfree(new_path);
		}
		dstr_free(&norm);
	}
	update_image(image, path);
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
        flood_image_file_tick(&img->obs_image, elapsed_ns);
        flood_image_file_update_texture(&img->obs_image);
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



// ── Crossfade between avatars ─────────────────────────────────────────────

// Hash of the 9 image paths. It changes only when the avatar itself changes.
static uint64_t avatar_paths_hash(obs_data_t *settings)
{
	static const char *const keys[] = {
		"path_idle",         "path_blink",        "path_action",
		"path_talk_1",       "path_talk_2",       "path_talk_3",
		"path_talk_1_blink", "path_talk_2_blink", "path_talk_3_blink",
	};

	uint64_t hash = 1469598103934665603ULL; // FNV-1a, 64 bits
	for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
		const char *s = obs_data_get_string(settings, keys[k]);
		if (!s)
			s = "";
		for (; *s; s++) {
			hash ^= (uint64_t)(unsigned char)*s;
			hash *= 1099511628211ULL;
		}
		hash ^= 0xFFULL; // separator between two paths
		hash *= 1099511628211ULL;
	}
	return hash;
}

// Shader that blends the outgoing avatar (texA) and the new one (texB) pixel by pixel.
// Each pixel mixes the two colors weighted by their own alpha, so transparent areas stay
// transparent and no dark halo appears mid-fade. rect_a / rect_b give where each image
// sits inside the shared reference box (x, y, width, height).
static const char *const FLOOD_FADE_EFFECT = R"effect(
uniform float4x4 ViewProj;
uniform texture2d texA;
uniform texture2d texB;
uniform float2 box_size;
uniform float4 rect_a;
uniform float4 rect_b;
uniform float fade_t;

sampler_state textureSampler {
	Filter   = Linear;
	AddressU = Clamp;
	AddressV = Clamp;
};

struct VertData {
	float4 pos : POSITION;
	float2 uv  : TEXCOORD0;
};

VertData VSFade(VertData v_in)
{
	VertData v_out;
	v_out.pos = mul(float4(v_in.pos.xyz, 1.0), ViewProj);
	v_out.uv  = v_in.uv;
	return v_out;
}

// 1 when the image has a size and the point lies inside it, 0 otherwise
float image_mask(float2 uv, float4 rect)
{
	bool valid  = rect.z > 0.0 && rect.w > 0.0;
	bool inside = uv.x >= 0.0 && uv.y >= 0.0 && uv.x <= 1.0 && uv.y <= 1.0;
	return (valid && inside) ? 1.0 : 0.0;
}

float4 PSFade(VertData v_in) : TARGET
{
	float2 p  = v_in.uv * box_size;
	float2 ua = (p - rect_a.xy) / max(rect_a.zw, float2(0.0001, 0.0001));
	float2 ub = (p - rect_b.xy) / max(rect_b.zw, float2(0.0001, 0.0001));

	float4 a = texA.Sample(textureSampler, saturate(ua)) * image_mask(ua, rect_a);
	float4 b = texB.Sample(textureSampler, saturate(ub)) * image_mask(ub, rect_b);

	float wa    = a.a * (1.0 - fade_t);
	float wb    = b.a * fade_t;
	float alpha = wa + wb;
	float3 rgb  = (a.rgb * wa + b.rgb * wb) / max(alpha, 0.0001);
	return float4(rgb, alpha);
}

technique Draw
{
	pass
	{
		vertex_shader = VSFade(v_in);
		pixel_shader  = PSFade(v_in);
	}
}
)effect";

// Creates the fade shader once per source. Must run in the graphics context.
static bool ensure_fade_effect(struct flood_tuber_data *data)
{
	if (data->fade_effect)
		return true;
	if (data->fade_effect_failed)
		return false;

	char *errors = NULL;
	data->fade_effect = gs_effect_create(FLOOD_FADE_EFFECT, "flood-tuber-fade.effect", &errors);
	if (!data->fade_effect) {
		data->fade_effect_failed = true;
		BLOG(LOG_WARNING, "Avatar fade disabled, shader failed to compile: %s",
		     errors ? errors : "unknown error");
	}
	if (errors)
		bfree(errors);
	return data->fade_effect != NULL;
}

// Called from flood_tuber_update() when the avatar changes, in the graphics context.
// The displayed avatar moves to 'prev' so its textures stay alive during the fade,
// then 'cur' is reloaded with the new avatar. A fade still running is cut short:
// the latest request wins.
static void begin_avatar_fade(struct flood_tuber_data *data, bool fade)
{
	data->prev.Free();
	data->transition.active = false;

	if (!fade || !ensure_fade_effect(data))
		return;

	std::swap(data->prev, data->cur);
	data->transition.elapsed = 0.0f;
	data->transition.duration = data->fade_duration;
	data->transition.active = true;
}

// Advances the animation of every image of one avatar
static void flood_avatar_tick(AvatarImages *set, uint64_t elapsed_ns)
{
	flood_image_tick(&set->image_idle, elapsed_ns);
	flood_image_tick(&set->image_blink, elapsed_ns);
	flood_image_tick(&set->image_action, elapsed_ns);
	flood_image_tick(&set->image_talking_1, elapsed_ns);
	flood_image_tick(&set->image_talking_2, elapsed_ns);
	flood_image_tick(&set->image_talking_3, elapsed_ns);
	flood_image_tick(&set->image_talking_1_blink, elapsed_ns);
	flood_image_tick(&set->image_talking_2_blink, elapsed_ns);
	flood_image_tick(&set->image_talking_3_blink, elapsed_ns);
}

// Advances the fade by 'seconds'. Runs in the graphics context: the outgoing avatar is released at the end.
static void update_avatar_transition(struct flood_tuber_data *data, float seconds)
{
	data->transition.elapsed += seconds;
	if (data->transition.elapsed >= data->transition.duration) {
		data->transition.elapsed = data->transition.duration;
		data->transition.active = false;
		data->prev.Free();
	}
}

// Rectangle where a texture is drawn inside the reference box: aspect kept, centered
struct FloodFitRect {
	int x;
	int y;
	int w;
	int h;
};

static FloodFitRect fit_in_reference_box(gs_texture_t *tex, uint32_t ref_w, uint32_t ref_h)
{
	FloodFitRect r = {0, 0, 0, 0};
	if (!tex)
		return r;

	uint32_t tw = gs_texture_get_width(tex);
	uint32_t th = gs_texture_get_height(tex);
	float scale = (tw && th) ? fminf((float)ref_w / (float)tw, (float)ref_h / (float)th) : 1.0f;

	r.w = (int)lroundf((float)tw * scale);
	r.h = (int)lroundf((float)th * scale);
	r.x = ((int)ref_w - r.w) / 2;
	r.y = ((int)ref_h - r.h) / 2;
	return r;
}

// Picks the frame to show from one avatar's images, according to the current animation state
static gs_texture_t *select_avatar_texture(struct flood_tuber_data *data, AvatarImages *s)
{
	gs_texture_t *tex = flood_image_get_texture(&s->image_idle);

	if (data->current_state == AvatarState::ACTION && flood_image_get_texture(&s->image_action)) {
		tex = flood_image_get_texture(&s->image_action);
	}
	else if (data->current_state == AvatarState::TALKING) {
		bool blink = data->is_blinking_now;

		int idx = data->talking_frame_index;
		FloodImage *talk_img = nullptr;
		FloodImage *talk_blink_img = nullptr;

		// Select based on index
		if (idx == 0) {
			talk_img = &s->image_talking_1;
			talk_blink_img = &s->image_talking_1_blink;
		} else if (idx == 1) {
			talk_img = &s->image_talking_2;
			talk_blink_img = &s->image_talking_2_blink;
		} else {
			talk_img = &s->image_talking_3;
			talk_blink_img = &s->image_talking_3_blink;
		}

		// Fallbacks
		if (!flood_image_get_texture(talk_img) && idx == 2) {
			talk_img = &s->image_talking_2;
			talk_blink_img = &s->image_talking_2_blink;
			idx = 1;
		}
		if (!flood_image_get_texture(talk_img) && idx == 1) {
			talk_img = &s->image_talking_1;
			talk_blink_img = &s->image_talking_1_blink;
		}

		// Determine final texture
		if (blink && talk_blink_img && flood_image_get_texture(talk_blink_img)) {
			tex = flood_image_get_texture(talk_blink_img);
		} else if (talk_img && flood_image_get_texture(talk_img)) {
			tex = flood_image_get_texture(talk_img);
		}

		// Final fallback for blinking if specific talk-blink is missing
		if (blink && (!talk_blink_img || !flood_image_get_texture(talk_blink_img)) && flood_image_get_texture(&s->image_blink)) {
			tex = flood_image_get_texture(&s->image_blink);
		}
	}
	else {
		if (data->is_blinking_now && flood_image_get_texture(&s->image_blink)) {
			tex = flood_image_get_texture(&s->image_blink);
		} else {
			tex = flood_image_get_texture(&s->image_idle);
		}
	}
	return tex;
}

// Draws the outgoing and the new avatar in the reference box, blended by the fade shader.
// The caller has already set the matrix (motion offset, mirror).
static void draw_avatar_crossfade(struct flood_tuber_data *data, gs_texture_t *tex_old, gs_texture_t *tex_new,
				  uint32_t ref_w, uint32_t ref_h)
{
	gs_effect_t *effect = data->fade_effect;
	gs_texture_t *any_tex = tex_new ? tex_new : tex_old;

	float t = 1.0f;
	if (data->transition.duration > 0.0f)
		t = data->transition.elapsed / data->transition.duration;
	t = fminf(fmaxf(t, 0.0f), 1.0f);
	if (data->fade_smooth)
		t = t * t * (3.0f - 2.0f * t);

	FloodFitRect ra = fit_in_reference_box(tex_old, ref_w, ref_h);
	FloodFitRect rb = fit_in_reference_box(tex_new, ref_w, ref_h);

	struct vec2 box_px;
	struct vec4 vec_a;
	struct vec4 vec_b;
	vec2_set(&box_px, (float)ref_w, (float)ref_h);
	vec4_set(&vec_a, (float)ra.x, (float)ra.y, (float)ra.w, (float)ra.h);
	vec4_set(&vec_b, (float)rb.x, (float)rb.y, (float)rb.w, (float)rb.h);

	gs_eparam_t *p_a = gs_effect_get_param_by_name(effect, "texA");
	gs_eparam_t *p_b = gs_effect_get_param_by_name(effect, "texB");
	gs_eparam_t *p_box = gs_effect_get_param_by_name(effect, "box_size");
	gs_eparam_t *p_ra = gs_effect_get_param_by_name(effect, "rect_a");
	gs_eparam_t *p_rb = gs_effect_get_param_by_name(effect, "rect_b");
	gs_eparam_t *p_t = gs_effect_get_param_by_name(effect, "fade_t");

	// Same sRGB handling as obs_source_draw()
	const bool linear_srgb = gs_get_linear_srgb();
	const bool srgb_previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(linear_srgb);

	// A missing avatar is bound to the other texture; its rectangle is empty, so nothing is drawn for it
	gs_texture_t *bind_a = tex_old ? tex_old : any_tex;
	gs_texture_t *bind_b = tex_new ? tex_new : any_tex;
	if (linear_srgb) {
		gs_effect_set_texture_srgb(p_a, bind_a);
		gs_effect_set_texture_srgb(p_b, bind_b);
	} else {
		gs_effect_set_texture(p_a, bind_a);
		gs_effect_set_texture(p_b, bind_b);
	}
	gs_effect_set_vec2(p_box, &box_px);
	gs_effect_set_vec4(p_ra, &vec_a);
	gs_effect_set_vec4(p_rb, &vec_b);
	gs_effect_set_float(p_t, t);

	gs_blend_state_push();
	gs_reset_blend_state();
	while (gs_effect_loop(effect, "Draw"))
		gs_draw_sprite(any_tex, 0, ref_w, ref_h);
	gs_blend_state_pop();

	gs_enable_framebuffer_srgb(srgb_previous);
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
// ── Raccourcis : changement d'avatar ─────────────────────────────────────

// Copie les réglages audio d'un jeu de settings vers un autre
static void copy_audio_keys(obs_data_t *from, obs_data_t *to)
{
	obs_data_set_string(to, "audio_source",  obs_data_get_string(from, "audio_source"));
	obs_data_set_double(to, "threshold",     obs_data_get_double(from, "threshold"));
	obs_data_set_int(to,    "release_delay", obs_data_get_int(from, "release_delay"));
	obs_data_set_double(to, "talking_speed", obs_data_get_double(from, "talking_speed"));
}

// Chemin unique de changement d'avatar : bouton Charger, cycle, emplacements.
// Si le dossier de l'avatar est introuvable, rien n'est modifié.
bool switch_to_avatar(struct flood_tuber_data *tuber, const char *name)
{
	obs_data_t *settings = obs_source_get_settings(tuber->source);
	obs_data_t *audio = obs_data_create();
	copy_audio_keys(settings, audio);

	if (!apply_avatar_to_settings(settings, name, false)) {
		BLOG(LOG_WARNING, "Avatar introuvable, changement annulé : %s", name);
		obs_data_release(audio);
		obs_data_release(settings);
		return false;
	}

	if (obs_data_get_bool(settings, "keep_audio_on_switch"))
		copy_audio_keys(audio, settings);

	obs_data_set_string(settings, "avatar_list", name);
	obs_source_update(tuber->source, settings);
	obs_data_release(audio);
	obs_data_release(settings);
	return true;
}

// Avatar suivant (+1) ou précédent (-1), dans l'ordre de la liste
static void switch_by_offset(struct flood_tuber_data *tuber, int offset)
{
	obs_data_t *settings = obs_source_get_settings(tuber->source);
	std::vector<std::string> names;
	flood_tuber_list_avatars(settings, names);
	std::string current = obs_data_get_string(settings, "avatar_list");
	obs_data_release(settings);

	if (names.empty()) return;

	int n = (int)names.size();
	int idx = 0;
	for (int i = 0; i < n; i++) {
		if (names[i] == current) { idx = i; break; }
	}
	switch_to_avatar(tuber, names[((idx + offset) % n + n) % n].c_str());
}

// Emplacement 1..10 : lit slot_N puis bascule. Emplacement vide = rien.
static void switch_to_slot(struct flood_tuber_data *tuber, int index)
{
	char key[16];
	snprintf(key, sizeof(key), "slot_%d", index);

	obs_data_t *settings = obs_source_get_settings(tuber->source);
	std::string name = obs_data_get_string(settings, key);
	obs_data_release(settings);

	if (!name.empty())
		switch_to_avatar(tuber, name.c_str());
}

static void on_hotkey_next(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (pressed) switch_by_offset((struct flood_tuber_data *)data, +1);
}

static void on_hotkey_prev(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (pressed) switch_by_offset((struct flood_tuber_data *)data, -1);
}

static void on_hotkey_slot(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	struct flood_slot_hotkey *hk = (struct flood_slot_hotkey *)data;
	if (pressed) switch_to_slot(hk->tuber, hk->index);
}

static void *flood_tuber_create(obs_data_t *settings, obs_source_t *source)
{
	struct flood_tuber_data *data = (struct flood_tuber_data *)bzalloc(sizeof(struct flood_tuber_data));
	data->source = source;
	data->current_db = -100.0f;
	data->current_state = AvatarState::IDLE;
	data->talking_frame_index = 0;
	data->timer_release_hold = 0.0f;

	// Raccourcis : suivant, précédent, puis emplacements 1..10
	data->hotkey_next = obs_hotkey_register_source(source, "flood_tuber.avatar_next",
		obs_module_text("hotkey_avatar_next"), on_hotkey_next, data);
	data->hotkey_prev = obs_hotkey_register_source(source, "flood_tuber.avatar_prev",
		obs_module_text("hotkey_avatar_prev"), on_hotkey_prev, data);

	for (int i = 0; i < FLOOD_SLOT_COUNT; i++) {
		struct flood_slot_hotkey *hk = &data->slot_hotkeys[i];
		hk->tuber = data;
		hk->index = i + 1;

		char name[32], desc[64];
		snprintf(name, sizeof(name), "flood_tuber.slot_%d", hk->index);
		snprintf(desc, sizeof(desc), "%s %d", obs_module_text("hotkey_slot"), hk->index);
		hk->id = obs_hotkey_register_source(source, name, desc, on_hotkey_slot, hk);
	}

	obs_source_update(source, settings);
	return data;
}

static void flood_tuber_remove_audio_capture(struct flood_tuber_data *data)
{
    if (!data->audio_source)
        return;

    obs_source_t *source = obs_weak_source_get_source(data->audio_source);
    if (source) {
        obs_source_remove_audio_capture_callback(source, audio_callback, data);
        obs_source_release(source);
    }
    obs_weak_source_release(data->audio_source);
    data->audio_source = nullptr;
}

// The mic source often does not exist yet when this source is created.
// Keep the selected name and attach the capture once the source appears.
static void flood_tuber_try_audio_capture(struct flood_tuber_data *data)
{
    if (data->audio_source) {
        obs_source_t *existing = obs_weak_source_get_source(data->audio_source);
        if (existing) {
            obs_source_release(existing);
            return;
        }
        obs_weak_source_release(data->audio_source);
        data->audio_source = nullptr;
    }

    if (!data->audio_source_name[0])
        return;

    uint64_t now = os_gettime_ns();
    if (now - data->capture_check_time < 3000000000ULL)
        return;
    data->capture_check_time = now;

    obs_source_t *capture = obs_get_source_by_name(data->audio_source_name);
    if (!capture)
        return;

    data->audio_source = obs_source_get_weak_source(capture);
    obs_source_add_audio_capture_callback(capture, audio_callback, data);
    BLOG(LOG_INFO, "Added audio capture to '%s'", obs_source_get_name(capture));
    obs_source_release(capture);
}

// Plugin Cleanup: Frees memory and releases resources
static void flood_tuber_destroy(void *d)
{
	struct flood_tuber_data *data = (struct flood_tuber_data *)d;

	flood_tuber_remove_audio_capture(data);
	obs_enter_graphics();
		data->cur.Free();
		data->prev.Free();
		if (data->fade_effect) {
			gs_effect_destroy(data->fade_effect);
			data->fade_effect = NULL;
		}
	obs_leave_graphics();
	obs_hotkey_unregister(data->hotkey_next);
	obs_hotkey_unregister(data->hotkey_prev);
	for (int i = 0; i < FLOOD_SLOT_COUNT; i++)
		obs_hotkey_unregister(data->slot_hotkeys[i].id);

	bfree(data);
}


// Update Settings: Called when user changes property values
static void flood_tuber_update(void *data_ptr, obs_data_t *settings)
{
	struct flood_tuber_data *data = (struct flood_tuber_data *)data_ptr;

	// -- Avatar crossfade --
	const char *fade_easing = obs_data_get_string(settings, "avatar_fade_easing");
	int fade_ms = (int)obs_data_get_int(settings, "avatar_fade_duration");
	data->fade_enabled = obs_data_get_bool(settings, "avatar_fade_enabled");
	data->fade_smooth = fade_easing && strcmp(fade_easing, "Smooth") == 0;
	data->fade_duration = (float)(fade_ms < 0 ? 0 : (fade_ms > 5000 ? 5000 : fade_ms)) / 1000.0f;

	// The avatar changed if one of its image paths changed (checked before the images are reloaded)
	uint64_t avatar_hash = avatar_paths_hash(settings);
	bool avatar_changed = data->avatar_ready && avatar_hash != data->avatar_hash;
	data->avatar_hash = avatar_hash;
	data->avatar_ready = true;

	if (avatar_changed || (!data->fade_enabled && data->transition.active)) {
		obs_enter_graphics();
		begin_avatar_fade(data, avatar_changed && data->fade_enabled && data->fade_duration > 0.0f);
		obs_leave_graphics();
	}

	update_image_setting(&data->cur.image_idle, settings, "path_idle");
	update_image_setting(&data->cur.image_blink, settings, "path_blink");
	update_image_setting(&data->cur.image_action, settings, "path_action");
	update_image_setting(&data->cur.image_talking_1, settings, "path_talk_1");
	update_image_setting(&data->cur.image_talking_2, settings, "path_talk_2");
	update_image_setting(&data->cur.image_talking_3, settings, "path_talk_3");
	update_image_setting(&data->cur.image_talking_1_blink, settings, "path_talk_1_blink");
	update_image_setting(&data->cur.image_talking_2_blink, settings, "path_talk_2_blink");
	update_image_setting(&data->cur.image_talking_3_blink, settings, "path_talk_3_blink");

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
	else if (strcmp(motion_type, "Oscillating") == 0)
		data->talk_effect = TalkingEffect::OSCILLATING;
	else
		data->talk_effect = TalkingEffect::NONE;

	data->effect_speed = (float)obs_data_get_int(settings, "motion_speed") / 100.0f;
	data->effect_strength = (float)obs_data_get_int(settings, "motion_strength");
	data->mirror = obs_data_get_bool(settings, "mirror");
	data->reference_height = (uint32_t)obs_data_get_int(settings, "reference_height");
	if (data->reference_height < 16) data->reference_height = 512;
	data->talk_interval = (float)obs_data_get_double(settings, "talking_speed");
	if (data->talk_interval < 0.01f) data->talk_interval = 0.01f;

	const char *audio_source_name = obs_data_get_string(settings, "audio_source");
    if (!audio_source_name || !audio_source_name[0]) {
        flood_tuber_remove_audio_capture(data);
        data->audio_source_name[0] = '\0';
    } else if (strcmp(data->audio_source_name, audio_source_name) != 0) {
        flood_tuber_remove_audio_capture(data);
        size_t name_len = strlen(audio_source_name);
        if (name_len >= sizeof(data->audio_source_name))
            name_len = sizeof(data->audio_source_name) - 1;
        memcpy(data->audio_source_name, audio_source_name, name_len);
        data->audio_source_name[name_len] = '\0';
        data->capture_check_time = os_gettime_ns() - 3000000000ULL;
    }

	data->time_until_next_action = data->action_interval_min;
	data->time_until_next_blink = data->blink_interval_min;
}

// Main Tick: Handles state logic (Idle/Talking switching, timers for blinking and actions)
static void flood_tuber_tick(void *data_ptr, float seconds)
{
	struct flood_tuber_data *data = (struct flood_tuber_data *)data_ptr;

	flood_tuber_try_audio_capture(data);

	// Tick animations for all images
	// Convert seconds to nanoseconds for flood_image_file_tick
	uint64_t elapsed_ns = (uint64_t)(seconds * 1000000000.0f);

    obs_enter_graphics(); // Required for standard texture updates

	flood_avatar_tick(&data->cur, elapsed_ns);
	if (data->transition.active) {
		flood_avatar_tick(&data->prev, elapsed_ns);
		update_avatar_transition(data, seconds);
	}

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

	if (flood_image_get_texture(&data->cur.image_blink)) {
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

		// Amplitude en % de la hauteur de référence : indépendante de la résolution des PNG
		float amp = data->effect_strength * 0.01f * (float)data->reference_height;

		if (data->talk_effect == TalkingEffect::BOUNCE) {
			data->offset_y = -fabsf(sinf(data->timer_effect * 5.0f)) * amp;
		}
		else if (data->talk_effect == TalkingEffect::SHAKE) {
			data->offset_x = sinf(data->timer_effect * 25.0f) * amp * 0.8f;
			data->offset_y = cosf(data->timer_effect * 20.0f) * amp * 0.8f;
		}
		else if (data->talk_effect == TalkingEffect::OSCILLATING) {
			// Dérive douce en huit : x suit une sinusoïde, y une sinusoïde deux fois plus rapide
			data->offset_x = sinf(data->timer_effect * 3.0f) * amp;
			data->offset_y = sinf(data->timer_effect * 6.0f) * amp * 0.5f;
		}
	} else {
		data->timer_effect = 0.0f;
	}
}

// Boîte commune d'affichage : hauteur réglée par l'utilisateur, largeur selon le ratio
// de l'image au repos. Seul le ratio compte, jamais la taille en pixels.
static void flood_tuber_reference_size(struct flood_tuber_data *data, uint32_t *w, uint32_t *h)
{
	uint32_t ref_h = data->reference_height ? data->reference_height : 512;
	uint32_t iw = flood_image_get_width(&data->cur.image_idle);
	uint32_t ih = flood_image_get_height(&data->cur.image_idle);
	float aspect = (iw && ih) ? (float)iw / (float)ih : 1.0f;

	*h = ref_h;
	*w = (uint32_t)lroundf((float)ref_h * aspect);
	if (*w < 1) *w = 1;
}

// Render: draws the current avatar. While a fade runs, the outgoing and the new avatar are blended.
static void flood_tuber_render(void *data_ptr, gs_effect_t *effect)
{
	struct flood_tuber_data *data = (struct flood_tuber_data *)data_ptr;
	UNUSED_PARAMETER(effect);

	gs_texture_t *tex = select_avatar_texture(data, &data->cur);
	bool fading = data->transition.active && data->fade_effect != nullptr;
	gs_texture_t *tex_old = fading ? select_avatar_texture(data, &data->prev) : nullptr;

	if (!tex && !tex_old)
		return;

	// Every frame is fitted in the same box (aspect kept, centered), so all images share
	// the same height and width on screen, and the motion offsets stay identical.
	uint32_t ref_w, ref_h;
	flood_tuber_reference_size(data, &ref_w, &ref_h);

	gs_matrix_push();
	gs_matrix_translate3f(data->offset_x, data->offset_y, 0.0f);
	if (data->mirror) {
		gs_matrix_translate3f((float)ref_w, 0.0f, 0.0f);
		gs_matrix_scale3f(-1.0f, 1.0f, 1.0f);
	}

	if (fading) {
		draw_avatar_crossfade(data, tex_old, tex, ref_w, ref_h);
	} else if (tex) {
		FloodFitRect r = fit_in_reference_box(tex, ref_w, ref_h);
		obs_source_draw(tex, r.x, r.y, (uint32_t)r.w, (uint32_t)r.h, false);
	}

	gs_matrix_pop();
}

static uint32_t flood_tuber_get_width(void *data_ptr)
{
	struct flood_tuber_data *data = (struct flood_tuber_data *)data_ptr;
	uint32_t w, h;
	flood_tuber_reference_size(data, &w, &h);
	return w;
}
static uint32_t flood_tuber_get_height(void *data_ptr)
{
	struct flood_tuber_data *data = (struct flood_tuber_data *)data_ptr;
	uint32_t w, h;
	flood_tuber_reference_size(data, &w, &h);
	return h;
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
