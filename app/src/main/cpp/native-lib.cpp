#include <jni.h>
#include <android/log.h>
#include <GLES3/gl32.h>

#define LOG_TAG "Kronos3D_Bridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// Camera/View parameters (Fase 2)
static float camera_azimuth = 45.0f;
static float camera_elevation = 30.0f;
static float camera_distance = 5.0f;
static int screen_width = 1280;
static int screen_height = 720;

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeInit(JNIEnv* env, jobject obj) {
    LOGI("nativeInit: Initializing OpenGL state (Fase 1)");
    // Clear screen to black
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeResize(JNIEnv* env, jobject obj, jint width, jint height) {
    LOGI("nativeResize: %d x %d", width, height);
    screen_width = width;
    screen_height = height;
    glViewport(0, 0, width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeRender(JNIEnv* env, jobject obj) {
    // Stage 1 default render: black screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeOrbit(JNIEnv* env, jobject obj, jfloat dx, jfloat dy) {
    camera_azimuth += dx * 0.25f;
    camera_elevation += dy * 0.25f;
    if (camera_elevation > 89.0f) camera_elevation = 89.0f;
    if (camera_elevation < -89.0f) camera_elevation = -89.0f;
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeZoom(JNIEnv* env, jobject obj, jfloat delta) {
    camera_distance -= delta;
    if (camera_distance < 0.5f) camera_distance = 0.5f;
    if (camera_distance > 100.0f) camera_distance = 100.0f;
}
