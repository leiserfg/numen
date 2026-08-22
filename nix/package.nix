{
  lib,
  stdenv,
  cmake,
  ninja,
  openssl,
  catch2_3,
  src ? lib.cleanSource ../.,
  withRepl ? true,
  doCheck ? true,
}:
stdenv.mkDerivation {
  pname = "numen";
  # single source of truth: project(... VERSION x.y.z) in CMakeLists.txt
  version = builtins.head (
    builtins.match ".*project\\([^)]*VERSION ([0-9.]+).*" (builtins.readFile ../CMakeLists.txt)
  );
  inherit src;

  nativeBuildInputs = [
    cmake
    ninja
  ];
  buildInputs = lib.optionals withRepl [ openssl ];
  nativeCheckInputs = [ catch2_3 ];

  cmakeFlags = [
    (lib.cmakeBool "BUILD_SHARED_LIBS" true)
    (lib.cmakeBool "BUILD_REPL" withRepl)
    (lib.cmakeBool "BUILD_TESTS" doCheck)
    (lib.cmakeBool "USE_SYSTEM_CATCH" true)
  ];

  inherit doCheck;

  meta = {
    description = "Natural language calculator library";
    homepage = "https://github.com/vicinaehq/numen";
    platforms = lib.platforms.unix;
  }
  // lib.optionalAttrs withRepl { mainProgram = "numen"; };
}
