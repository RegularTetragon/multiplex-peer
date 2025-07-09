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
      stdenv = pkgs.stdenv;
      mkSconsFlagsFromAttrSet = pkgs.lib.mapAttrsToList (
        k: v: if builtins.isString v then "${k}=${v}" else "${k}=${builtins.toJSON v}"
      );
  in {
    packages.${system} = {
      default = self.packages.${system}.godot-multiplex-peer;
      godot-cpp = 
      with stdenv; mkDerivation rec {
        pname = "godot-cpp";
        version = "1.0.0";
        nativeBuildInputs = with pkgs; [scons pkgconf libgcc xorg.libXcursor xorg.libXinerama xorg.libXi xorg.libXrandr wayland-utils mesa libGLU libGL alsa-lib pulseaudio];
        sconsFlags = mkSconsFlagsFromAttrSet {
          target = "editor";
          platform = "linux";
          arch = "x86_64";
          compiledb="yes";
          dev_build="yes";
        };
        src = godot-cpp;
        installPhase = ''
        ls -a
        cp -rT . $out
        '';
      };
      godot-multiplex-peer = with stdenv; mkDerivation rec {
        pname = "godot-multiplex-peer";
        version = "0.0.1";
        src = ./.;
        nativeBuildInputs = with pkgs; [scons self.packages.${system}.godot-cpp];
        sconsFlags = mkSconsFlagsFromAttrSet {
          target = "editor";
          platform = "linux";
          arch = "x86_64";
          compiledb="yes";
          dev_build="yes";
        };
        installPhase = ''
        cp -r bin/ $out
        cp LICENSE $out
        cp README.md $out
        cp steam-multiplayer-peer.gdextension $out
        cp compile_commands.json $out
        '';
      };
    };
  };
}
