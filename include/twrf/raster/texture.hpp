#pragma once

#include <cstdint>
#include <vector>
#include <algorithm>
#include <cmath>

namespace twrf::raster {

struct ColorRGBA {
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};
    uint8_t a{255};

    constexpr ColorRGBA() = default;
    constexpr ColorRGBA(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255)
        : r(r_), g(g_), b(b_), a(a_) {}

    static constexpr ColorRGBA black() noexcept { return {0, 0, 0, 255}; }
    static constexpr ColorRGBA white() noexcept { return {255, 255, 255, 255}; }
    static constexpr ColorRGBA red()   noexcept { return {255, 0, 0, 255}; }
    static constexpr ColorRGBA green() noexcept { return {0, 255, 0, 255}; }
    static constexpr ColorRGBA blue()  noexcept { return {0, 0, 255, 255}; }

    bool operator==(const ColorRGBA& o) const noexcept {
        return r == o.r && g == o.g && b == o.b && a == o.a;
    }

    bool operator!=(const ColorRGBA& o) const noexcept {
        return !(*this == o);
    }

    ColorRGBA operator*(float s) const noexcept {
        return {
            static_cast<uint8_t>(std::clamp(r * s, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(g * s, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(b * s, 0.0f, 255.0f)),
            a
        };
    }
};

class Texture {
public:
    Texture(int width, int height, ColorRGBA fill_color = ColorRGBA::white())
        : width_(width), height_(height), pixels_(width * height, fill_color) {}

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] const std::vector<ColorRGBA>& pixels() const noexcept { return pixels_; }
    [[nodiscard]] size_t size_bytes() const noexcept { return pixels_.size() * sizeof(ColorRGBA); }

    void set_pixel(int x, int y, ColorRGBA col) {
        if (x >= 0 && x < width_ && y >= 0 && y < height_) {
            pixels_[y * width_ + x] = col;
        }
    }

    [[nodiscard]] ColorRGBA get_pixel(int x, int y) const noexcept {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) return ColorRGBA::black();
        return pixels_[y * width_ + x];
    }

    [[nodiscard]] ColorRGBA sample(float u, float v) const noexcept {
        // Nearest-neighbor with wrapping
        u = u - std::floor(u);
        v = v - std::floor(v);
        int px = static_cast<int>(u * (width_ - 1) + 0.5f);
        int py = static_cast<int>(v * (height_ - 1) + 0.5f);
        return get_pixel(px, py);
    }

    static Texture create_checkerboard(int w = 64, int h = 64, int block_size = 8,
                                       ColorRGBA col1 = ColorRGBA::white(),
                                       ColorRGBA col2 = ColorRGBA{128, 128, 128, 255}) {
        Texture tex(w, h);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                bool is_even = ((x / block_size) + (y / block_size)) % 2 == 0;
                tex.set_pixel(x, y, is_even ? col1 : col2);
            }
        }
        return tex;
    }

private:
    int width_{0};
    int height_{0};
    std::vector<ColorRGBA> pixels_;
};

} // namespace twrf::raster
