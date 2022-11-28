cmake_minimum_required(VERSION 3.7)

project(sq68ux)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set(USE_FLAGS "-pthread -s USE_SDL=2 -s USE_PTHREADS=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 ${USE_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 ${USE_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${USE_FLAGS} -s ALLOW_MEMORY_GROWTH=1 -s PTHREAD_POOL_SIZE=2 --preload-file ${CMAKE_SOURCE_DIR}/Q68_SMSQ.bin@./Q68_SMSQ.bin")
    configure_file(q68_wrapper_sample.html q68.html COPYONLY)
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb -Og")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -Og")
    find_package(SDL2 REQUIRED)
endif()

include_directories(${CMAKE_CURRENT_LIST_DIR} include/ Musashi/ CLI11/include/ external/ ${SDL2_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})

add_executable(sq68ux
    src/emu_options.cpp
    src/q68_main.cpp
    src/q68_emulator.cpp
    src/q68_events.cpp
    src/q68_hardware.cpp
    src/q68_screen.cpp
#    qlay/qlio.cpp
    external/sdl-ps2.c
    Musashi/m68kcpu.c
    Musashi/m68kops.c
    Musashi/m68kdasm.c
    Musashi/softfloat/softfloat.c
    )

target_compile_options(sq68ux PRIVATE "-DQ68_EMU")

add_executable(sqlay3
    src/emu_options.cpp
    src/q68_main.cpp
    src/q68_emulator.cpp
    src/q68_events.cpp
    src/q68_hardware.cpp
    src/q68_screen.cpp
    qlay/qlio.cpp
    external/sdl-ps2.c
    Musashi/m68kcpu.c
    Musashi/m68kops.c
    Musashi/m68kdasm.c
    Musashi/softfloat/softfloat.c
    )

target_compile_options(sqlay3 PRIVATE "-DQLAY_EMU")

if (SDL2_LIBRARIES)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        set(EMU_BUILD_LIBS "-Wl,-Bstatic -L/usr/local/x86_64-w64-mingw32/lib -lSDL2main -lSDL2 -lpthread -Wl,-Bdynamic -lwsock32 -lwinmm -lsetupapi -limm32 -lversion -static-libgcc")
    else()
        set(EMU_BUILD_LIBS ${SDL2_LIBRARIES})
    endif()
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set(EMU_BUILD_LIBS ${SDL2_LIBRARIES})
else()
    set(EMU_BUILD_LIBS "SDL2::SDL2")
endif()

target_link_libraries(sq68ux ${EMU_BUILD_LIBS})
target_link_libraries(sqlay3 ${EMU_BUILD_LIBS})

install(TARGETS sq68ux sqlay3
	DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/
	)
