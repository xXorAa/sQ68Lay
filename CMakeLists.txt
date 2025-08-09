cmake_minimum_required(VERSION 3.10)
include(CheckIPOSupported)

project(sq68ux C)

check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)

add_compile_options(-U_FORTIFY_SOURCE -Werror -Wall -Wextra -Wpedantic)

find_package(SDL3 REQUIRED)

include_directories(
  ${CMAKE_CURRENT_LIST_DIR}
  include/
  Musashi/
  args/src/
  libayemu/include/
  inih/
  external/
  qlay/
  uthash/src/)

find_package(Git)
add_custom_target(
  SubmarineGitVersion
  COMMAND
    ${CMAKE_COMMAND} -D GIT_EXECUTABLE=${GIT_EXECUTABLE} -D
    INPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/version.c.in -D
    OUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/version.c -P
    ${CMAKE_CURRENT_SOURCE_DIR}/generate_version.cmake)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/version.c
                            PROPERTIES GENERATED TRUE)

cmake_path(NORMAL_PATH PROJECT_SOURCE_DIR OUTPUT_VARIABLE
           PROJECT_ROOT_NORMALIZED)
add_compile_options(-ffile-prefix-map=${PROJECT_ROOT_NORMALIZED}=)

if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
  set(EM_LINK "-sALLOW_MEMORY_GROWTH")
  set(QLAY3_LINK
      "--preload-file" "${CMAKE_SOURCE_DIR}/sqlay3.ini@/sqlay3.ini"
      "--preload-file"
      "${CMAKE_SOURCE_DIR}/roms/min1.98a1.bin@/roms/min1.98a1.bin")
  configure_file(qlay_wrapper_sample.html sqlay3.html COPYONLY)
endif()

add_custom_target(
  Musashi
  COMMAND make m68kops.c
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Musashi/)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/Musashi/m68kops.c
                            PROPERTIES GENERATED TRUE)

add_executable(
  sq68ux
  src/emulator_events.c
  src/emulator_files.c
  src/emulator_main.c
  src/emulator_options.c
  src/emulator_screen.c
  src/q68_hardware.c
  src/q68_hooks.c
  src/q68_keyboard.c
  src/q68_mainloop.c
  src/q68_memory.c
  src/q68_sd.c
  external/sdl-ps2.c
  args/src/args.c
  inih/ini.c
  Musashi/m68kcpu.c
  Musashi/m68kops.c
  Musashi/m68kdasm.c
  Musashi/softfloat/softfloat.c
  ${CMAKE_CURRENT_BINARY_DIR}/version.c)

add_dependencies(sq68ux SubmarineGitVersion Musashi)

set_property(TARGET sq68ux PROPERTY C_STANDARD 17)

if(ipo_supported)
    message(STATUS "IPO / LTO enabled")
    set_property(TARGET sq68ux PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "IPO / LTO not supported: <${ipo_error}>")
endif()

target_compile_options(
  sq68ux
  PRIVATE -DQ68_EMU
          -DEMU_STR="sq68ux"
          -Wall
          -Wextra
          -Wpedantic
          -Werror
          -DMUSASHI_CNF=\"emu68kconf.h\"
          "$<$<CONFIG:Debug>:-ggdb;-Og>")

target_link_options(sq68ux PRIVATE ${EM_LINK} ${QLAY3_LINK}
                    "$<$<CONFIG:Debug>:-ggdb;-Og>")

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  target_link_libraries(sq68ux PRIVATE SDL3::SDL3-static libssp.a)
else()
  target_link_libraries(sq68ux PRIVATE m SDL3::SDL3)
endif()

add_executable(
  sqlay3
  src/emulator_events.c
  src/emulator_files.c
  src/emulator_main.c
  src/emulator_options.c
  src/emulator_screen.c
  src/qlay_disk.c
  src/qlay_memory.c
  src/qlay_hardware.c
  src/qlay_hooks.c
  src/qlay_io.c
  src/qlay_keyboard.c
  src/qlay_mainloop.c
  src/qlay_sound.c
  src/qlay_trace.c
  args/src/args.c
  libayemu/src/ay8912.c
  inih/ini.c
  Musashi/m68kcpu.c
  Musashi/m68kops.c
  Musashi/m68kdasm.c
  Musashi/softfloat/softfloat.c
  ${CMAKE_CURRENT_BINARY_DIR}/version.c)

set_source_files_properties(
  libayemu/src/ay8912.c
  PROPERTIES
    COMPILE_FLAGS
    "-Wno-unused-variable -Wno-format -Wno-unused-parameter -Wno-strict-prototypes -Wno-invalid-utf8 -Wno-format-pedantic"
)

add_dependencies(sqlay3 SubmarineGitVersion Musashi)

set_property(TARGET sqlay3 PROPERTY C_STANDARD 17)

if(ipo_supported)
    message(STATUS "IPO / LTO enabled")
    set_property(TARGET sqlay3 PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "IPO / LTO not supported: <${ipo_error}>")
endif()

target_compile_options(
  sqlay3
  PRIVATE -DQLAY_EMU
          -DEMU_STR="sqlay3"
          -Wall
          -Wextra
          -Wpedantic
          -Werror
          -DMUSASHI_CNF=\"emu68kconf.h\"
          "$<$<CONFIG:Debug>:-ggdb;-Og>")

if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
  target_link_options(
    sqlay3 PRIVATE -sALLOW_MEMORY_GROWTH
    --preload-file=${CMAKE_SOURCE_DIR}/sqlay3.ini@/sqlay3.ini
    --preload-file=${CMAKE_SOURCE_DIR}/roms/min1.98a1.bin@/roms/min1.98a1.bin)
else()
  target_link_options(sqlay3 PRIVATE "$<$<CONFIG:Debug>:-ggdb;-Og>")
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  target_link_libraries(sqlay3 PRIVATE SDL3::SDL3-static libssp.a)
else()
  target_link_libraries(sqlay3 PRIVATE m SDL3::SDL3)
endif()

# m68kdasm.c doesnt pass _FORTIFY_SOURCE=2
if(NOT ${CMAKE_C_COMPILER_ID} MATCHES "Clang")
  set_source_files_properties(Musashi/m68kdasm.c
                              PROPERTIES COMPILE_FLAGS -Wno-format-overflow)
endif()

set_source_files_properties(args/src/args.c Musashi/m68kcpu.c
                            PROPERTIES COMPILE_FLAGS -Wno-pedantic)
set_source_files_properties(Musashi/m68kops.c PROPERTIES COMPILE_FLAGS
                                                         -Wno-unused-variable)

install(TARGETS sq68ux DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/)
install(TARGETS sqlay3 DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/)
