#!/bin/sh
/bin/mkdir -p "wasm"
(emcmake cmake -B wasm . -DCMAKE_FIND_ROOT_PATH=/ && cd wasm && make)
