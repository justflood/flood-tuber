#include "webp-decoder.h"
#include <fstream>
#include <vector>
#include <webp/decode.h>
#include <webp/demux.h>

#define BLOG(level, format, ...) blog(level, "[WebP-Decoder] " format, ##__VA_ARGS__)

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

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        BLOG(LOG_WARNING, "Failed to open file: %s", path);
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size <= 0) return false;

    std::vector<uint8_t> buffer(size);
    if (!file.read((char*)buffer.data(), size)) {
        return false;
    }

    return DecodeData(buffer.data(), (size_t)size);
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
