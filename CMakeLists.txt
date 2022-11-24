cmake_minimum_required(VERSION 3.7)

project(sq68ux)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb -Og")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -Og")
find_package(SDL2 REQUIRED)
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED)

include_directories(${CMAKE_CURRENT_LIST_DIR} include/ Musashi/ external/ ${SDL2_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})

add_executable(sq68ux
    src/q68_main.cpp
    src/q68_emulator.cpp
    src/q68_events.cpp
    src/q68_hardware.cpp
    src/q68_screen.cpp
    external/sdl-ps2.c
    Musashi/m68kcpu.c
    Musashi/m68kops.c
    Musashi/m68kdasm.c
    Musashi/softfloat/softfloat.c
    )

if (SDL2_LIBRARIES AND Boost_FOUND)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        target_link_libraries(sq68ux -Wl,-Bstatic -L/usr/local/x86_64-w64-mingw32/lib -lSDL2main -lSDL2 -Wl,-Bdynamic -lwsock32 -lwinmm -lsetupapi -limm32 -lversion -static-libgcc)
    else()
        target_link_libraries(sq68ux ${SDL2_LIBRARIES} ${Boost_LIBRARIES})
    endif()
else()
    target_link_libraries(sq68ux SDL2::SDL2)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	target_link_libraries(sqq68ux -Wl,-Bstatic -lpthread -Wl,-Bdynamic)
endif()

install(TARGETS sq68ux
	DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/
	)
