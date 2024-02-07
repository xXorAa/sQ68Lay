TARGETS = build/sqlay3 build/sq68ux build/compile_commands.json

ALL : ${TARGETS} 
	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=yes -B build
	cmake --build build

${TARGETS} : FORCE

FORCE: ;

clean:
	rm -rf build/
