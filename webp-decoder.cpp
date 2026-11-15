#include "webp-decoder.h"
#include <cstdio>
#include <vector>
#include <util/platform.h>
#include <webp/decode.h>
#include <webp/demux.h>

#define BLOG(level, format, ...) blog(level, "[WebP-Decoder] " format, ##__VA_ARGS__)

// Reads a whole file into memory. os_fopen() takes UTF-8 paths on every platform, while
// std::ifstream and fopen() fail on Windows as soon as a folder name has an accent.
static bool read_file_utf8(const char *path, std::vector<unsigned char> &out)
{
    FILE *f = os_fopen(path, "rb");
    if (!f) return false;

    os_fseeki64(f, 0, SEEK_END);
    int64_t size = os_ftelli64(f);
    os_fseeki64(f, 0, SEEK_SET);

    bool ok = size > 0;
    if (ok) {
        out.resize((size_t)size);
        ok = fread(out.data(), 1, (size_t)size, f) == (size_t)size;
    }
    fclose(f);
    if (!ok) out.clear();
    return ok;
}

WebPDecoder::WebPDecoder() {}

WebPDecoder::~WebPDecoder() {
    VerifyFree();
}

void WebPDecoder::VerifyFree() {
    obs_enter_graphics();
    for (auto& frame : frames) {
        if (frame.texture) {
            gs_texture_destroy(frame.texture);
        }
    }
    obs_leave_graphics();
    frames.clear();
    is_animated = false;
    width = 0;
    height = 0;
    total_duration = 0;
}

bool WebPDecoder::Load(const char* path) {
    VerifyFree();

    std::vector<unsigned char> buffer;
    if (!read_file_utf8(path, buffer)) {
        BLOG(LOG_WARNING, "Failed to read file: %s", path);
        return false;
    }

    return DecodeData(buffer.data(), buffer.size());
}

bool WebPDecoder::DecodeData(const uint8_t* data, size_t size) {
    WebPData webp_data;
    webp_data.bytes = data;
    webp_data.size = size;

    WebPAnimDecoderOptions dec_options;
    WebPAnimDecoderOptionsInit(&dec_options);
    dec_options.color_mode = MODE_RGBA;

    WebPAnimDecoder* dec = WebPAnimDecoderNew(&webp_data, &dec_options);
    if (dec == NULL) {
        BLOG(LOG_WARNING, "Failed to create WebPAnimDecoder");
        return false;
    }

    WebPAnimInfo anim_info;
    if (!WebPAnimDecoderGetInfo(dec, &anim_info)) {
        BLOG(LOG_WARNING, "Failed to get animation info");
        WebPAnimDecoderDelete(dec);
        return false;
    }

    width = anim_info.canvas_width;
    height = anim_info.canvas_height;
    loop_count = anim_info.loop_count;
    is_animated = (anim_info.frame_count > 1);

    BLOG(LOG_INFO, "Decoding WebP: %dx%d, Frames: %d, Loops: %d", width, height, anim_info.frame_count, loop_count);

    int prev_timestamp = 0;
    
    // We must decode ALL frames to get correct blending
    obs_enter_graphics();
    
    while (WebPAnimDecoderHasMoreFrames(dec)) {
        uint8_t* buf;
        int timestamp;
        if (!WebPAnimDecoderGetNext(dec, &buf, &timestamp)) {
            break;
        }

        // Create texture for this frame
        gs_texture_t* tex = gs_texture_create(width, height, GS_RGBA, 1, (const uint8_t**)&buf, GS_DYNAMIC);
        if (!tex) {
            BLOG(LOG_WARNING, "Failed to create texture for frame");
            continue;
        }

        WebPFrame frame;
        frame.texture = tex;
        frame.timestamp_ms = timestamp;
        frame.duration_ms = timestamp - prev_timestamp;
        
        frames.push_back(frame);
        prev_timestamp = timestamp;
    }

    obs_leave_graphics();
    WebPAnimDecoderDelete(dec);

    total_duration = prev_timestamp;
    return !frames.empty();
}

gs_texture_t* WebPDecoder::GetTextureForTime(uint64_t time_ms) {
    if (frames.empty()) return NULL;
    if (!is_animated) return frames[0].texture;

    // Handle looping
    if (total_duration > 0) {
        time_ms %= total_duration;
    }

    // Find the frame that covers this timestamp.
    // timestamp_ms is the END time of the frame.
    // So we want the first frame where frame.timestamp_ms > time_ms
    for (const auto& frame : frames) {
        if (frame.timestamp_ms > (int)time_ms) {
            return frame.texture;
        }
    }

    // Fallback to last frame (should be covered by loop logic, but just in case)
    return frames.back().texture;
}
