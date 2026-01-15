#include "apng-decoder.h"
#include <util/platform.h>
#include <obs-module.h>
#include <fstream>

#define BLOG(level, format, ...) blog(level, "[APNG-Decoder] " format, ##__VA_ARGS__)

// Helper to read big-endian integers
static uint32_t read_u32(const unsigned char* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}
static uint16_t read_u16(const unsigned char* p) {
    return (p[0] << 8) | p[1];
}

// Helper to write big-endian integers
static void set_u32_be(unsigned char* p, uint32_t val) {
    p[0] = (unsigned char)((val >> 24) & 0xFF);
    p[1] = (unsigned char)((val >> 16) & 0xFF);
    p[2] = (unsigned char)((val >> 8) & 0xFF);
    p[3] = (unsigned char)(val & 0xFF);
}

APNGDecoder::APNGDecoder() {}

APNGDecoder::~APNGDecoder() {
    Free();
}

void APNGDecoder::Free() {
    for (auto& f : frames) {
        if (f.texture) {
            obs_enter_graphics();
            gs_texture_destroy(f.texture);
            obs_leave_graphics();
        }
    }
    frames.clear();
    width = 0;
    height = 0;
}

bool APNGDecoder::Load(const char* path) {
    Free();

    std::vector<unsigned char> file_data;
    unsigned error = lodepng::load_file(file_data, path);
    if (error) {
        BLOG(LOG_WARNING, "Failed to load file %s: %s", path, lodepng_error_text(error));
        return false;
    }

    if (!ParseChunks(file_data)) {
        // Fallback: Try loading as standard static PNG
        BLOG(LOG_INFO, "Not an APNG, loading as static PNG: %s", path);
        
        std::vector<unsigned char> image;
        unsigned w, h;
        error = lodepng::decode(image, w, h, file_data);
        if (error) return false;

        width = w;
        height = h;

        APNGFrame frame;
        frame.delay_ms = 1000;
        
        obs_enter_graphics();
        const uint8_t* data_ptr = image.data();
        frame.texture = gs_texture_create(w, h, GS_RGBA, 1, &data_ptr, GS_DYNAMIC);
        obs_leave_graphics();
        
        if (frame.texture) {
             gs_texture_set_image(frame.texture, image.data(), w * 4, false); // safety
             frames.push_back(frame);
             return true;
        }
        return false;
    }

    return !frames.empty();
}

bool APNGDecoder::ParseChunks(const std::vector<unsigned char>& source) {
    if (source.size() < 29) return false; // Too small
    
    // Check Signature
    if (source[0] != 137 || source[1] != 80 || source[2] != 78) return false;

    // Read IHDR
    width = read_u32(&source[16]);
    height = read_u32(&source[20]);

    std::vector<APNGFrameInfo> frame_infos;
    APNGFrameInfo current_info;
    bool is_apng = false;
    uint32_t first_frame_seq = 0;

    std::vector<unsigned char> common_chunks;

    size_t pos = 8;
    // Iterate chunks
    while (pos < source.size()) {
        if (pos + 8 > source.size()) break;
        uint32_t len = read_u32(&source[pos]);
        
        const unsigned char* chunk_data = &source[pos + 8];
        uint32_t type = read_u32(&source[pos + 4]);

        // debug: Log chunk type (as string)
        char type_str[5];
        memcpy(type_str, &source[pos + 4], 4);
        type_str[4] = '\0';
        BLOG(LOG_DEBUG, "Chunk: %s, Len: %u", type_str, len);

        if (type == 0x6163544C) { // acTL
            is_apng = true;
            num_plays = read_u32(chunk_data + 4);
            BLOG(LOG_INFO, "Found acTL (APNG)! NumPlays: %u", num_plays);
        }
        else if (type == 0x6663544C) { // fcTL
            // Finish previous frame info if valid
            if (!current_info.compressed_data.empty()) {
                frame_infos.push_back(current_info);
                current_info = APNGFrameInfo();
                current_info.compressed_data.clear();
            }

            current_info.seq = read_u32(chunk_data);
            current_info.width = read_u32(chunk_data + 4);
            current_info.height = read_u32(chunk_data + 8);
            current_info.x_offset = read_u32(chunk_data + 12);
            current_info.y_offset = read_u32(chunk_data + 16);
            current_info.delay_num = read_u16(chunk_data + 20);
            current_info.delay_den = read_u16(chunk_data + 22);
            current_info.dispose_op = chunk_data[24];
            current_info.blend_op = chunk_data[25];
            
            BLOG(LOG_DEBUG, "fcTL Frame: %ux%u, Pos(%u,%u), Delay(%u/%u), Seq:%u", 
                current_info.width, current_info.height, 
                current_info.x_offset, current_info.y_offset, 
                current_info.delay_num, current_info.delay_den, current_info.seq);
        }
        else if (type == 0x66644154) { // fdAT
            if (is_apng) {
                 // Sequence (4 bytes) + Data
                 if (len > 4) {
                     current_info.compressed_data.insert(current_info.compressed_data.end(), chunk_data + 4, chunk_data + len);
                 }
            }
        }
        else if (type == 0x49444154) { // IDAT
             current_info.compressed_data.insert(current_info.compressed_data.end(), chunk_data, chunk_data + len);
        }
        else if (type == 0x49454E44) { // IEND
             if (!current_info.compressed_data.empty()) {
                frame_infos.push_back(current_info);
             }
             break;
        }
        else if (type != 0x49484452) { // Not IHDR (handled manually), copy other chunks (PLTE, tRNS, etc.)
             // Append entire chunk (Length + Type + Data + CRC)
             common_chunks.insert(common_chunks.end(), &source[pos], &source[pos] + 8 + len + 4);
        }

        pos += 8 + len + 4; // Length + Type + Data + CRC
    }
    
    if (!is_apng) return false;

    // Now decode all collected frames
    std::vector<unsigned char> canvas(width * height * 4, 0);       // Current full canvas
    
    total_duration_ms = 0;

    obs_enter_graphics();

    for (const auto& info : frame_infos) {
        // Construct PNG
        std::vector<unsigned char> png_data;
        // Signature
        const unsigned char sig[] = {137, 80, 78, 71, 13, 10, 26, 10};
        png_data.insert(png_data.end(), sig, sig + 8);
        
        // IHDR
        if (source.size() >= 33) {
            std::vector<unsigned char> ihdr_body(13);
            memcpy(ihdr_body.data(), &source[16], 13); // Copy IHDR body from original
            
            // Set W, H for this frame
            set_u32_be(ihdr_body.data() + 0, info.width);
            set_u32_be(ihdr_body.data() + 4, info.height);
            
            // Create chunk
            unsigned char header[8];
            set_u32_be(header, 13);
            header[4] = 'I'; header[5] = 'H'; header[6] = 'D'; header[7] = 'R';
            png_data.insert(png_data.end(), header, header + 8);
            png_data.insert(png_data.end(), ihdr_body.begin(), ihdr_body.end());
            
            // CRC
            std::vector<unsigned char> check_buf;
            check_buf.push_back('I'); check_buf.push_back('H'); check_buf.push_back('D'); check_buf.push_back('R');
            check_buf.insert(check_buf.end(), ihdr_body.begin(), ihdr_body.end());
            unsigned crc = lodepng_crc32(check_buf.data(), check_buf.size());
            
            unsigned char crc_b[4]; set_u32_be(crc_b, crc);
            png_data.insert(png_data.end(), crc_b, crc_b + 4);
        }

        // Insert Common Chunks (PLTE, tRNS, gAMA...)
        png_data.insert(png_data.end(), common_chunks.begin(), common_chunks.end());
        
        // IDAT (Joined data)
        if (!info.compressed_data.empty()) {
             unsigned char header[8];
             set_u32_be(header, (uint32_t)info.compressed_data.size());
             header[4] = 'I'; header[5] = 'D'; header[6] = 'A'; header[7] = 'T';
             png_data.insert(png_data.end(), header, header + 8);
             png_data.insert(png_data.end(), info.compressed_data.begin(), info.compressed_data.end());
             
             // CRC
             std::vector<unsigned char> check_buf;
             check_buf.push_back('I'); check_buf.push_back('D'); check_buf.push_back('A'); check_buf.push_back('T');
             check_buf.insert(check_buf.end(), info.compressed_data.begin(), info.compressed_data.end());
             unsigned crc = lodepng_crc32(check_buf.data(), check_buf.size());

             unsigned char crc_b[4]; set_u32_be(crc_b, crc);
             png_data.insert(png_data.end(), crc_b, crc_b + 4);
        }

        // IEND
        const unsigned char iend[] = {0,0,0,0, 'I','E','N','D', 174,66,96,130};
        png_data.insert(png_data.end(), iend, iend + 12);

        // Decode this frame
        std::vector<unsigned char> frame_raw;
        unsigned fw, fh;
        unsigned error = lodepng::decode(frame_raw, fw, fh, png_data);
        
        if (error) {
             BLOG(LOG_WARNING, "LodePNG Decode Error: %u %s", error, lodepng_error_text(error));
        }

        if (!error && fw == info.width && fh == info.height) {
            
            // 1. Snapshot for DISPOSE_PREVIOUS
            std::vector<unsigned char> before_draw;
            if (info.dispose_op == 2) { // APNG_DISPOSE_OP_PREVIOUS
                before_draw = canvas;
            }

            // 2. Blend
            for (size_t y = 0; y < fh; ++y) {
                for (size_t x = 0; x < fw; ++x) {
                    size_t c_x = info.x_offset + x;
                    size_t c_y = info.y_offset + y;
                    if (c_x >= width || c_y >= height) continue;

                    size_t src_idx = (y * fw + x) * 4;
                    size_t dst_idx = (c_y * width + c_x) * 4;

                    unsigned char r = frame_raw[src_idx + 0];
                    unsigned char g = frame_raw[src_idx + 1];
                    unsigned char b = frame_raw[src_idx + 2];
                    unsigned char a = frame_raw[src_idx + 3];

                    if (info.blend_op == 0) { // APNG_BLEND_OP_SOURCE
                        canvas[dst_idx + 0] = r;
                        canvas[dst_idx + 1] = g;
                        canvas[dst_idx + 2] = b;
                        canvas[dst_idx + 3] = a;
                    } else { // APNG_BLEND_OP_OVER
                         float src_a = a / 255.0f;
                         float dst_a = canvas[dst_idx + 3] / 255.0f;
                         float out_a = src_a + dst_a * (1.0f - src_a);

                         if (out_a > 0.0f) {
                             canvas[dst_idx + 0] = (unsigned char)((r * src_a + canvas[dst_idx + 0] * dst_a * (1 - src_a)) / out_a);
                             canvas[dst_idx + 1] = (unsigned char)((g * src_a + canvas[dst_idx + 1] * dst_a * (1 - src_a)) / out_a);
                             canvas[dst_idx + 2] = (unsigned char)((b * src_a + canvas[dst_idx + 2] * dst_a * (1 - src_a)) / out_a);
                             canvas[dst_idx + 3] = (unsigned char)(out_a * 255.0f);
                         } else {
                             canvas[dst_idx + 3] = 0;
                         }
                    }
                }
            }

            // 3. Create Texture
            APNGFrame frame;
            float num = (float)info.delay_num;
            float den = (float)info.delay_den;
            if (den == 0) den = 100.0f;
            frame.delay_ms = (uint32_t)((num / den) * 1000.0f);
            if (frame.delay_ms == 0) frame.delay_ms = 100; // Default min delay

            const uint8_t* data_ptr = canvas.data();
            frame.texture = gs_texture_create(width, height, GS_RGBA, 1, &data_ptr, GS_DYNAMIC);
            frames.push_back(frame);
            total_duration_ms += frame.delay_ms;

            // 4. Dispose
            if (info.dispose_op == 1) { // APNG_DISPOSE_OP_BACKGROUND
                 for (size_t y = 0; y < fh; ++y) {
                    for (size_t x = 0; x < fw; ++x) {
                        size_t c_x = info.x_offset + x;
                        size_t c_y = info.y_offset + y;
                        if (c_x >= width || c_y >= height) continue;
                        size_t dst_idx = (c_y * width + c_x) * 4;
                        memset(&canvas[dst_idx], 0, 4);
                    }
                 }
            } else if (info.dispose_op == 2) { // APNG_DISPOSE_OP_PREVIOUS
                canvas = before_draw;
            }
        }
    }
    
    obs_leave_graphics();
    return true;
}

void APNGDecoder::DecodeFrame(const APNGFrameInfo& info, std::vector<unsigned char>& canvas, std::vector<unsigned char>& prev_canvas) {
    // Unused helper, logic moved inline
}

gs_texture_t* APNGDecoder::GetTextureForTime(uint64_t time_ms) {
    if (frames.empty()) return nullptr;
    if (frames.size() == 1) return frames[0].texture;
    
    uint64_t t = time_ms % total_duration_ms;
    uint64_t current_t = 0;
    
    for (const auto& f : frames) {
        if (t < current_t + f.delay_ms) {
            return f.texture;
        }
        current_t += f.delay_ms;
    }
    return frames.back().texture;
}
