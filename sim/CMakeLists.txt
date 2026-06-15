cmake_minimum_required(VERSION 3.20)
project(deskclock_sim LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

find_package(SDL2 REQUIRED)

file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS
  "${REPO_ROOT}/lib/lvgl/src/*.c"
)
list(FILTER LVGL_SOURCES EXCLUDE REGEX "/src/(demos|examples|libs|others)/")
list(FILTER LVGL_SOURCES EXCLUDE REGEX "/src/drivers/(display|evdev|glfw|libinput|nuttx|qnx|uefi|wayland|windows|x11)/")
list(FILTER LVGL_SOURCES EXCLUDE REGEX "/src/draw/(dma2d|nema_gfx|nxp|opengles|renesas|sdl|vg_lite)/")
list(APPEND LVGL_SOURCES
  "${REPO_ROOT}/lib/lvgl/src/libs/bin_decoder/lv_bin_decoder.c"
  "${REPO_ROOT}/lib/lvgl/src/libs/fsdrv/lv_fs_stdio.c"
  "${REPO_ROOT}/lib/lvgl/src/libs/lodepng/lodepng.c"
  "${REPO_ROOT}/lib/lvgl/src/libs/lodepng/lv_lodepng.c"
  "${REPO_ROOT}/lib/lvgl/src/libs/tjpgd/lv_tjpgd.c"
  "${REPO_ROOT}/lib/lvgl/src/libs/tjpgd/tjpgd.c"
  "${REPO_ROOT}/lib/lvgl/src/others/snapshot/lv_snapshot.c"
)

file(GLOB APP_SOURCES CONFIGURE_DEPENDS
  "${REPO_ROOT}/src/app/*.cpp"
  "${REPO_ROOT}/src/app/*.c"
)

add_executable(deskclock_sim
  ${LVGL_SOURCES}
  ${APP_SOURCES}
  "${CMAKE_CURRENT_LIST_DIR}/HardwareStubs.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
)

target_compile_definitions(deskclock_sim PRIVATE
  DESKCLOCK_SIMULATOR=1
  LV_CONF_INCLUDE_SIMPLE
)

target_include_directories(deskclock_sim PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}"
  "${CMAKE_CURRENT_LIST_DIR}/include"
  "${REPO_ROOT}"
  "${REPO_ROOT}/src"
  "${REPO_ROOT}/lib/lvgl"
  ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(deskclock_sim PRIVATE ${SDL2_LIBRARIES})

add_executable(deskclock_setup_tests
  "${REPO_ROOT}/src/app/SetupSession.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/tests/SetupSessionHarnessTest.cpp"
)

target_compile_definitions(deskclock_setup_tests PRIVATE
  DESKCLOCK_SIMULATOR=1
)

target_include_directories(deskclock_setup_tests PRIVATE
  "${REPO_ROOT}/src"
)

enable_testing()
add_test(NAME deskclock_setup_tests COMMAND deskclock_setup_tests)

if(APPLE)
  target_link_libraries(deskclock_sim PRIVATE "-framework CoreFoundation")
endif()
