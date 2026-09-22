#pragma once

#include "twrf/raster/texture.hpp"
#include <vector>
#include <string>
#include <fstream>
#include <cmath>
#include <cstdint>

namespace twrf::raster {

class FrameBuffer {
public:
    FrameBuffer(int width, int height, ColorRGBA clear_col = ColorRGBA::black())
        : width_(width), height_(height),
          pixels_(width * height, clear_col),
          depth_(width * height, 1.0f) {}

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] const std::vector<ColorRGBA>& pixels() const noexcept { return pixels_; }
    [[nodiscard]] const std::vector<float>& depth() const noexcept { return depth_; }

    void clear(ColorRGBA col = ColorRGBA::black(), float depth_val = 1.0f) {
        std::fill(pixels_.begin(), pixels_.end(), col);
        std::fill(depth_.begin(), depth_.end(), depth_val);
    }

    void set_pixel(int x, int y, ColorRGBA col) noexcept {
        if (x >= 0 && x < width_ && y >= 0 && y < height_) {
            pixels_[y * width_ + x] = col;
        }
    }

    [[nodiscard]] ColorRGBA get_pixel(int x, int y) const noexcept {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) return ColorRGBA::black();
        return pixels_[y * width_ + x];
    }

    void set_depth(int x, int y, float d) noexcept {
        if (x >= 0 && x < width_ && y >= 0 && y < height_) {
            depth_[y * width_ + x] = d;
        }
    }

    [[nodiscard]] float get_depth(int x, int y) const noexcept {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) return 1.0f;
        return depth_[y * width_ + x];
    }

    // Blits a rendered tile directly into the master display buffer
    void blit_tile(int tile_x, int tile_y, int tile_w, int tile_h, const ColorRGBA* tile_color) noexcept {
        if (!tile_color) return;
        for (int ty = 0; ty < tile_h; ++ty) {
            int py = tile_y + ty;
            if (py >= height_) break;
            for (int tx = 0; tx < tile_w; ++tx) {
                int px = tile_x + tx;
                if (px >= width_) break;
                pixels_[py * width_ + px] = tile_color[ty * tile_w + tx];
            }
        }
    }

    // Exact bitwise comparison
    [[nodiscard]] bool is_bitwise_identical(const FrameBuffer& other) const noexcept {
        if (width_ != other.width_ || height_ != other.height_) return false;
        return pixels_ == other.pixels_;
    }

    // Counts number of pixels where difference exceeds tolerance
    [[nodiscard]] uint64_t diff_pixel_count(const FrameBuffer& other, int channel_tolerance = 0) const noexcept {
        if (width_ != other.width_ || height_ != other.height_) return width_ * height_;
        uint64_t diff_count = 0;
        for (size_t i = 0; i < pixels_.size(); ++i) {
            int dr = std::abs(static_cast<int>(pixels_[i].r) - static_cast<int>(other.pixels_[i].r));
            int dg = std::abs(static_cast<int>(pixels_[i].g) - static_cast<int>(other.pixels_[i].g));
            int db = std::abs(static_cast<int>(pixels_[i].b) - static_cast<int>(other.pixels_[i].b));
            if (dr > channel_tolerance || dg > channel_tolerance || db > channel_tolerance) {
                diff_count++;
            }
        }
        return diff_count;
    }

    // 64-bit FNV-1a hash over all pixel bytes
    [[nodiscard]] uint64_t compute_hash() const noexcept {
        uint64_t hash = 14695981039346656037ULL;
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(pixels_.data());
        size_t total_bytes = pixels_.size() * sizeof(ColorRGBA);
        for (size_t i = 0; i < total_bytes; ++i) {
            hash ^= bytes[i];
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    // Save as standard PPM image (portable, no dependencies)
    bool save_ppm(const std::string& filename) const {
        std::ofstream ofs(filename, std::ios::binary);
        if (!ofs) return false;
        ofs << "P6\n" << width_ << " " << height_ << "\n255\n";
        for (const auto& p : pixels_) {
            ofs.put(static_cast<char>(p.r));
            ofs.put(static_cast<char>(p.g));
            ofs.put(static_cast<char>(p.b));
        }
        return true;
    }

private:
    int width_{0};
    int height_{0};
    std::vector<ColorRGBA> pixels_;
    std::vector<float> depth_;
};

} // namespace twrf::raster
