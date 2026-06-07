# Agent Context - Kronos3D Reconstruction

This file provides architectural state, environmental configurations, and compiler solutions mapped during reconstruction.

## NDK Mismatch Resolution
- The environment initially loaded Android NDK `26.1.10909125` but lacked a `source.properties` file causing compilation failure.
- Overrode NDK location explicitly by defining `ndkVersion = "27.2.12479018"` inside `app/build.gradle.kts` to target the fully configured SDK folder in the home directory.

## JVM 25 Parser Mismatch
- The workspace default JDK was upgraded to `25.0.2`.
- The Kotlin compiler plugin (version `1.9.23`) in combination with Gradle wrapper failed to parse Java Version `25` classes, resulting in `java.lang.IllegalArgumentException: 25.0.2`.
- Mitigated this issue by locking `release.sh` compile steps under explicit `JAVA_HOME` pointing to JVM 17: `/usr/lib/jvm/java-1.17.0-openjdk-amd64`.

## Touch handler
- Left pointer pinch gesture zoom as technical debt due to device-specific Gestures anomalies.
- Handled touch events via `KronosGLSurfaceView.kt` passing delta modifications to Orbit camera via JNI bridges.
