cmake_minimum_required(VERSION 3.13)
project(runner LANGUAGES CXX)

# Define the application target. To change its name, change BINARY_NAME in the
# top-level CMakeLists.txt, not the value here, or `flutter run` will no longer
# work.
#
# Any new source files that you add to the application should be added here.
add_executable(${BINARY_NAME}
  "main.cc"
  "my_application.cc"
  "${FLUTTER_MANAGED_DIR}/generated_plugin_registrant.cc"
  "screen_recorder/screen_recorder_plugin.cc"
  "screen_recorder/screen_recorder_native.cc"
  "screen_recorder/portal/portal_client.cc"
  "screen_recorder/capture/pipewire_capture.cc"
  "screen_recorder/encoder/ffmpeg_writer.cc"
)

# Apply the standard set of build settings. This can be removed for applications
# that need different build settings.
apply_standard_settings(${BINARY_NAME})

# Add preprocessor definitions for the application ID.
add_definitions(-DAPPLICATION_ID="${APPLICATION_ID}")

# Add dependency libraries. Add any application-specific dependencies here.
target_link_libraries(${BINARY_NAME} PRIVATE flutter)
target_link_libraries(${BINARY_NAME} PRIVATE PkgConfig::GTK)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GIO REQUIRED gio-2.0 gio-unix-2.0)
pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)
pkg_check_modules(SPA REQUIRED libspa-0.2)

target_include_directories(${BINARY_NAME} PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/screen_recorder"
  "${CMAKE_CURRENT_SOURCE_DIR}/screen_recorder/portal"
  "${CMAKE_CURRENT_SOURCE_DIR}/screen_recorder/capture"
  "${CMAKE_CURRENT_SOURCE_DIR}/screen_recorder/encoder"
  "${CMAKE_CURRENT_SOURCE_DIR}/screen_recorder/utils"
  ${GLIB_INCLUDE_DIRS}
  ${GIO_INCLUDE_DIRS}
  ${PIPEWIRE_INCLUDE_DIRS}
  ${SPA_INCLUDE_DIRS}
)

target_compile_options(${BINARY_NAME} PRIVATE
  ${GLIB_CFLAGS_OTHER}
  ${GIO_CFLAGS_OTHER}
  ${PIPEWIRE_CFLAGS_OTHER}
  ${SPA_CFLAGS_OTHER}
)

target_link_libraries(${BINARY_NAME} PRIVATE
  ${GLIB_LIBRARIES}
  ${GIO_LIBRARIES}
  ${PIPEWIRE_LIBRARIES}
  ${SPA_LIBRARIES}
)
