cmake_minimum_required(VERSION 3.21)

project(sq68ux C)

add_compile_options(-U_FORTIFY_SOURCE -Werror -Wall -Wextra -Wpedantic)

if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
  set(USE_FLAGS "-s USE_SDL=2")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 ${USE_FLAGS}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 ${USE_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} ${USE_FLAGS} -s ALLOW_MEMORY_GROWTH=1 --preload-file ${CMAKE_SOURCE_DIR}/Q68_SMSQ.bin@./Q68_SMSQ.bin"
  )
  configure_file(q68_wrapper_sample.html q68.html COPYONLY)
else()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb -Og")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -Og")
  find_package(SDL3 REQUIRED)
endif()

include_directories(
  ${CMAKE_CURRENT_LIST_DIR}
  include/
  Musashi/
  args/src/
  inih/
  log.c/src/
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

cmake_path(NORMAL_PATH PROJECT_SOURCE_DIR OUTPUT_VARIABLE PROJECT_ROOT_NORMALIZED)
add_compile_options(-ffile-prefix-map=${PROJECT_ROOT_NORMALIZED}=)

add_custom_target(
  Musashi
  COMMAND
  make m68kops.c
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Musashi/
)
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
  log.c/src/log.c
  Musashi/m68kcpu.c
  Musashi/m68kops.c
  Musashi/m68kdasm.c
  Musashi/softfloat/softfloat.c
  ${CMAKE_CURRENT_BINARY_DIR}/version.c)

add_dependencies(sq68ux SubmarineGitVersion Musashi)

target_compile_options(sq68ux PRIVATE -DQ68_EMU -DEMU_STR="sq68ux" -Wall
  -Wextra -Wpedantic -Werror
  -DMUSASHI_CNF=\"emu68kconf.h\")

set_property(TARGET sq68ux PROPERTY C_STANDARD 17)

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
  src/qlay_trace.c
  args/src/args.c
  inih/ini.c
  log.c/src/log.c
  Musashi/m68kcpu.c
  Musashi/m68kops.c
  Musashi/m68kdasm.c
  Musashi/softfloat/softfloat.c
  ${CMAKE_CURRENT_BINARY_DIR}/version.c)

add_dependencies(sqlay3 SubmarineGitVersion Musashi)

target_compile_options(sqlay3 PRIVATE -DQLAY_EMU -DEMU_STR="sqlay3" -Wall
  -Wextra -Wpedantic -Werror
  -DMUSASHI_CNF=\"emu68kconf.h\")

set_property(TARGET sqlay3 PROPERTY C_STANDARD 17)

# m68kdasm.c doesnt pass _FORTIFY_SOURCE=2
if(NOT ${CMAKE_C_COMPILER_ID} MATCHES "Clang")
  set_source_files_properties(Musashi/m68kdasm.c
    PROPERTIES COMPILE_FLAGS -Wno-format-overflow)
endif()

set_source_files_properties(args/src/args.c Musashi/m68kcpu.c
  PROPERTIES COMPILE_FLAGS -Wno-pedantic)
set_source_files_properties(Musashi/m68kops.c
  PROPERTIES COMPILE_FLAGS -Wno-unused-variable)

if(SDL3_LIBRARIES)
  if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(EMU_BUILD_LIBS
      "-Wl,-Bstatic -L/usr/local/x86_64-w64-mingw32/lib -lmingw32 -lSDL3main -lSDL3 -lpthread -Wl,-Bdynamic -lwsock32 -lwinmm -lsetupapi -limm32 -lversion -static-libgcc"
    )
  else()
    set(EMU_BUILD_LIBS ${SDL3_LIBRARIES})
  endif()
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
  set(EMU_BUILD_LIBS ${SDL3_LIBRARIES})
else()
  set(EMU_BUILD_LIBS "SDL3::SDL3")
endif()

target_link_libraries(sq68ux -lm ${EMU_BUILD_LIBS})
target_link_libraries(sqlay3 -lm ${EMU_BUILD_LIBS})

install(TARGETS sqlay3 DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/)
