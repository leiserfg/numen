{
  description = "numen: natural language calculator library";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      lib = nixpkgs.lib;
      forAllSystems = f: lib.genAttrs lib.systems.flakeExposed (system: f nixpkgs.legacyPackages.${system});
      # single source of truth: project(... VERSION x.y.z) in CMakeLists.txt
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
            description = "Natural language calculator library";
            homepage = "https://github.com/vicinaehq/libnumen";
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
