#!/bin/sh

set -e

# We are running inside a minimal Rocky container in order to preserve disk
# space on the GitHub runners. Install the bare minimum of packages required by
# Gaffer/RenderMan.

microdnf install -y lcms2 mesa-libGL mesa-libGLU libglvnd-opengl fontconfig libgomp libxcb libxkbcommon-x11 xcb-util-image xcb-util-cursor xcb-util-wm xcb-util-keysyms libICE libSM

# Run the tests.

export QT_DEBUG_PLUGINS=1

echo "::add-matcher::./.github/workflows/main/problemMatchers/unittest.json"
$GAFFER_BUILD_DIR/bin/gaffer test IECoreRenderManTest GafferRenderManTest GafferRenderManUITest
echo "::remove-matcher owner=unittest::"
