#!/bin/bash
set -e

# Config
RSJFW_DIR="$HOME/.rsjfw"
TEST_PREFIX="$HOME/.rsjfw/manual_test_pfx"
STUDIO_BIN="$(find $RSJFW_DIR/versions -name RobloxStudioBeta.exe | head -n 1)"

# Find Wine
WINE_DIR="$(find $RSJFW_DIR/wine -name wine -type f | grep "bin/wine$" | head -n 1 | xargs dirname | xargs dirname)"
WINE_BIN="$WINE_DIR/bin/wine"

if [ -z "$WINE_BIN" ]; then
    echo "Could not find wine binary!"
    exit 1
fi

# Lib Paths (Crucial!)
# export LD_LIBRARY_PATH="$WINE_DIR/lib/wine/x86_64-unix:$WINE_DIR/lib/wine:$WINE_DIR/lib:$LD_LIBRARY_PATH"

echo "Using Wine: $WINE_BIN"
echo "Using Studio: $STUDIO_BIN"
echo "LD_LIBRARY_PATH: $LD_LIBRARY_PATH"

# Clean Setup
echo "Creating clean prefix at $TEST_PREFIX..."
rm -rf "$TEST_PREFIX"
export WINEPREFIX="$TEST_PREFIX"
export WINEARCH="win64"
export WINEDEBUG="-all,err+all"
# export WINEDEBUG="+loaddll" # Uncomment to debug DLLs

"$WINE_BIN" wineboot -u
"$WINE_BIN"server -w

# WebView2 Setup
echo "Setting up WebView2..."
WV2_SOURCE="$RSJFW_DIR/prefix/drive_c/Program Files (x86)/Microsoft/EdgeWebView"
WV2_DEST="$TEST_PREFIX/drive_c/Program Files (x86)/Microsoft/EdgeWebView"

if [ -d "$WV2_SOURCE" ]; then
    mkdir -p "$(dirname "$WV2_DEST")"
    cp -r "$WV2_SOURCE" "$WV2_DEST"
    
    # Manually adding reg keys for WebView2 since we don't have the parser here
    # This is rough but might help if Studio checks existence
    cat <<EOF >> "$TEST_PREFIX/system.reg"

[Software\\\\WOW6432Node\\\\Microsoft\\\\EdgeUpdate\\\\Clients\\\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}]
"pv"="143.0.3650.139"
"location"="C:\\\\Program Files (x86)\\\\Microsoft\\\\EdgeWebView\\\\Application"
EOF

else
    echo "WARNING: WebView2 source not found in main prefix! Studio might hang."
fi

# Env Vars
export WEBVIEW2_BROWSER_EXECUTABLE_FOLDER="C:\\Program Files (x86)\\Microsoft\\EdgeWebView\\Application\\143.0.3650.139"
export WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS="--no-sandbox --disable-gpu --disable-dev-shm-usage --disable-features=RendererCodeIntegrity"

# Qt Fix - Force native to use bundled Qt
export WINEDLLOVERRIDES="Qt5Core,Qt5Gui,Qt5Widgets,Qt5Network,Qt5Qml,Qt5Quick=n,b;winebrowser.exe=b;d3dcompiler_47=n,b;atmlib=b;d3d11,dxgi,d3d9,d3d10core=n,b"

echo "Launching Studio..."
cd "$(dirname "$STUDIO_BIN")" # Change cwd to studio dir
"$WINE_BIN" "$STUDIO_BIN"
