#!/bin/sh
/bin/mkdir -p "wasm"
(emcmake cmake -B wasm . && cd wasm && make)
