#!/bin/sh

set -e

# We are running inside a minimal Rocky container in order to preserve disk
# space on the GitHub runners. Install the bare minimum of packages required by
# Gaffer/RenderMan.

microdnf install -y lcms2 mesa-libGL mesa-libGLU libglvnd-opengl fontconfig libgomp

# Run the tests.

echo "::add-matcher::./.github/workflows/main/problemMatchers/unittest.json"
$GAFFER_BUILD_DIR/bin/gaffer test IECoreRenderManTest GafferRenderManTest
echo "::remove-matcher owner=unittest::"
