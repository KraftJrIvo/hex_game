cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20 CACHE STRING "The C++ standard to use")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if (WIN32)
  set(CMAKE_CXX_FLAGS "/utf-8")
  set(CMAKE_CXX_FLAGS "/EHsc")
  set(CMAKE_C_FLAGS "/utf-8")
  set(CMAKE_C_FLAGS "/EHsc")
else() 
  set(CMAKE_CXX_FLAGS "-fPIC")
endif()
add_compile_definitions(_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
add_compile_definitions(_SILENCE_ALL_CXX20_DEPRECATION_WARNINGS)
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

project(HEX_GAME VERSION 0.0.1 DESCRIPTION "raylib game base" LANGUAGES CXX C)

add_executable(embedfile "src/util/embedfile.c")
add_custom_command(OUTPUT res_tiles.c COMMAND embedfile res_tiles ${HEX_GAME_SOURCE_DIR}/res/tiles.png DEPENDS ${HEX_GAME_SOURCE_DIR}/res/tiles.png)
add_custom_command(OUTPUT res_explosion.c COMMAND embedfile res_explosion ${HEX_GAME_SOURCE_DIR}/res/explosion.png DEPENDS ${HEX_GAME_SOURCE_DIR}/res/explosion.png)
add_custom_command(OUTPUT res_splash.c COMMAND embedfile res_splash ${HEX_GAME_SOURCE_DIR}/res/splash.png DEPENDS ${HEX_GAME_SOURCE_DIR}/res/splash.png)
add_custom_command(OUTPUT res_font.c COMMAND embedfile res_font ${HEX_GAME_SOURCE_DIR}/res/cozette.otf DEPENDS ${HEX_GAME_SOURCE_DIR}/res/cozette.otf)
add_custom_command(OUTPUT res_music.c COMMAND embedfile res_music ${HEX_GAME_SOURCE_DIR}/res/music.ogg DEPENDS ${HEX_GAME_SOURCE_DIR}/res/music.ogg)
add_custom_command(OUTPUT res_clang0.c COMMAND embedfile res_clang0 ${HEX_GAME_SOURCE_DIR}/res/clang0.ogg DEPENDS ${HEX_GAME_SOURCE_DIR}/res/clang0.ogg)
add_custom_command(OUTPUT res_clang1.c COMMAND embedfile res_clang1 ${HEX_GAME_SOURCE_DIR}/res/clang1.ogg DEPENDS ${HEX_GAME_SOURCE_DIR}/res/clang1.ogg)
add_custom_command(OUTPUT res_clang2.c COMMAND embedfile res_clang2 ${HEX_GAME_SOURCE_DIR}/res/clang2.ogg DEPENDS ${HEX_GAME_SOURCE_DIR}/res/clang2.ogg)
add_custom_command(OUTPUT res_sndexp.c COMMAND embedfile res_sndexp ${HEX_GAME_SOURCE_DIR}/res/sndexp.ogg DEPENDS ${HEX_GAME_SOURCE_DIR}/res/sndexp.ogg)
add_custom_command(OUTPUT res_shatter.c COMMAND embedfile res_shatter ${HEX_GAME_SOURCE_DIR}/res/shatter.ogg DEPENDS ${HEX_GAME_SOURCE_DIR}/res/shatter.ogg)
add_custom_command(OUTPUT res_whoosh0.c COMMAND embedfile res_whoosh0 ${HEX_GAME_SOURCE_DIR}/res/whoosh0.ogg DEPENDS ${HEX_GAME_SOURCE_DIR}/res/whoosh0.ogg)
add_custom_command(OUTPUT res_whoosh1.c COMMAND embedfile res_whoosh1 ${HEX_GAME_SOURCE_DIR}/res/whoosh1.ogg DEPENDS ${HEX_GAME_SOURCE_DIR}/res/whoosh1.ogg)
add_custom_command(OUTPUT res_sizzle.c COMMAND embedfile res_sizzle ${HEX_GAME_SOURCE_DIR}/res/sizzle.ogg DEPENDS ${HEX_GAME_SOURCE_DIR}/res/sizzle.ogg)
add_custom_command(OUTPUT res_mskfs.c COMMAND embedfile res_mskfs ${HEX_GAME_SOURCE_DIR}/res/mask.fs DEPENDS ${HEX_GAME_SOURCE_DIR}/res/mask.fs)
add_custom_command(OUTPUT res_ppfs.c COMMAND embedfile res_ppfs ${HEX_GAME_SOURCE_DIR}/res/post_proc.fs DEPENDS ${HEX_GAME_SOURCE_DIR}/res/post_proc.fs)

# TARGET
set(GAME_SOURCE_FILES    
  "src/game.cpp"
  res_tiles.c
  res_explosion.c
  res_splash.c
  res_font.c
  res_music.c
  res_clang0.c
  res_clang1.c
  res_clang2.c
  res_sndexp.c
  res_shatter.c
  res_whoosh0.c
  res_whoosh1.c
  res_sizzle.c
  res_mskfs.c
  res_ppfs.c
)
if (GAME_BASE_SHARED_BUILD)
  add_library(GAME_NEW SHARED ${GAME_SOURCE_FILES})
  target_compile_definitions(GAME_NEW PRIVATE GAME_BASE_DLL)
  add_custom_command(TARGET GAME_NEW
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove -f ${GAME_SOURCE_FILES}/build/GAME.dll
    COMMENT "Removing old dll"
  )
else()
  add_library(GAME ${GAME_SOURCE_FILES})
endif()

# DEPENDENCIES
set(RAYLIB_VERSION 5.5.4)
if (GAME_BASE_SHARED_BUILD)
  set(BUILD_SHARED_LIBS ON CACHE BOOL "Build raylib as shared library" FORCE)
endif()
find_package(raylib ${RAYLIB_VERSION} QUIET) # QUIET or REQUIRED
if (NOT raylib_FOUND) # If there's none, fetch and build raylib
  include(FetchContent)
  set(CMAKE_TLS_VERIFY FALSE)
  FetchContent_Declare(
    raylib
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/KraftJrIvo/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz
    URL_HASH SHA512=0f24c24108a75b5f4e82de38e814f17220823bd1e578369eee4122f323e491dc4a607d192c21935170035a22b23d0f4a5f197486758be434c6f80759f1356b80
  )
  FetchContent_GetProperties(raylib)
  if (NOT raylib_POPULATED)
    set(FETCHCONTENT_QUIET NO)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    FetchContent_MakeAvailable(raylib)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # don't build the supplied examples
  endif()
endif()
target_compile_options(raylib PUBLIC -DRAYMATH_DISABLE_CPP_OPERATORS)
target_compile_options(raylib PUBLIC -DMANUAL_INPUT_EVENTS_POLLING)
target_compile_options(raylib PUBLIC -DGRAPHICS_API_OPENGL_33)

if (GAME_BASE_SHARED_BUILD)
  target_link_libraries(GAME_NEW PUBLIC raylib)
else()
  target_link_libraries(GAME PUBLIC raylib)
endif()