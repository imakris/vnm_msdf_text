#include <vnm_msdf_text/msdf_text.h>

#include <msdfgen.h>
#include <msdfgen-ext.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace vnm {
namespace msdf_text {
namespace {

constexpr char32_t k_replacement_char = 0xFFFD;

bool is_continuation(unsigned char c)
{
    return (c & 0xC0) == 0x80;
}

char32_t utf8_decode_one(const char*& it, const char* end)
{
    if (it >= end) {
        return k_replacement_char;
    }

    const auto* p = reinterpret_cast<const unsigned char*>(it);
    const unsigned char c0 = *p++;

    if ((c0 & 0x80) == 0) {
        it = reinterpret_cast<const char*>(p);
        return static_cast<char32_t>(c0);
    }

    std::size_t sequence_length = 0;
    char32_t codepoint = 0;
    if ((c0 & 0xE0) == 0xC0) {
        sequence_length = 2;
        codepoint = c0 & 0x1F;
    }
    else
    if ((c0 & 0xF0) == 0xE0) {
        sequence_length = 3;
        codepoint = c0 & 0x0F;
    }
    else
    if ((c0 & 0xF8) == 0xF0) {
        sequence_length = 4;
        codepoint = c0 & 0x07;
    }
    else {
        it = reinterpret_cast<const char*>(p);
        return k_replacement_char;
    }

    const auto* sequence_end = reinterpret_cast<const unsigned char*>(end);
    if (p + sequence_length - 1 > sequence_end) {
        it = end;
        return k_replacement_char;
    }

    for (std::size_t i = 1; i < sequence_length; ++i) {
        if (!is_continuation(*p)) {
            it = reinterpret_cast<const char*>(p);
            return k_replacement_char;
        }
        codepoint = (codepoint << 6) | (*p++ & 0x3F);
    }

    it = reinterpret_cast<const char*>(p);
    if ((sequence_length == 2 && codepoint < 0x80) ||
        (sequence_length == 3 && codepoint < 0x800) ||
        (sequence_length == 4 && codepoint < 0x10000) ||
        codepoint > 0x10FFFF ||
        (codepoint >= 0xD800 && codepoint <= 0xDFFF))
    {
        return k_replacement_char;
    }

    return codepoint;
}

std::size_t codepoint_to_utf8(char32_t codepoint, char* out)
{
    auto* p = reinterpret_cast<unsigned char*>(out);

    if (codepoint < 0x80) {
        p[0] = static_cast<unsigned char>(codepoint);
        return 1;
    }
    if (codepoint < 0x800) {
        p[0] = static_cast<unsigned char>(0xC0 | (codepoint >> 6));
        p[1] = static_cast<unsigned char>(0x80 | (codepoint & 0x3F));
        return 2;
    }
    if (codepoint < 0x10000) {
        if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
            return 0;
        }
        p[0] = static_cast<unsigned char>(0xE0 | (codepoint >> 12));
        p[1] = static_cast<unsigned char>(0x80 | ((codepoint >> 6) & 0x3F));
        p[2] = static_cast<unsigned char>(0x80 | (codepoint & 0x3F));
        return 3;
    }
    if (codepoint <= 0x10FFFF) {
        p[0] = static_cast<unsigned char>(0xF0 | (codepoint >> 18));
        p[1] = static_cast<unsigned char>(0x80 | ((codepoint >> 12) & 0x3F));
        p[2] = static_cast<unsigned char>(0x80 | ((codepoint >> 6) & 0x3F));
        p[3] = static_cast<unsigned char>(0x80 | (codepoint & 0x3F));
        return 4;
    }

    return 0;
}

void append_utf8(std::vector<char32_t>& chars, std::string_view utf8)
{
    const auto decoded = utf8_to_codepoints(utf8);
    chars.insert(chars.end(), decoded.begin(), decoded.end());
}

build_result_t failure(const std::string& message)
{
    build_result_t result;
    result.ok = false;
    result.message = message;
    return result;
}

template <class Visitor>
float for_each_positioned_glyph(
    const atlas_t& atlas,
    std::string_view text,
    float start_x,
    Visitor&& visit)
{
    float pen_x = start_x;
    char32_t previous = 0;
    const auto codepoints = utf8_to_codepoints(text);
    for (char32_t codepoint : codepoints) {
        const auto glyph_it = atlas.glyphs.find(codepoint);
        if (glyph_it == atlas.glyphs.end()) {
            continue;
        }
        if (previous != 0) {
            const auto kerning_it = atlas.kerning_px.find(kerning_key_t{previous, codepoint});
            if (kerning_it != atlas.kerning_px.end()) {
                pen_x += kerning_it->second;
            }
        }
        visit(pen_x, glyph_it->second);
        pen_x += glyph_it->second.advance_x;
        previous = codepoint;
    }
    return pen_x;
}

} // namespace

std::vector<char32_t> default_codepoints()
{
    static const std::vector<char32_t> codepoints = [] {
        static const char* ascii_printable =
            " 0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

        static const char* latin_accented =
            "\xC3\x80\xC3\x81\xC3\x82\xC3\x83\xC3\x84\xC3\x85\xC3\x86\xC3\x87\xC3\x88\xC3\x89\xC3\x8A"
            "\xC3\x8B\xC3\x8C\xC3\x8D\xC3\x8E\xC3\x8F\xC3\x90\xC3\x91\xC3\x92\xC3\x93\xC3\x94\xC3\x95"
            "\xC3\x96\xC3\x98\xC3\x99\xC3\x9A\xC3\x9B\xC3\x9C\xC3\x9D\xC3\x9E\xC3\x9F\xC3\xA0\xC3\xA1"
            "\xC3\xA2\xC3\xA3\xC3\xA4\xC3\xA5\xC3\xA6\xC3\xA7\xC3\xA8\xC3\xA9\xC3\xAA\xC3\xAB\xC3\xAC"
            "\xC3\xAD\xC3\xAE\xC3\xAF\xC3\xB0\xC3\xB1\xC3\xB2\xC3\xB3\xC3\xB4\xC3\xB5\xC3\xB6\xC3\xB8"
            "\xC3\xB9\xC3\xBA\xC3\xBB\xC3\xBC\xC3\xBD\xC3\xBE\xC3\xBF\xC5\x92\xC5\x93\xC5\xA0\xC5\xA1"
            "\xC5\xB8\xC6\x92";

        static const char* greek =
            "\xCE\x91\xCE\x92\xCE\x93\xCE\x94\xCE\x95\xCE\x96\xCE\x97\xCE\x98\xCE\x99\xCE\x9A\xCE\x9B"
            "\xCE\x9C\xCE\x9D\xCE\x9E\xCE\x9F\xCE\xA0\xCE\xA1\xCE\xA3\xCE\xA4\xCE\xA5\xCE\xA6\xCE\xA7"
            "\xCE\xA8\xCE\xA9\xCE\xB1\xCE\xB2\xCE\xB3\xCE\xB4\xCE\xB5\xCE\xB6\xCE\xB7\xCE\xB8\xCE\xB9"
            "\xCE\xBA\xCE\xBB\xCE\xBC\xCE\xBD\xCE\xBE\xCE\xBF\xCF\x80\xCF\x81\xCF\x83\xCF\x84\xCF\x85"
            "\xCF\x86\xCF\x87\xCF\x88\xCF\x89\xCE\x86\xCE\x88\xCE\x89\xCE\x8A\xCE\x8C\xCE\x8E\xCE\x8F"
            "\xCE\xAC\xCE\xAD\xCE\xAE\xCE\xAF\xCF\x8C\xCF\x8D\xCF\x8E\xCF\x8A\xCF\x8B\xCE\x90\xCE\xB0"
            "\xCE\xAA\xCE\xAB\xCF\x82\xC2\xAB\xC2\xBB\xCE\x87";

        static const char* currency_popular =
            "\xE2\x82\xAC\xC2\xA2\xC2\xA3\xC2\xA4\xC2\xA5\xE0\xB8\xBF\xE2\x82\xBD\xE2\x82\xB9\xE2"
            "\x82\xA9";

        static const char* currency_all =
            "\xE2\x82\xB5\xD8\x8B\xE0\xA7\xB2\xE0\xA7\xB3\xE0\xA7\xBB\xE0\xAB\xB1\xE0\xAF\xB9\xE1\x9F"
            "\x9B\xE2\x82\xA0\xE2\x82\xA1\xE2\x82\xA2\xE2\x82\xA3\xE2\x82\xA4\xE2\x82\xA5\xE2\x82\xA6"
            "\xE2\x82\xA7\xE2\x82\xA8\xE2\x82\xAA\xE2\x82\xAB\xE2\x82\xAD\xE2\x82\xAE\xE2\x82\xAF\xE2"
            "\x82\xB0\xE2\x82\xB1\xE2\x82\xB2\xE2\x82\xB3\xE2\x82\xB4\xE2\x82\xB8\xE2\x82\xBA\xE2\x82"
            "\xBC\xE2\x82\xBE\xEF\xB7\xBC\xEF\xB9\xA9\xEF\xBC\x84\xEF\xBF\xA0\xEF\xBF\xA1\xEF\xBF\xA5"
            "\xEF\xBF\xA6";

        static const char* ui_symbols =
            "\xE2\x98\x90"
            "\xE2\x98\x91"
            "\xE2\x98\x92"
            "\xF0\x9F\x94\x98"
            "\xF0\x9F\x97\x95"
            "\xF0\x9F\x97\x96"
            "\xF0\x9F\x97\x97"
            "\xE2\x9C\x95";

        std::vector<char32_t> chars;
        append_utf8(chars, ascii_printable);
        append_utf8(chars, latin_accented);
        append_utf8(chars, greek);
        append_utf8(chars, currency_popular);
        append_utf8(chars, currency_all);
        append_utf8(chars, ui_symbols);
        std::sort(chars.begin(), chars.end());
        chars.erase(std::unique(chars.begin(), chars.end()), chars.end());
        return chars;
    }();
    return codepoints;
}

std::vector<char32_t> utf8_to_codepoints(std::string_view text)
{
    std::vector<char32_t> result;
    if (text.empty()) {
        return result;
    }
    result.reserve(text.size());
    const char* it = text.data();
    const char* end = it + text.size();
    while (it < end) {
        result.push_back(utf8_decode_one(it, end));
    }
    return result;
}

std::string codepoints_to_utf8(const std::vector<char32_t>& codepoints)
{
    std::string result;
    result.reserve(codepoints.size() * 2);

    char buffer[4];
    for (char32_t codepoint : codepoints) {
        const std::size_t length = codepoint_to_utf8(codepoint, buffer);
        if (length > 0) {
            result.append(buffer, length);
        }
    }
    return result;
}

build_result_t build_font_atlas(
    const std::uint8_t* font_data,
    std::size_t font_size,
    int pixel_height,
    std::span<const char32_t> codepoints,
    const options_t& options,
    const log_callback_t& log_debug)
{
    if (!font_data || font_size == 0) {
        return failure("No font data supplied");
    }
    if (pixel_height <= 0) {
        return failure("Font pixel height must be positive");
    }
    if (options.atlas_size <= 0) {
        return failure("MSDF atlas size must be positive");
    }

    msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
    if (!ft) {
        return failure("Failed to initialize FreeType for msdfgen");
    }

    msdfgen::FontHandle* font_handle = msdfgen::loadFontData(
        ft,
        reinterpret_cast<const msdfgen::byte*>(font_data),
        static_cast<int>(font_size));
    if (!font_handle) {
        msdfgen::deinitializeFreetype(ft);
        return failure("Failed to load font data for msdfgen");
    }

    msdfgen::FontMetrics metrics{};
    if (!msdfgen::getFontMetrics(metrics, font_handle)) {
        msdfgen::destroyFont(font_handle);
        msdfgen::deinitializeFreetype(ft);
        return failure("Failed to query font metrics for msdfgen");
    }

    build_result_t result;
    result.ok = true;
    atlas_t& atlas = result.atlas;
    atlas.pixel_height = pixel_height;
    atlas.atlas_size = options.atlas_size;

    const double bitmap_scale =
        std::max(static_cast<double>(pixel_height), options.min_atlas_font_size) /
        metrics.ascenderY;
    const double draw_scale = static_cast<double>(pixel_height) / metrics.ascenderY;
    const double screen_to_atlas_ratio = draw_scale / bitmap_scale;

    atlas.px_range =
        (options.atlas_px_range * static_cast<float>(screen_to_atlas_ratio)) *
        options.sharpness_bias;
    atlas.baseline_offset_px = static_cast<float>(-metrics.descenderY * draw_scale);
    atlas.rgba.assign(
        static_cast<std::size_t>(options.atlas_size) *
        static_cast<std::size_t>(options.atlas_size) * 4,
        0);

    int pen_x = 0;
    int pen_y = 0;
    int row_h = 0;

    for (char32_t codepoint : codepoints) {
        msdfgen::Shape shape;
        double advance = 0.0;
        if (!msdfgen::loadGlyph(
                shape,
                font_handle,
                static_cast<msdfgen::unicode_t>(codepoint),
                &advance))
        {
            continue;
        }
        msdfgen::edgeColoringSimple(shape, 3.0, 0);

        const auto bounds = shape.getBounds();
        const double width_em = bounds.r - bounds.l;
        const double height_em = bounds.t - bounds.b;

        if (height_em <= 0.0 || width_em <= 0.0) {
            if (advance > 0.0) {
                glyph_t glyph;
                glyph.advance_x = static_cast<float>(advance * draw_scale);
                atlas.glyphs.emplace(codepoint, glyph);
            }
            continue;
        }

        const int bitmap_w = static_cast<int>(
            std::ceil(width_em * bitmap_scale + options.atlas_px_range * 2.0f));
        const int bitmap_h = static_cast<int>(
            std::ceil(height_em * bitmap_scale + options.atlas_px_range * 2.0f));
        if (bitmap_w <= 0 ||
            bitmap_h <= 0 ||
            bitmap_w > options.atlas_size ||
            bitmap_h > options.atlas_size)
        {
            continue;
        }

        if (pen_x + bitmap_w > options.atlas_size) {
            pen_x = 0;
            pen_y += row_h + 1;
            row_h = 0;
        }
        if (pen_y + bitmap_h > options.atlas_size) {
            if (log_debug) {
                log_debug("MSDF atlas out of space, skipping remaining glyphs");
            }
            break;
        }

        msdfgen::Bitmap<float, 4> bitmap(bitmap_w, bitmap_h);
        const msdfgen::Vector2 msdf_scale(bitmap_scale, bitmap_scale);
        const msdfgen::Vector2 msdf_translate(
            -bounds.l + (options.atlas_px_range / bitmap_scale),
            -bounds.b + (options.atlas_px_range / bitmap_scale));
        const msdfgen::Projection projection(msdf_scale, msdf_translate);
        msdfgen::generateMTSDF(bitmap, shape, projection, options.atlas_px_range);

        for (int y = 0; y < bitmap_h; ++y) {
            for (int x = 0; x < bitmap_w; ++x) {
                const auto& pixel = bitmap(x, y);
                const int dst_idx = ((pen_y + y) * options.atlas_size + (pen_x + x)) * 4;
                for (int c = 0; c < 4; ++c) {
                    const float normalized = std::max(0.0f, std::min(pixel[c], 1.0f));
                    atlas.rgba[static_cast<std::size_t>(dst_idx + c)] =
                        static_cast<std::uint8_t>(std::round(normalized * 255.0f));
                }
            }
        }

        glyph_t glyph;
        glyph.advance_x = static_cast<float>(advance * draw_scale);
        glyph.plane_left =
            static_cast<float>( bounds.l * draw_scale - options.atlas_px_range * screen_to_atlas_ratio);
        glyph.plane_right =
            static_cast<float>( bounds.r * draw_scale + options.atlas_px_range * screen_to_atlas_ratio);
        glyph.plane_top =
            static_cast<float>(-bounds.b * draw_scale + options.atlas_px_range * screen_to_atlas_ratio);
        glyph.plane_bottom =
            static_cast<float>(-bounds.t * draw_scale - options.atlas_px_range * screen_to_atlas_ratio);

        const double uv_width = width_em * bitmap_scale + 2.0 * options.atlas_px_range;
        const double uv_height = height_em * bitmap_scale + 2.0 * options.atlas_px_range;

        glyph.uv_left   = static_cast<float>(pen_x)            / options.atlas_size;
        glyph.uv_right  = static_cast<float>(pen_x + uv_width) / options.atlas_size;
        glyph.uv_top    = static_cast<float>(pen_y)             / options.atlas_size;
        glyph.uv_bottom = static_cast<float>(pen_y + uv_height) / options.atlas_size;

        atlas.glyphs.emplace(codepoint, glyph);

        if (codepoint == static_cast<char32_t>('0')) {
            atlas.monospace_advance_px = glyph.advance_x;
            atlas.monospace_advance_reliable = glyph.advance_x > 0.0f;
        }

        row_h = std::max(row_h, bitmap_h);
        pen_x += bitmap_w + 1;
    }

    if (options.build_kerning_table) {
        for (char32_t left : codepoints) {
            for (char32_t right : codepoints) {
                double k = 0.0;
                if (msdfgen::getKerning(
                        k,
                        font_handle,
                        static_cast<msdfgen::unicode_t>(left),
                        static_cast<msdfgen::unicode_t>(right)))
                {
                    const float kern_px = static_cast<float>(k * draw_scale);
                    if (kern_px != 0.0f) {
                        atlas.kerning_px.emplace(kerning_key_t{left, right}, kern_px);
                    }
                }
            }
        }
    }

    msdfgen::destroyFont(font_handle);
    msdfgen::deinitializeFreetype(ft);
    return result;
}

float measure_text_px(const atlas_t& atlas, std::string_view text)
{
    return for_each_positioned_glyph(
        atlas, text, 0.0f,
        [](float, const glyph_t&) {});
}

void append_text_quads(
    const atlas_t& atlas,
    std::string_view text,
    float x,
    float y,
    std::vector<text_vertex_t>& vertices,
    std::vector<std::uint32_t>* indices)
{
    for_each_positioned_glyph(
        atlas, text, x,
        [&](float pen_x, const glyph_t& glyph) {
            const float x0 = pen_x + glyph.plane_left;
            const float x1 = pen_x + glyph.plane_right;
            const float y0 = y + glyph.plane_bottom;
            const float y1 = y + glyph.plane_top;
            const float s_min = std::min(glyph.uv_left, glyph.uv_right);
            const float s_max = std::max(glyph.uv_left, glyph.uv_right);
            const float t_min = std::min(glyph.uv_top, glyph.uv_bottom);
            const float t_max = std::max(glyph.uv_top, glyph.uv_bottom);

            const std::uint32_t base =
                static_cast<std::uint32_t>(vertices.size());
            vertices.push_back({x0, y0, glyph.uv_left,  glyph.uv_bottom, s_min, t_min, s_max, t_max});
            vertices.push_back({x0, y1, glyph.uv_left,  glyph.uv_top,    s_min, t_min, s_max, t_max});
            vertices.push_back({x1, y1, glyph.uv_right, glyph.uv_top,    s_min, t_min, s_max, t_max});
            vertices.push_back({x1, y0, glyph.uv_right, glyph.uv_bottom, s_min, t_min, s_max, t_max});

            if (indices) {
                indices->push_back(base);
                indices->push_back(base + 1);
                indices->push_back(base + 2);
                indices->push_back(base);
                indices->push_back(base + 2);
                indices->push_back(base + 3);
            }
        });
}

} // namespace msdf_text
} // namespace vnm
