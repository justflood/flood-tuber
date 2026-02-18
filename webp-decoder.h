#pragma once

#include <string>
#include <vector>
#include <obs-module.h>
#include <graphics/graphics.h>

struct WebPFrame {
    gs_texture_t* texture;
    int duration_ms;  // Duration of this frame in milliseconds
    int timestamp_ms; // Cumulative timestamp when this frame ends
};

class WebPDecoder {
public:
    WebPDecoder();
    ~WebPDecoder();

    // Load a WebP file from path
    bool Load(const char* path);

    // Free all resources
    void VerifyFree();

    // Get current texture based on elapsed time (in milliseconds)
    // Returns the texture and updates frame index internally if needed,
    // but typically we pass time and get texture.
    gs_texture_t* GetTextureForTime(uint64_t time_ms);

    bool IsAnimated() const { return is_animated; }
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }

private:
    std::vector<WebPFrame> frames;
    bool is_animated = false;
    int width = 0;
    int height = 0;
    int loop_count = 0;
    int total_duration = 0;

    // Helper to decode raw data
    bool DecodeData(const uint8_t* data, size_t size);
};
