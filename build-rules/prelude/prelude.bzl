# The rules this project builds with.
#
# Buck2's own prelude is not used: it is a large tree whose C++ toolchain machinery exists
# to support far more than one program, and the whole build here is "compile these sources,
# link one executable, run some generators". These rules are that, and nothing else.
#
# The compiler and its flags come from the nix devShell through .buckconfig reads of the
# environment, so a buck2 build and tools/build.sh are the same compile.

load("@prelude//cxx.bzl", _cxx_binary = "cxx_binary", _cxx_library = "cxx_library")
load("@prelude//generate.bzl", _generated_header = "generated_header")

cxx_binary = _cxx_binary
cxx_library = _cxx_library
generated_header = _generated_header
