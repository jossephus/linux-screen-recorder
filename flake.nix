{
  description = "Linux Screen Recorder - Development environment";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        isLinux = pkgs.stdenv.isLinux;
        linuxDeps = with pkgs; [
          clang
          cmake
          ninja
          pkg-config
          gtk3
          pipewire
          ffmpeg
        ];
        commonDeps = with pkgs; [
          clang
          cmake
          ninja
          pkg-config
          ffmpeg
        ];
      in
      {
        devShells.default = pkgs.mkShell {
          buildInputs = if isLinux then linuxDeps else commonDeps;
        };
      }
    );
}
