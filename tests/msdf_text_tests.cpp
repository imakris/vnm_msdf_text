#include <vnm_msdf_text/msdf_text.h>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace msdf = vnm::msdf_text;

namespace {

constexpr char32_t k_replacement_char = 0xFFFD;
constexpr char32_t k_invalid_surrogate = 0xD800;
constexpr char32_t k_invalid_too_large = 0x110000;

bool check(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

bool contains_codepoint(const std::vector<char32_t>& codepoints, char32_t codepoint)
{
    return std::find(codepoints.begin(), codepoints.end(), codepoint) != codepoints.end();
}

std::vector<std::uint8_t> read_test_font()
{
    std::ifstream file(VNM_MSDF_TEXT_TEST_FONT_FILE, std::ios::binary);
    if (!file) {
        throw std::runtime_error("failed to open test font");
    }

    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>());
}

msdf::options_t atlas_options(int atlas_size)
{
    msdf::options_t options;
    options.atlas_size          = atlas_size;
    options.min_atlas_font_size = 48.0;
    options.atlas_px_range      = 2.0f;
    options.sharpness_bias      = 2.5f;
    options.build_kerning_table = false;
    return options;
}

msdf::build_result_t build_test_atlas(
    const std::vector<std::uint8_t>& font_data,
    std::span<const char32_t> codepoints,
    int atlas_size = 256,
    msdf::Missing_glyph_policy missing_glyph_policy = msdf::Missing_glyph_policy::SKIP)
{
    msdf::options_t options = atlas_options(atlas_size);
    options.missing_glyph_policy = missing_glyph_policy;

    return msdf::build_font_atlas(
        font_data.data(),
        font_data.size(),
        32,
        codepoints,
        options);
}

char32_t missing_codepoint_for_font(const std::vector<std::uint8_t>& font_data)
{
    const std::vector<char32_t> candidates = {
        0x1F518,
        0x10FFFD,
        0xE000,
        0x2FA1D,
    };

    for (char32_t candidate : candidates) {
        const std::vector<char32_t> codepoints = {candidate};
        const msdf::build_result_t result = build_test_atlas(font_data, codepoints);
        if (contains_codepoint(result.missing_codepoints, candidate)) {
            return candidate;
        }
    }

    throw std::runtime_error("test font unexpectedly covers all missing-glyph candidates");
}

bool test_utf8_truncated_sequence()
{
    const std::string truncated = std::string("\xE2\x82", 2);
    const std::vector<char32_t> decoded = msdf::utf8_to_codepoints(truncated);

    if (!check(decoded.size() == 1, "truncated UTF-8 must decode to one replacement codepoint")) {
        return false;
    }
    return check(decoded[0] == k_replacement_char, "truncated UTF-8 must decode to U+FFFD");
}

bool test_default_codepoints_are_explicit_scalars()
{
    const std::vector<char32_t> codepoints = msdf::default_codepoints();

    bool ok = true;
    ok &= check(codepoints.size() == 291, "default codepoints must keep expected coverage count");
    ok &= check(
        std::is_sorted(codepoints.begin(), codepoints.end()),
        "default codepoints must be sorted");
    ok &= check(
        std::adjacent_find(codepoints.begin(), codepoints.end()) == codepoints.end(),
        "default codepoints must be unique");
    ok &= check(contains_codepoint(codepoints, U' '), "default codepoints must include space");
    ok &= check(contains_codepoint(codepoints, U'~'), "default codepoints must include ASCII");
    ok &= check(contains_codepoint(codepoints, 0x20AC), "default codepoints must include Euro sign");
    ok &= check(
        contains_codepoint(codepoints, k_replacement_char),
        "default codepoints must include U+FFFD replacement glyph");
    return ok;
}

bool test_invalid_and_duplicate_codepoints()
{
    const std::vector<std::uint8_t> font_data = read_test_font();
    const std::vector<char32_t> codepoints = {
        U'W',
        U'W',
        k_invalid_surrogate,
        k_invalid_too_large,
    };
    const msdf::build_result_t result = build_test_atlas(font_data, codepoints);

    bool ok = true;
    ok &= check(
        result.status == msdf::Build_status::PARTIAL_SUCCESS,
        "invalid codepoints with a usable glyph must produce partial success");
    ok &= check(result.atlas.glyphs.count(U'W') == 1, "duplicate codepoints must build one glyph");
    ok &= check(result.invalid_codepoints.size() == 2, "invalid codepoints must be reported once");
    ok &= check(
        contains_codepoint(result.invalid_codepoints, k_invalid_surrogate),
        "invalid surrogate must be reported");
    ok &= check(
        contains_codepoint(result.invalid_codepoints, k_invalid_too_large),
        "codepoints above Unicode range must be reported");
    return ok;
}

bool test_missing_glyph_failure()
{
    const std::vector<std::uint8_t> font_data = read_test_font();
    const char32_t missing_codepoint = missing_codepoint_for_font(font_data);
    const std::vector<char32_t> codepoints = {missing_codepoint};
    const msdf::build_result_t result = build_test_atlas(font_data, codepoints);

    bool ok = true;
    ok &= check(
        result.status == msdf::Build_status::FAILURE,
        "only missing glyphs must fail the atlas build");
    ok &= check(result.atlas.glyphs.empty(), "missing glyph build must not emit glyphs");
    ok &= check(result.atlas.rgba.empty(), "failure result must not retain atlas texture storage");
    ok &= check(result.atlas.atlas_size == 0, "failure result must expose a default atlas");
    ok &= check(
        contains_codepoint(result.missing_codepoints, missing_codepoint),
        "missing glyph codepoint must be reported");
    return ok;
}

bool test_too_small_atlas_failure()
{
    const std::vector<std::uint8_t> font_data = read_test_font();
    const std::vector<char32_t> codepoints = {U'W'};
    const msdf::build_result_t result = build_test_atlas(font_data, codepoints, 1);

    bool ok = true;
    ok &= check(
        result.status == msdf::Build_status::FAILURE,
        "atlas that cannot fit the requested glyph must fail");
    ok &= check(result.atlas.glyphs.empty(), "too-small atlas must not emit glyphs");
    ok &= check(result.atlas.rgba.empty(), "too-small atlas failure must clear texture storage");
    ok &= check(
        contains_codepoint(result.skipped_too_large, U'W'),
        "too-large glyph must be reported");
    return ok;
}

bool test_partial_success_keeps_usable_atlas()
{
    const std::vector<std::uint8_t> font_data = read_test_font();
    const char32_t missing_codepoint = missing_codepoint_for_font(font_data);
    const std::vector<char32_t> codepoints = {U'W', missing_codepoint};
    const msdf::build_result_t result = build_test_atlas(font_data, codepoints);

    bool ok = true;
    ok &= check(
        result.status == msdf::Build_status::PARTIAL_SUCCESS,
        "emitted glyph plus missing glyph must produce partial success");
    ok &= check(result.atlas.glyphs.count(U'W') == 1, "partial atlas must retain emitted glyphs");
    ok &= check(
        contains_codepoint(result.missing_codepoints, missing_codepoint),
        "partial atlas must report missing glyphs");
    ok &= check(!result.atlas.rgba.empty(), "partial atlas must retain texture storage");

    std::vector<msdf::text_vertex_t> vertices;
    std::vector<std::uint32_t> indices;
    msdf::append_text_quads(result.atlas, "W", 0.0f, 0.0f, vertices, &indices);
    ok &= check(vertices.size() == 4, "partial atlas must remain usable for emitted glyphs");
    ok &= check(indices.size() == 6, "partial atlas must emit indexed quads");
    return ok;
}

bool test_replacement_missing_glyph_policy()
{
    const std::vector<std::uint8_t> font_data = read_test_font();
    const char32_t missing_codepoint = missing_codepoint_for_font(font_data);
    const std::vector<char32_t> codepoints = {U'W', missing_codepoint};
    const msdf::build_result_t result = build_test_atlas(
        font_data,
        codepoints,
        256,
        msdf::Missing_glyph_policy::USE_REPLACEMENT_CHARACTER);

    bool ok = true;
    ok &= check(
        result.status == msdf::Build_status::PARTIAL_SUCCESS,
        "replacement policy must report partial success for missing coverage");
    ok &= check(result.atlas.glyphs.count(U'W') == 1, "replacement atlas must retain covered glyphs");
    ok &= check(
        result.atlas.glyphs.count(missing_codepoint) == 1,
        "replacement policy must alias missing codepoints when U+FFFD is available");
    ok &= check(
        contains_codepoint(result.missing_codepoints, missing_codepoint),
        "replacement policy must still report missing original codepoints");

    const std::vector<char32_t> missing_text = {missing_codepoint};
    std::vector<msdf::text_vertex_t> vertices;
    msdf::append_text_quads(
        result.atlas,
        msdf::codepoints_to_utf8(missing_text),
        0.0f,
        0.0f,
        vertices);
    ok &= check(vertices.size() == 4, "replacement glyph must emit a visible quad");
    return ok;
}

bool test_fail_missing_glyph_policy()
{
    const std::vector<std::uint8_t> font_data = read_test_font();
    const char32_t missing_codepoint = missing_codepoint_for_font(font_data);
    const std::vector<char32_t> codepoints = {U'W', missing_codepoint};
    const msdf::build_result_t result = build_test_atlas(
        font_data,
        codepoints,
        256,
        msdf::Missing_glyph_policy::FAIL_BUILD);

    bool ok = true;
    ok &= check(
        result.status == msdf::Build_status::FAILURE,
        "fail policy must reject missing requested glyphs");
    ok &= check(result.atlas.glyphs.empty(), "fail policy must not emit partial glyphs");
    ok &= check(result.atlas.rgba.empty(), "fail policy must not allocate atlas storage");
    ok &= check(
        contains_codepoint(result.missing_codepoints, missing_codepoint),
        "fail policy must report the missing codepoint");
    return ok;
}

bool test_atlas_generation_smoke()
{
    const std::vector<std::uint8_t> font_data = read_test_font();
    const std::vector<char32_t> codepoints = {U'W'};
    const msdf::build_result_t result = build_test_atlas(font_data, codepoints);

    bool ok = true;
    ok &= check(result.status == msdf::Build_status::SUCCESS, "single covered glyph must build");
    ok &= check(result.atlas.px_range > 0.0f, "atlas pixel range must be positive");
    ok &= check(result.atlas.font_metrics.ascender > 0.0f, "font ascender metric must be positive");
    ok &= check(result.atlas.font_metrics.descender < 0.0f, "font descender metric must be negative");
    ok &= check(result.atlas.font_metrics.line_height > 0.0f, "font line height metric must be positive");
    ok &= check(result.atlas.font_metrics.em_size > 0.0f, "font em size metric must be positive");
    ok &= check(!result.atlas.rgba.empty(), "atlas texture storage must exist");
    ok &= check(
        std::any_of(
            result.atlas.rgba.begin(),
            result.atlas.rgba.end(),
            [](std::uint8_t value) { return value != 0; }),
        "generated atlas must contain non-zero distance data");

    const auto glyph_it = result.atlas.glyphs.find(U'W');
    ok &= check(glyph_it != result.atlas.glyphs.end(), "atlas must contain W glyph");
    if (glyph_it == result.atlas.glyphs.end()) {
        return false;
    }

    const msdf::glyph_t& glyph = glyph_it->second;
    ok &= check(glyph.advance_x > 0.0f, "glyph advance must be positive");
    ok &= check(glyph.plane_left < glyph.plane_right, "glyph plane X range must be ordered");
    ok &= check(glyph.plane_bottom < glyph.plane_top, "glyph plane Y range must be ordered");
    ok &= check(glyph.uv_left < glyph.uv_right, "glyph UV S range must be ordered");
    ok &= check(glyph.uv_top < glyph.uv_bottom, "glyph UV T range must be ordered");
    return ok;
}

bool test_whitespace_skips_degenerate_quads()
{
    msdf::atlas_t atlas;

    msdf::glyph_t space;
    space.advance_x = 5.0f;
    atlas.glyphs.emplace(U' ', space);

    msdf::glyph_t glyph;
    glyph.advance_x    = 7.0f;
    glyph.plane_left   = 0.0f;
    glyph.plane_right  = 6.0f;
    glyph.plane_bottom = -8.0f;
    glyph.plane_top    = 2.0f;
    glyph.uv_left      = 0.1f;
    glyph.uv_right     = 0.2f;
    glyph.uv_top       = 0.3f;
    glyph.uv_bottom    = 0.4f;
    atlas.glyphs.emplace(U'A', glyph);

    std::vector<msdf::text_vertex_t> vertices;
    std::vector<std::uint32_t> indices;
    msdf::append_text_quads(atlas, " A ", 10.0f, 20.0f, vertices, &indices);

    int visited_glyphs = 0;
    float a_pen_x = 0.0f;
    const float final_pen_x = msdf::for_each_positioned_glyph(
        atlas,
        " A ",
        0.0f,
        [&](const msdf::positioned_glyph_t& positioned) {
            ++visited_glyphs;
            if (positioned.codepoint == U'A') {
                a_pen_x = positioned.pen_x;
            }
        });
    const msdf::text_bounds_t bounds = msdf::measure_text_bounds_px(atlas, " A ");

    bool ok = true;
    ok &= check(
        msdf::measure_text_advance_px(atlas, " A ") == 17.0f,
        "measurement must include spaces");
    ok &= check(final_pen_x == 17.0f, "layout callback must return final pen advance");
    ok &= check(visited_glyphs == 3, "layout callback must visit invisible advance glyphs");
    ok &= check(a_pen_x == 5.0f, "layout callback must position visible glyph after leading space");
    ok &= check(bounds.has_visible_glyphs, "visual bounds must report visible glyphs");
    ok &= check(bounds.advance_x == 17.0f, "visual bounds must include final advance");
    ok &= check(bounds.left == 5.0f, "visual bounds left edge must ignore leading space");
    ok &= check(bounds.right == 11.0f, "visual bounds right edge must include visible glyph");
    ok &= check(bounds.top == -8.0f, "visual bounds top must be the minimum glyph Y");
    ok &= check(bounds.bottom == 2.0f, "visual bounds bottom must be the maximum glyph Y");
    ok &= check(vertices.size() == 4, "only visible glyphs must emit vertices");
    ok &= check(indices.size() == 6, "only visible glyphs must emit indices");
    if (vertices.size() == 4) {
        ok &= check(vertices[0].x == 15.0f, "visible glyph quad must be positioned after leading space");
    }
    return ok;
}

bool run_test(const char* name, bool (*test)())
{
    try {
        if (test()) {
            std::cerr << "PASS: " << name << '\n';
            return true;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "FAIL: " << name << ": " << e.what() << '\n';
    }
    catch (...) {
        std::cerr << "FAIL: " << name << ": unknown exception\n";
    }

    return false;
}

} // namespace

int main()
{
    bool ok = true;
    ok &= run_test("utf8 truncated sequence", test_utf8_truncated_sequence);
    ok &= run_test("default codepoints are explicit scalars", test_default_codepoints_are_explicit_scalars);
    ok &= run_test("invalid and duplicate codepoints", test_invalid_and_duplicate_codepoints);
    ok &= run_test("missing glyph failure", test_missing_glyph_failure);
    ok &= run_test("too-small atlas failure", test_too_small_atlas_failure);
    ok &= run_test("partial success keeps usable atlas", test_partial_success_keeps_usable_atlas);
    ok &= run_test("replacement missing glyph policy", test_replacement_missing_glyph_policy);
    ok &= run_test("fail missing glyph policy", test_fail_missing_glyph_policy);
    ok &= run_test("atlas generation smoke", test_atlas_generation_smoke);
    ok &= run_test("whitespace skips degenerate quads", test_whitespace_skips_degenerate_quads);
    return ok ? 0 : 1;
}
