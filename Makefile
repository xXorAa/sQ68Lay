TARGETS = build/sqlay3 build/sq68ux build/compile_commands.json
TARGETS_MINGW = build/sqlux.exe

ALL : ${TARGETS} 
	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=yes -B build
	cmake --build build

mingw32 : ${TARGETS_MINGW}
	cmake -DCMAKE_TOOLCHAIN_FILE=Toolchain-mingw-w64-i686.cmake -B build
	cmake --build build -j 8

mingw64 : ${TARGETS_MINGW}
	cmake -DCMAKE_TOOLCHAIN_FILE=Toolchain-mingw-w64-x86_64.cmake -B build
	cmake --build build -j 8

install :
	cmake --install build

${TARGETS} : FORCE
${TARGETS_MINGW} : FORCE

FORCE: ;

clean:
	rm -rf build/
	make -C Musashi clean
