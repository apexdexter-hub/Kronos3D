#!/bin/bash
FASE=${1:-"X"}
DESC=${2:-"build"}
VERSION="fase${FASE}-${DESC}"

# Export JDK 17 explicitly to prevent Kotlin compiler crash on JDK 25
export JAVA_HOME="/usr/lib/jvm/java-1.17.0-openjdk-amd64"
export PATH="$JAVA_HOME/bin:$PATH"

# Relative to script path
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export ANDROID_HOME="${ANDROID_HOME:-$HOME/android-sdk}"
OUTPUT_DIR="$SCRIPT_DIR/release"
KEYSTORE="$HOME/kronos3d-dev.keystore"
APK_UNSIGNED="$SCRIPT_DIR/app/build/outputs/apk/debug/app-debug.apk"
APK_SIGNED="$OUTPUT_DIR/Kronos3D-${VERSION}.apk"

./gradlew clean 2>/dev/null
rm -rf app/build 2>/dev/null
rm -rf /home/codespace/.gradle/caches/8.9/transforms/ 2>/dev/null
./gradlew --stop 2>/dev/null
echo "🧹 Cache limpiado"

echo "📁 Output dir: $OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"

# Clean old APKs and signatures
if ls "$OUTPUT_DIR"/*.apk 2>/dev/null; then
    echo "🗑️ Eliminando APK anterior..."
    rm -f "$OUTPUT_DIR"/*.apk
    rm -f "$OUTPUT_DIR"/*.idsig
fi

# Build
cd "$SCRIPT_DIR"
./gradlew assembleDebug 2>&1 | tail -20

# Check compilation
if [ ! -f "$APK_UNSIGNED" ]; then
    echo "❌ BUILD FAILED - APK no encontrado en $APK_UNSIGNED"
    exit 1
fi

echo "✅ APK encontrado: $(du -h $APK_UNSIGNED | cut -f1)"

# Check keystore
if [ ! -f "$KEYSTORE" ]; then
    echo "🔑 Keystore no encontrado, generando..."
    keytool -genkeypair -alias kronos3d-dev -keyalg RSA \
        -keysize 2048 -validity 10000 \
        -keystore "$KEYSTORE" \
        -storepass kronos3d2024 -keypass kronos3d2024 \
        -dname "CN=Kronos3D Dev, OU=Dev, O=Kronos3D, L=Unknown, S=Unknown, C=US"
fi

# Align
$ANDROID_HOME/build-tools/35.0.0/zipalign -f -v 4 \
    "$APK_UNSIGNED" "$OUTPUT_DIR/aligned.apk"

# Sign
$ANDROID_HOME/build-tools/35.0.0/apksigner sign \
    --ks "$KEYSTORE" \
    --ks-key-alias kronos3d-dev \
    --ks-pass pass:kronos3d2024 \
    --key-pass pass:kronos3d2024 \
    --out "$APK_SIGNED" \
    "$OUTPUT_DIR/aligned.apk"

# Clean aligned temp file
rm -f "$OUTPUT_DIR/aligned.apk"

# Verify signature
echo "🔐 Verificando firma..."
$ANDROID_HOME/build-tools/35.0.0/apksigner verify --verbose "$APK_SIGNED"

# Final result
echo ""
echo "═══════════════════════════════════════════"
echo "✅ APK lista para descarga:"
echo "   📦 $APK_SIGNED"
echo "   📏 Tamaño: $(du -h $APK_SIGNED | cut -f1)"
echo "═══════════════════════════════════════════"
