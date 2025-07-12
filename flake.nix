{
  inputs = {
    nixpkgs = {
      type = "github";
      owner = "NixOS";
      repo = "nixpkgs";
      ref = "nixos-unstable";
    };
    godot-cpp = {
      flake = false;
      type = "github";
      owner = "godotengine";
      repo = "godot-cpp";
      ref = "f3a1a2fd458dfaf4de08c906f22a2fe9e924b16f";
    };
  };
  outputs = {self, nixpkgs, godot-cpp, ...}:
  let system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
      pkgsCross = import nixpkgs {
        inherit system;
        crossSystem = {
          config = "x86_64-w64-mingw32";
        };
        allowUnsupportedSystem = true;
      };
      stdenv = pkgs.stdenv;
      mkSconsFlagsFromAttrSet = pkgs.lib.mapAttrsToList (
        k: v: if builtins.isString v then "${k}=${v}" else "${k}=${builtins.toJSON v}"
      );
  in {
    devShells.${system}.default = pkgs.mkShell {
      buildInputs = [pkgs.scons pkgsCross.buildPackages.gcc pkgsCross.windows.mcfgthreads];
    };
    packages.${system} = {
      default = self.packages.${system}.godot-multiplex-peer;
      godot-multiplex-peer-windows = with stdenv; mkDerivation rec {
        pname = "godot-multiplex-peer";
        version = "0.0.1";
        src = ./.;
        nativeBuildInputs = with pkgs; [scons pkgsCross.buildPackages.gcc pkgsCross.windows.mcfgthreads];
        sconsFlags = mkSconsFlagsFromAttrSet {
          target = "editor";
          platform = "windows";
          arch = "x86_64";
          compiledb="yes";
          dev_build="yes";
        };
      };
      godot-multiplex-peer = with stdenv; mkDerivation rec {
        pname = "godot-multiplex-peer";
        version = "0.0.1";
        src = ./.;
        nativeBuildInputs = with pkgs; [scons];
        sconsFlags = mkSconsFlagsFromAttrSet {
          target = "editor";
          platform = "linux";
          arch = "x86_64";
          compiledb="yes";
          dev_build="yes";
        };
      };
    };
  };
}
