get_filename_component(_vnm_msdf_text_workspace_root "${CMAKE_CURRENT_LIST_DIR}/../../../varinomics/fetched_libs" ABSOLUTE)

set(_vnm_msdf_text_freetype_source_dir "${_vnm_msdf_text_workspace_root}/freetype-src")
if(EXISTS "${_vnm_msdf_text_freetype_source_dir}/CMakeLists.txt")
  set(FETCHCONTENT_SOURCE_DIR_FREETYPE "${_vnm_msdf_text_freetype_source_dir}" CACHE PATH "" FORCE)
  set(FETCHCONTENT_SOURCE_DIR_freetype "${_vnm_msdf_text_freetype_source_dir}" CACHE PATH "" FORCE)
endif()

set(_vnm_msdf_text_msdfgen_source_dir "${_vnm_msdf_text_workspace_root}/msdfgen-src")
if(EXISTS "${_vnm_msdf_text_msdfgen_source_dir}/CMakeLists.txt")
  set(FETCHCONTENT_SOURCE_DIR_MSDFGEN "${_vnm_msdf_text_msdfgen_source_dir}" CACHE PATH "" FORCE)
  set(FETCHCONTENT_SOURCE_DIR_msdfgen "${_vnm_msdf_text_msdfgen_source_dir}" CACHE PATH "" FORCE)
endif()

unset(_vnm_msdf_text_freetype_source_dir)
unset(_vnm_msdf_text_msdfgen_source_dir)
unset(_vnm_msdf_text_workspace_root)
