#pragma once

#include <vector>
#include <cmath>
#include <cstdint>
#include <cassert>
#include <algorithm>

namespace twrf::neural {

struct Tensor1D {
    std::vector<float> data;

    Tensor1D() = default;
    explicit Tensor1D(size_t size, float init_val = 0.0f) : data(size, init_val) {}
    Tensor1D(std::initializer_list<float> list) : data(list) {}

    [[nodiscard]] size_t size() const noexcept { return data.size(); }
    [[nodiscard]] float& operator[](size_t i) noexcept { return data[i]; }
    [[nodiscard]] const float& operator[](size_t i) const noexcept { return data[i]; }

    [[nodiscard]] float dot(const Tensor1D& o) const noexcept {
        assert(data.size() == o.data.size());
        float sum = 0.0f;
        for (size_t i = 0; i < data.size(); ++i) {
            sum += data[i] * o.data[i];
        }
        return sum;
    }

    Tensor1D operator+(const Tensor1D& o) const {
        assert(data.size() == o.data.size());
        Tensor1D res(data.size());
        for (size_t i = 0; i < data.size(); ++i) {
            res[i] = data[i] + o.data[i];
        }
        return res;
    }

    void relu_inplace() noexcept {
        for (auto& v : data) {
            v = std::max(0.0f, v);
        }
    }

    void gelu_inplace() noexcept {
        constexpr float SQRT_2_OVER_PI = 0.7978845608f;
        for (auto& x : data) {
            x = 0.5f * x * (1.0f + std::tanh(SQRT_2_OVER_PI * (x + 0.044715f * x * x * x)));
        }
    }
};

struct Tensor2D {
    size_t rows{0};
    size_t cols{0};
    std::vector<float> data;

    Tensor2D() = default;
    Tensor2D(size_t r, size_t c, float init_val = 0.0f)
        : rows(r), cols(c), data(r * c, init_val) {}

    [[nodiscard]] float& at(size_t r, size_t c) noexcept {
        return data[r * cols + c];
    }

    [[nodiscard]] const float& at(size_t r, size_t c) const noexcept {
        return data[r * cols + c];
    }

    // Deterministic matrix-vector multiplication: y = W * x
    [[nodiscard]] Tensor1D mat_vec(const Tensor1D& x) const {
        assert(x.size() == cols);
        Tensor1D y(rows, 0.0f);
        for (size_t r = 0; r < rows; ++r) {
            float sum = 0.0f;
            size_t row_offset = r * cols;
            for (size_t c = 0; c < cols; ++c) {
                sum += data[row_offset + c] * x[c];
            }
            y[r] = sum;
        }
        return y;
    }
};

} // namespace twrf::neural
