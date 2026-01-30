{
  pkgs ? import <nixpkgs> { },
}:
pkgs.mkShell.override { stdenv = pkgs.llvmPackages_latest.libcxxStdenv; } {
  nativeBuildInputs = with pkgs; [
    cmake
    clang-tools
    ninja
    llvmPackages_latest.lldb
    ccache
    openssl
  ];
}
