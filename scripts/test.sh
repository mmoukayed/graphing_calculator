#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
test_output=$(mktemp -d "${TMPDIR:-/tmp}/deckcalc-tests.XXXXXX")
cxx=${CXX:-clang++}
"$cxx" -std=c++17 -Wall -Wextra -pedantic -fsanitize=address,undefined -g -Ilib/CalcCore test/core_tests.cpp lib/CalcCore/CalcCore.cpp lib/CalcCore/Equation.cpp -o "$test_output/core"
"$test_output/core"
"$cxx" -std=c++17 -Wall -Wextra -pedantic -fsanitize=address,undefined -g -Itest/host -Iinclude -Ilib/CalcCore -Ilib/MathLayout test/ui_tests.cpp lib/CalcCore/CalcCore.cpp lib/CalcCore/Equation.cpp lib/MathLayout/MathLayout.cpp -o "$test_output/ui"
"$test_output/ui" "$test_output"
