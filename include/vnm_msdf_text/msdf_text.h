#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace vnm {
namespace msdf_text {

struct glyph_t
{
    float advance_x    = 0.0f;
    float plane_left   = 0.0f;
    float plane_bottom = 0.0f;
    float plane_right  = 0.0f;
    float plane_top    = 0.0f;
    float uv_left      = 0.0f;
    float uv_bottom    = 0.0f;
    float uv_right     = 0.0f;
    float uv_top       = 0.0f;
};

struct kerning_key_t
{
    char32_t left  = 0;
    char32_t right = 0;

    bool operator==(const kerning_key_t&) const = default;
};

} // namespace msdf_text
} // namespace vnm

template <>
struct std::hash<vnm::msdf_text::kerning_key_t>
{
    std::size_t operator()(const vnm::msdf_text::kerning_key_t& key) const noexcept
    {
        const std::size_t left  = static_cast<std::size_t>(key.left);
        const std::size_t right = static_cast<std::size_t>(key.right);
        return left ^ (right + 0x9E3779B9u + (left << 6) + (left >> 2));
    }
};

namespace vnm {
namespace msdf_text {

using glyph_map_t   = std::unordered_map<char32_t, glyph_t>;
using kerning_map_t = std::unordered_map<kerning_key_t, float>;

struct atlas_t
{
    int pixel_height = 0;
    int atlas_size   = 0;
    std::vector<std::uint8_t> rgba;
    glyph_map_t glyphs;
    kerning_map_t kerning_px;
    float px_range = 0.0f;
    float baseline_offset_px = 0.0f;
    float monospace_advance_px = 0.0f;
    bool monospace_advance_reliable = false;
};

struct options_t
{
    int atlas_size = 2048;
    double min_atlas_font_size = 48.0;
    float atlas_px_range = 2.0f;
    float sharpness_bias = 2.5f;
    bool build_kerning_table = true;
};

struct build_result_t
{
    bool ok = false;
    std::string message;
    atlas_t atlas;
};

struct text_vertex_t
{
    float x;
    float y;
    float s;
    float t;
    float s_min;
    float t_min;
    float s_max;
    float t_max;
};

using log_callback_t = std::function<void(const std::string&)>;

std::vector<char32_t> default_codepoints();

std::vector<char32_t> utf8_to_codepoints(std::string_view text);
std::string codepoints_to_utf8(const std::vector<char32_t>& codepoints);

build_result_t build_font_atlas(
    const std::uint8_t* font_data,
    std::size_t font_size,
    int pixel_height,
    std::span<const char32_t> codepoints,
    const options_t& options = options_t(),
    const log_callback_t& log_debug = log_callback_t());

float measure_text_px(const atlas_t& atlas, std::string_view text);

void append_text_quads(
    const atlas_t& atlas,
    std::string_view text,
    float x,
    float y,
    std::vector<text_vertex_t>& vertices,
    std::vector<std::uint32_t>* indices = nullptr);

} // namespace msdf_text
} // namespace vnm
