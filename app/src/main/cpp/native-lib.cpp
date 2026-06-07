#include <jni.h>
#include <android/log.h>
#include <GLES3/gl32.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <chrono>
#include <cmath>

#include "engine/mesh/mesh.h"
#include "engine/render/viewport.h"
#include "engine/render/shader.h"
#include "engine/render/overlay.h"

#define LOG_TAG "Kronos3D_Bridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// Shaders sources inline
const char* mesh_vert_src = R"(#version 300 es
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
uniform mat4 u_MVP;
uniform mat4 u_Model;
out vec3 v_Normal;
out vec3 v_FragPos;
void main() {
    v_FragPos = vec3(u_Model * vec4(a_Position, 1.0));
    v_Normal = mat3(transpose(inverse(u_Model))) * a_Normal;
    gl_Position = u_MVP * vec4(a_Position, 1.0);
})";

const char* mesh_frag_src = R"(#version 300 es
precision mediump float;
in vec3 v_Normal;
in vec3 v_FragPos;
out vec4 fragColor;
void main() {
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));
    vec3 norm = normalize(v_Normal);
    float ambient = 0.3;
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 baseColor = vec3(0.7, 0.7, 0.7);
    vec3 result = baseColor * (ambient + diff);
    fragColor = vec4(result, 1.0);
})";

const char* overlay_vert_src = R"(#version 300 es
layout(location = 0) in vec3 a_Position;
uniform mat4 u_MVP;
void main() {
    gl_Position = u_MVP * vec4(a_Position, 1.0);
})";

const char* overlay_frag_src = R"(#version 300 es
precision mediump float;
uniform vec4 u_Color;
out vec4 fragColor;
void main() {
    fragColor = u_Color;
})";

// Render resources
static KrCamera camera;
static KrMesh mesh;
static KrOverlay overlay;

static GLuint mesh_program = 0;
static GLuint overlay_program = 0;

static GLuint cube_vao = 0;
static GLuint cube_vbo = 0;
static GLuint cube_ebo = 0;

static int screen_width = 1280;
static int screen_height = 720;

// Stats metrics
static float current_fps = 0.0f;
static auto last_frame_time = std::chrono::high_resolution_clock::now();

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeInit(JNIEnv* env, jobject obj) {
    LOGI("nativeInit: Initializing OpenGL viewport and resources (Fase 2)");
    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    kr_camera_init(camera);
    mesh = kr_mesh_create_cube();
    overlay.init();

    mesh_program = kr_program_create(mesh_vert_src, mesh_frag_src);
    overlay_program = kr_program_create(overlay_vert_src, overlay_frag_src);

    // Setup Cube mesh buffers
    glGenVertexArrays(1, &cube_vao);
    glGenBuffers(1, &cube_vbo);
    glGenBuffers(1, &cube_ebo);

    glBindVertexArray(cube_vao);

    // Bind VBO
    glBindBuffer(GL_ARRAY_BUFFER, cube_vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(KrVertex), mesh.vertices.data(), GL_STATIC_DRAW);

    // Bind EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.faces.size() * sizeof(KrFace), mesh.faces.data(), GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(KrVertex), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(KrVertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeResize(JNIEnv* env, jobject obj, jint width, jint height) {
    screen_width = width;
    screen_height = height;
    glViewport(0, 0, width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeRender(JNIEnv* env, jobject obj) {
    // Measure FPS
    auto current_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> delta = current_time - last_frame_time;
    last_frame_time = current_time;
    current_fps = 1.0f / delta.count();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Calculate Matrices using GLM
    float aspect = (float)screen_width / (float)screen_height;
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

    // Orbital camera calculation
    float rad_az = glm::radians(camera.azimuth);
    float rad_el = glm::radians(camera.elevation);
    
    glm::vec3 camera_pos;
    camera_pos.x = camera.distance * cos(rad_el) * sin(rad_az);
    camera_pos.y = camera.distance * sin(rad_el);
    camera_pos.z = camera.distance * cos(rad_el) * cos(rad_az);

    glm::mat4 view = glm::lookAt(camera_pos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 mvp = projection * view * model;

    // 1. Draw Grid Overlay
    overlay.render(overlay_program, glm::value_ptr(mvp));

    // 2. Draw Phong Lit Cube
    glUseProgram(mesh_program);
    GLint mvp_loc = glGetUniformLocation(mesh_program, "u_MVP");
    GLint model_loc = glGetUniformLocation(mesh_program, "u_Model");

    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(model));

    glBindVertexArray(cube_vao);
    glDrawElements(GL_TRIANGLES, mesh.faces.size() * 3, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // Logging stats or debugging
    // HUD overlay display (Fase 2 UI integration will render stats onto overlay_ui)
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeOrbit(JNIEnv* env, jobject obj, jfloat dx, jfloat dy) {
    // Touch drag sensitivity: 0.003 rad/pixel translate to degrees
    kr_camera_orbit(camera, dx * 0.15f, dy * 0.15f);
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeZoom(JNIEnv* env, jobject obj, jfloat delta) {
    kr_camera_zoom(camera, delta);
}
