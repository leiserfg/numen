{
  description = "numen: natural language calculator library";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      lib = nixpkgs.lib;
      forAllSystems =
        f: lib.genAttrs lib.systems.flakeExposed (system: f nixpkgs.legacyPackages.${system});
    in
    {
      packages = forAllSystems (pkgs: rec {
        # override knobs: withRepl, doCheck, stdenv
        numen = pkgs.callPackage ./nix/package.nix { src = self; };
        default = numen;
      });

      overlays.default = final: prev: {
        numen = final.callPackage ./nix/package.nix { src = self; };
      };

      formatter = forAllSystems (pkgs: pkgs.nixfmt);

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          inputsFrom = [ self.packages.${pkgs.stdenv.hostPlatform.system}.numen ];
          packages = [ pkgs.clang-tools ];
        };
      });
    };
}
