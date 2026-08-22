{
  description = "numen: a zero dependency calculator library with first class support for units and timezone conversions";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      lib = nixpkgs.lib;
      forAllSystems = f: lib.genAttrs lib.systems.flakeExposed (system: f nixpkgs.legacyPackages.${system});
      version = builtins.head (builtins.match ".*project\\([^)]*VERSION ([0-9.]+).*" (builtins.readFile ./CMakeLists.txt));
    in
    {
      packages = forAllSystems (pkgs: rec {
        numen = pkgs.stdenv.mkDerivation {
          pname = "numen";
          inherit version;
          src = self;

          nativeBuildInputs = [ pkgs.cmake pkgs.ninja ];
          buildInputs = [ pkgs.openssl ];
          nativeCheckInputs = [ pkgs.catch2_3 ];

          cmakeFlags = [
            (lib.cmakeBool "BUILD_SHARED_LIBS" true)
            (lib.cmakeBool "BUILD_REPL" true)
            (lib.cmakeBool "BUILD_TESTS" true)
            (lib.cmakeBool "USE_SYSTEM_CATCH" true)
          ];

          doCheck = true;

          meta = {
            description = "Zero dependency calculator library with first class support for units and timezone conversions";
            homepage = "https://github.com/vicinaehq/numen";
            mainProgram = "numen";
            platforms = lib.platforms.unix;
          };
        };
        default = numen;
      });

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          inputsFrom = [ self.packages.${pkgs.stdenv.hostPlatform.system}.numen ];
          packages = [ pkgs.clang-tools ];
        };
      });
    };
}
