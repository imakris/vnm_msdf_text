# vnm_msdf_text

Small CPU-side MSDF text atlas builder shared by Varinomics plotting and editor
components.

The library builds a static C++ target:

```cmake
vnm_msdf_text::vnm_msdf_text
```

It uses FreeType and msdfgen. When configured as the top-level project, CMake
fetches those dependencies when compatible targets are not already available.
Parent projects should set `VNM_MSDF_TEXT_FETCH_DEPS` explicitly.

## Licensing

The source code is licensed under the BSD 2-Clause License. The bundled
`fonts/monospace.ttf` font is licensed separately under the Ubuntu Font Licence
1.0; see `THIRD_PARTY_NOTICES.md`.

## API contract

`build_font_atlas` builds a row-major linear RGBA8 MTSDF atlas from font bytes
and a requested set of Unicode scalar values. Requested codepoints are validated,
sorted, and deduplicated before glyph generation.

Build results use `Build_status`:

- `SUCCESS`: every valid requested codepoint was emitted.
- `PARTIAL_SUCCESS`: the atlas contains at least one emitted glyph, and the
  diagnostic vectors describe skipped codepoints.
- `FAILURE`: no usable atlas was produced. The returned atlas is
  default-constructed and must not be rendered.

Diagnostics are split into invalid Unicode scalar values, missing font coverage,
glyph load failures, glyphs too large for the atlas, and glyphs skipped after
the atlas ran out of space.

`options_t::missing_glyph_policy` controls missing requested codepoints:

- `SKIP`: report missing coverage and omit those codepoints from the atlas.
- `USE_REPLACEMENT_CHARACTER`: when the font contains U+FFFD, alias missing
  codepoints to that replacement glyph while still reporting the original
  missing codepoints.
- `FAIL_BUILD`: fail the build when any valid requested codepoint is missing.

`atlas_t::font_metrics` exposes ascender, descender, line height, and em size in
output pixels. A baseline-to-descender-bottom offset can be computed as
`-atlas.font_metrics.descender`. `zero_advance_px` is the advance of glyph `0`
when available; it is a reference advance, not proof that the font is monospace.

`default_codepoints()` is a UI-oriented scalar set covering printable ASCII,
selected Latin, Greek, currency, and UI symbol codepoints. It includes U+FFFD so
callers can request a replacement glyph for invalid UTF-8 fallback. The bundled
font is a test fixture and license-noticed convenience asset; it does not cover
every codepoint in this default set.

## Layout and rendering

`append_text_quads` treats `x, y` as the baseline origin in output pixels. The
layout convention is screen-style Y-down coordinates. Glyph plane coordinates
are relative to the baseline; for normal visible glyphs, `plane_bottom` is the
smaller Y value and `plane_top` is the larger Y value after converting the
font's Y-up outline space.

Atlas data is row-major RGBA8. Row 0 is the first row in memory, and the UV `t`
coordinate increases downward to match that layout. Upload the texture as
linear data, not sRGB.

`options_t::atlas_px_range` is the baked distance range in atlas pixels.
Internally, it is converted to shape units for msdfgen. `atlas_t::px_range` is
the output-pixel distance range intended for shader reconstruction after build
scaling and `sharpness_bias` are applied. `options_t::atlas_gutter_px` controls
empty pixels between packed glyph bitmaps. Shaders should still clamp sampling
to the glyph UV rectangle when using linear filtering.

Text layout is single-line, left-to-right codepoint layout with optional
kerning. It does not perform HarfBuzz shaping, bidirectional reordering,
ligature substitution, grapheme-cluster handling, or combining-mark placement.
`measure_text_advance_px` returns pen advance, not visual bounds. Invalid UTF-8
decodes to U+FFFD; missing glyphs are skipped during layout unless the atlas
contains a glyph entry for that decoded codepoint.

For custom renderers, `for_each_positioned_glyph` exposes the same decoded
layout stream used by `measure_text_advance_px`, `measure_text_bounds_px`, and
`append_text_quads`. `append_text_quads` can throw `std::length_error` if indexed
output would exceed `uint32_t` capacity; vector growth may throw allocation
exceptions.

## Building

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

To require system or pre-provided FreeType/msdfgen targets without network
fetching:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DVNM_MSDF_TEXT_FETCH_DEPS=OFF
cmake --build build
```

For offline builds with fetched dependencies already checked out, pass
`FETCHCONTENT_SOURCE_DIR_FREETYPE` and `FETCHCONTENT_SOURCE_DIR_MSDFGEN`, or set
`VNM_MSDF_TEXT_DEP_OVERRIDES_FILE` to a CMake file that defines those cache
entries.

## Tests

When this project is configured as the top-level CMake project, tests are built
by default:

```bash
cmake --build build --target vnm_msdf_text_tests
ctest --test-dir build --output-on-failure
```

Set `-DVNM_MSDF_TEXT_BUILD_TESTS=OFF` to skip the test executable.
