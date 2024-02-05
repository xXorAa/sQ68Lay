{ pkgs ? import <nixpkgs> {} }:

with pkgs;
mkShell {
  nativeBuildInputs = [
    glib
    clang
    cmake
    pkg-config
    SDL2
  ];
}
