#!/usr/bin/env bash
# Build and run the order book tests, then the benchmark, for one version.
# Output goes to the terminal and to results/<version>_<timestamp>.txt.
#
# Usage:
#   ./run.sh v01            # tests + all benchmark scenarios for v01
#   ./run.sh v01 mixed      # tests + only scenarios whose name contains "mixed"
#
# Override the compiler with:  CXX=/path/to/clang++ ./run.sh v01

set -euo pipefail

cd "$(dirname "$0")"

VERSION="${1:-}"
FILTER="${2:-}"

if [[ -z "$VERSION" || ! -f "$VERSION/orderbook.hpp" ]]; then
    echo "usage: $0 <version> [scenario-filter]" >&2
    echo "available versions:" $(ls -d v*/ 2>/dev/null | tr -d /) >&2
    exit 1
fi

# Apple clang 15 can't compile orderbook.hpp (C++20 parenthesized aggregate init).
CXX="${CXX:-/opt/homebrew/opt/llvm/bin/clang++}"
BUILD_DIR="${TMPDIR:-/tmp}/orderbook_build/$VERSION"
mkdir -p "$BUILD_DIR" results

OUT="results/${VERSION}_$(date +%Y%m%d_%H%M%S).txt"

{
    echo "=== $VERSION  $(date)  compiler: $("$CXX" --version | head -1)"

    echo
    echo "=== tests (ASan + UBSan)"
    "$CXX" -std=c++20 -Wall -Wextra -O1 -g -fsanitize=address,undefined -I "$VERSION" \
        orderbook_test.cpp -o "$BUILD_DIR/orderbook_test"
    "$BUILD_DIR/orderbook_test"

    echo
    echo "=== benchmark (-O3)"
    "$CXX" -std=c++20 -Wall -Wextra -O3 -DNDEBUG -mcpu=native -I "$VERSION" \
        orderbook_bench.cpp -o "$BUILD_DIR/orderbook_bench"
    "$BUILD_DIR/orderbook_bench" "$FILTER"
} 2>&1 | tee "$OUT"

echo
echo "saved to $OUT"
