with import <nixpkgs> {};
llvmPackages_16.stdenv.mkDerivation {
  name = "env";
  nativeBuildInputs = [ 
    cmake
    pkg-config
    clang-tools
  ];
  buildInputs = [
    SDL2
  ];
}

