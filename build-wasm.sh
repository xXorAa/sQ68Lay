#!/bin/sh
(emcmake cmake -B wasm . && cd wasm && make)
cp q68_wrapper_sample.html wasm/q68.html
