# vnm_msdf_text

Small CPU-side MSDF text atlas builder shared by Varinomics plotting and editor
components.

The library builds a static C++ target:

```cmake
vnm_msdf_text::vnm_msdf_text
```

It uses FreeType and msdfgen. By default, CMake fetches those dependencies when
compatible targets are not already available.

## Building

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

To prefer system or pre-provided FreeType/msdfgen targets:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DVNM_MSDF_TEXT_USE_SYSTEM_LIBS=ON
cmake --build build
```
