TARGETS = build/sqlay3 build/sq68ux build/compile_commands.json

ALL : ${TARGETS} 
	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=yes -B build
	cmake --build build

debug : ${TARGETS}
	cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=yes -B build
	cmake --build build

mingw32 : mingw_phony
	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=Toolchain-mingw-w64-i686.cmake -B mingw32
	cmake --build mingw32

mingw64 : mingw_phony
	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=Toolchain-mingw-w64-x86_64.cmake -B mingw64
	cmake --build mingw64

wasm : wasm_phony
	emcmake cmake -DCMAKE_BUILD_TYPE=Release -B wasm
	cmake --build wasm

install :
	cmake --install build

${TARGETS} : FORCE
${TARGETS_MINGW} : FORCE

.PHONY: wasm_phony
.PHONY: mingw_phony

FORCE: ;

clean:
	rm -rf build/
	rm -rf wasm/
	make -C Musashi clean
