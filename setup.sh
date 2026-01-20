#!/usr/bin/env bash
set -e

echo "========================================"
echo "  Hermes Build System (Linux/macOS)"
echo "========================================"
echo ""

# Check dependencies
check_dep() {
    if command -v $1 &>/dev/null || pkg-config --exists $2 2>/dev/null; then
        echo "  [OK] $1"
        return 0
    fi
    echo "  [ERROR] $1 not found"
    return 1
}

echo "[1/3] Checking dependencies..."
check_dep g++ gcc || exit 1
check_dep pkg-config || exit 1
check_dep boost || echo "  [WARN] Boost not via pkg-config, checking headers..."
check_dep pugixml || echo "  [WARN] Pugixml not via pkg-config, checking headers..."

# Build library
echo ""
echo "[2/3] Building Hermes library..."
cd src/Hermes
make clean
make
cp -f .libs/libhermes.so* ../../
cd ../..

# Build and run tests
echo ""
echo "[3/3] Running tests..."

if [ -f "simple_test.cpp" ]; then
    echo "  Building simple_test..."
    g++ -std=c++17 -I src/include -L . -o simple_test simple_test.cpp \
        -lhermes -lboost_system -lboost_thread -lpugixml -lpthread \
        -Wl,-rpath,'$ORIGIN'
    ./simple_test || true
fi

if [ -d "test/BoostTestHermes" ]; then
    echo "  Building official test suite..."
    cd test/BoostTestHermes
    make clean
    make
    ./BoostTestHermes || true
    cd ../..
fi

echo ""
echo "========================================"
echo "  Build Complete!"
echo "========================================"