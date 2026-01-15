#pragma once

#include <vector>
#include <string>
#include <graphics/graphics.h>
#include "lodepng.h"

struct APNGFrame {
    gs_texture_t* texture = nullptr; // The final full-frame texture to render
    uint32_t delay_ms = 100;         // Delay before next frame
};

// Internal structure to hold frame control data
struct APNGFrameInfo {
    uint32_t width = 0, height = 0;
    uint32_t x_offset = 0, y_offset = 0;
    uint32_t seq = 0;
    uint16_t delay_num = 0, delay_den = 0;
    uint8_t dispose_op = 0;
    uint8_t blend_op = 0;
    
    std::vector<unsigned char> compressed_data;
};

class APNGDecoder {
public:
    APNGDecoder();
    ~APNGDecoder();

    bool Load(const char* path);
    void Free();

    gs_texture_t* GetTextureForTime(uint64_t time_ms);
    
    bool IsAnimated() const { return frames.size() > 1; }
    uint32_t GetWidth() const { return width; }
    uint32_t GetHeight() const { return height; }

private:
    std::vector<APNGFrame> frames;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t num_plays = 0;
    uint64_t total_duration_ms = 0;

    bool ParseChunks(const std::vector<unsigned char>& source);
    void DecodeFrame(const APNGFrameInfo& info, std::vector<unsigned char>& canvas, std::vector<unsigned char>& prev_canvas);
};
