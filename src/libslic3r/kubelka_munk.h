/*
 * Kubelka-Munk pigment colour mixing — header-only
 *
 * Models physical subtractive mixing via the Saunderson equation applied
 * per linearised sRGB channel:
 *
 *   K/S = (1 - R)² / (2·R)          (reflectance → absorption/scatter ratio)
 *   (K/S)_mix = Σ cᵢ·(K/S)ᵢ         (linear combination by concentration)
 *   R_mix = 1 + K/S - √(K/S² + 2·K/S)   (back to reflectance)
 *
 * Blue + Yellow at t=0.5 yields green, not grey — the hallmark of correct
 * subtractive mixing vs. RGB lerp.
 *
 * No external dependencies beyond the C++ standard library.
 */

#ifndef SLIC3R_KUBELKA_MUNK_H
#define SLIC3R_KUBELKA_MUNK_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <utility>

namespace kubelka_munk {
namespace detail {

inline float srgb_to_linear(float v)
{
    return (v <= 0.04045f) ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
}

inline float linear_to_srgb(float v)
{
    v = std::max(0.0f, std::min(1.0f, v));
    return (v <= 0.0031308f) ? 12.92f * v : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}

// Reflectance → K/S.  Clamped away from 0 and 1 to avoid singularities.
inline float r_to_ks(float r)
{
    r = std::max(1e-4f, std::min(1.0f - 1e-4f, r));
    const float one_minus_r = 1.0f - r;
    return (one_minus_r * one_minus_r) / (2.0f * r);
}

// K/S → Reflectance (always in [0, 1]).
inline float ks_to_r(float ks)
{
    ks = std::max(0.0f, ks);
    return 1.0f + ks - std::sqrt(ks * ks + 2.0f * ks);
}

inline unsigned char to_u8(float v)
{
    return static_cast<unsigned char>(std::max(0.0f, std::min(1.0f, v)) * 255.0f + 0.5f);
}

} // namespace detail

// Mix two sRGB colours (0–255) at ratio t  (0 = colour1, 1 = colour2).
inline void lerp(unsigned char r1, unsigned char g1, unsigned char b1,
                 unsigned char r2, unsigned char g2, unsigned char b2,
                 float t,
                 unsigned char *out_r, unsigned char *out_g, unsigned char *out_b)
{
    using namespace detail;
    const float c2 = std::max(0.0f, std::min(1.0f, t));
    const float c1 = 1.0f - c2;

    const float lin1[3] = {
        srgb_to_linear(r1 / 255.0f),
        srgb_to_linear(g1 / 255.0f),
        srgb_to_linear(b1 / 255.0f),
    };
    const float lin2[3] = {
        srgb_to_linear(r2 / 255.0f),
        srgb_to_linear(g2 / 255.0f),
        srgb_to_linear(b2 / 255.0f),
    };

    const float r_mix = ks_to_r(c1 * r_to_ks(lin1[0]) + c2 * r_to_ks(lin2[0]));
    const float g_mix = ks_to_r(c1 * r_to_ks(lin1[1]) + c2 * r_to_ks(lin2[1]));
    const float b_mix = ks_to_r(c1 * r_to_ks(lin1[2]) + c2 * r_to_ks(lin2[2]));

    *out_r = to_u8(linear_to_srgb(r_mix));
    *out_g = to_u8(linear_to_srgb(g_mix));
    *out_b = to_u8(linear_to_srgb(b_mix));
}

struct WeightedRGB {
    unsigned char r, g, b;
    float weight;
};

// Mix N colours in a single pass — avoids accumulation error from sequential
// pairwise blending.  Weights need not sum to any specific value; they are
// normalised internally.
inline void lerp_multi(const std::vector<WeightedRGB> &colors,
                       unsigned char *out_r, unsigned char *out_g, unsigned char *out_b)
{
    using namespace detail;

    float total_w  = 0.0f;
    float ks[3]    = {0.0f, 0.0f, 0.0f};

    for (const auto &c : colors) {
        if (c.weight <= 0.0f)
            continue;
        total_w += c.weight;
        ks[0] += c.weight * r_to_ks(srgb_to_linear(c.r / 255.0f));
        ks[1] += c.weight * r_to_ks(srgb_to_linear(c.g / 255.0f));
        ks[2] += c.weight * r_to_ks(srgb_to_linear(c.b / 255.0f));
    }

    if (total_w <= 0.0f) {
        *out_r = *out_g = *out_b = 0;
        return;
    }

    *out_r = to_u8(linear_to_srgb(ks_to_r(ks[0] / total_w)));
    *out_g = to_u8(linear_to_srgb(ks_to_r(ks[1] / total_w)));
    *out_b = to_u8(linear_to_srgb(ks_to_r(ks[2] / total_w)));
}

} // namespace kubelka_munk

#endif // SLIC3R_KUBELKA_MUNK_H
