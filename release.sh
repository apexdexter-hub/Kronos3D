#!/bin/bash
FASE=${1:-"1"}
DESC=${2:-"build"}
VERSION="fase${FASE}-${DESC}"
OUTPUT_DIR="/workspaces/codespaces-blank/solid-capybara/Kronos3D/release"
KEYSTORE="$HOME/kronos3d-dev.keystore"
APK_UNSIGNED="app/build/outputs/apk/debug/app-debug.apk"
APK_SIGNED="$OUTPUT_DIR/Kronos3D-${VERSION}.apk"

rm -f "$OUTPUT_DIR"/*.apk
mkdir -p "$OUTPUT_DIR"
./gradlew assembleDebug 2>&1 | tail -20
if [ ! -f "$APK_UNSIGNED" ]; then echo "❌ BUILD FAILED"; exit 1; fi

# Align APK
$ANDROID_HOME/build-tools/35.0.0/zipalign -f -v 4 "$APK_UNSIGNED" "$OUTPUT_DIR/aligned.apk"

# Sign APK
$ANDROID_HOME/build-tools/35.0.0/apksigner sign \
    --ks "$KEYSTORE" --ks-alias kronos3d-dev \
    --ks-pass pass:kronos3d2024 --key-pass pass:kronos3d2024 \
    --out "$APK_SIGNED" "$OUTPUT_DIR/aligned.apk"

rm -f "$OUTPUT_DIR/aligned.apk"

# Verify signature
$ANDROID_HOME/build-tools/35.0.0/apksigner verify "$APK_SIGNED"
echo "✅ APK lista: $APK_SIGNED ($(du -h $APK_SIGNED | cut -f1))"
