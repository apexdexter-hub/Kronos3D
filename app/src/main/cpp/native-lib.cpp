#include <jni.h>
#include <android/log.h>
#include <GLES3/gl32.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <chrono>
#include <cmath>
#include <thread>
#include <fstream>

#include "engine/mesh/mesh.h"
#include "engine/render/viewport.h"
#include "engine/render/shader.h"
#include "engine/render/overlay.h"
#include "engine/render/gizmo.h"

#define LOG_TAG "Kronos3D_Bridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

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
uniform vec3 u_CameraPos;
out vec4 fragColor;
void main() {
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));
    vec3 norm = normalize(v_Normal);
    vec3 baseColor = vec3(0.6, 0.6, 0.6);
    
    // Ambient
    vec3 ambient = 0.25 * baseColor;
    
    // Diffuse
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * baseColor;
    
    // Specular (Phong)
    vec3 viewDir = normalize(u_CameraPos - v_FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = 0.15 * vec3(1.0) * spec;
    
    fragColor = vec4(ambient + diffuse + specular, 1.0);
})";

const char* overlay_vert_src = R"(#version 300 es
layout(location = 0) in vec3 a_Position;
uniform mat4 u_MVP;
void main() {
    gl_Position = u_MVP * vec4(a_Position, 1.0);
    gl_PointSize = 8.0;
})";

const char* overlay_frag_src = R"(#version 300 es
precision mediump float;
uniform vec4 u_Color;
out vec4 fragColor;
void main() {
    fragColor = u_Color;
})";

// Camera/View parameters (Fase 2)
static KrCamera camera;
static KrMesh mesh;
static KrOverlay overlay;

static GLuint mesh_program = 0;
static GLuint overlay_program = 0;

static GLuint cube_vao = 0;
static GLuint cube_vbo = 0;
static GLuint cube_ebo = 0;
static GLuint wire_ebo = 0;

static int screen_width = 1280;
static int screen_height = 720;

// Stats metrics
static float current_fps = 0.0f;
static float frame_time_ms = 0.0f;
static auto last_frame_time = std::chrono::high_resolution_clock::now();

// Edit & Tool modes
static KrEditMode current_edit_mode = OBJECT_MODE;
static KrToolMode current_tool_mode = TOOL_SELECT;

// Selection status
static int selected_face_id = -1;
static int selected_vertex_id = -1;

// Grid wireframe shaders
static GLuint wire_program = 0;
static GLuint vtx_program = 0;

// Extra helper to reconstruct buffers on mesh change
static void kr_mesh_rebuild_buffers();

static std::vector<unsigned int> g_indices;
static std::vector<unsigned int> g_wire_indices;

static void kr_mesh_rebuild_buffers() {
    glBindBuffer(GL_ARRAY_BUFFER, cube_vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(KrVertex), mesh.vertices.data(), GL_STATIC_DRAW);

    g_indices.clear();
    g_wire_indices.clear();
    for (const auto& f : mesh.faces) {
        g_indices.push_back(f.v0);
        g_indices.push_back(f.v1);
        g_indices.push_back(f.v2);
        
        g_wire_indices.push_back(f.v0); g_wire_indices.push_back(f.v1);
        g_wire_indices.push_back(f.v1); g_wire_indices.push_back(f.v2);
        
        if (f.is_quad()) {
            g_indices.push_back(f.v0);
            g_indices.push_back(f.v2);
            g_indices.push_back(f.v3);
            
            g_wire_indices.push_back(f.v2); g_wire_indices.push_back(f.v3);
            g_wire_indices.push_back(f.v3); g_wire_indices.push_back(f.v0);
        } else {
            g_wire_indices.push_back(f.v2); g_wire_indices.push_back(f.v0);
        }
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, g_indices.size() * sizeof(unsigned int), g_indices.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wire_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, g_wire_indices.size() * sizeof(unsigned int), g_wire_indices.data(), GL_STATIC_DRAW);
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeInit(JNIEnv* env, jobject obj) {
    LOGI("nativeInit: Initializing OpenGL viewport and resources (Fase 2)");
    glClearColor(0.24f, 0.24f, 0.24f, 1.0f); // Blender default viewport color
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    kr_camera_init(camera);
    mesh = kr_mesh_create_cube();
    overlay.init();

    mesh_program = kr_program_create(mesh_vert_src, mesh_frag_src);
    overlay_program = kr_program_create(overlay_vert_src, overlay_frag_src);

    // Setup Cube mesh buffers
    glGenVertexArrays(1, &cube_vao);
    glGenBuffers(1, &cube_vbo);
    glGenBuffers(1, &cube_ebo);
    glGenBuffers(1, &wire_ebo);

    glBindVertexArray(cube_vao);

    kr_mesh_rebuild_buffers();

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
    auto frame_start = std::chrono::high_resolution_clock::now();

    // Measure FPS
    auto current_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> delta = current_time - last_frame_time;
    last_frame_time = current_time;
    frame_time_ms = delta.count() * 1000.0f;
    current_fps = (delta.count() > 0.0f) ? (1.0f / delta.count()) : 60.0f;

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
    GLint light_pos_loc = glGetUniformLocation(mesh_program, "u_LightPos");
    GLint cam_pos_loc = glGetUniformLocation(mesh_program, "u_CameraPos");

    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform3f(light_pos_loc, 10.0f, 20.0f, 10.0f);
    glUniform3f(cam_pos_loc, camera_pos.x, camera_pos.y, camera_pos.z);

    glBindVertexArray(cube_vao);
    // Explicitly bind the correct index buffer to fix initial mangled shape
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube_ebo);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    glDrawElements(GL_TRIANGLES, g_indices.size(), GL_UNSIGNED_INT, 0);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindVertexArray(0);

    // Draw Selected Face highlight if applicable
    if (selected_face_id != -1 && selected_face_id < (int)mesh.faces.size()) {
        glUseProgram(overlay_program);
        GLint overlay_mvp_loc = glGetUniformLocation(overlay_program, "u_MVP");
        GLint overlay_color_loc = glGetUniformLocation(overlay_program, "u_Color");
        
        glUniformMatrix4fv(overlay_mvp_loc, 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform4f(overlay_color_loc, 1.0f, 0.8f, 0.0f, 1.0f); // Yellow highlight

        glBindVertexArray(cube_vao);
        // Render only the selected face indices
        int idx_offset = 0;
        int count = 0;
        for (int i = 0; i < selected_face_id; ++i) {
            idx_offset += mesh.faces[i].is_quad() ? 6 : 3;
        }
        count = mesh.faces[selected_face_id].is_quad() ? 6 : 3;
        
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, (void*)(idx_offset * sizeof(unsigned int)));
        glBindVertexArray(0);
    }

    // Draw Edit Mode overlays (Vertices and Edges)
    if (current_edit_mode == EDIT_MODE) {
        glUseProgram(overlay_program);
        GLint overlay_mvp_loc = glGetUniformLocation(overlay_program, "u_MVP");
        GLint overlay_color_loc = glGetUniformLocation(overlay_program, "u_Color");
        glUniformMatrix4fv(overlay_mvp_loc, 1, GL_FALSE, glm::value_ptr(mvp));

        glBindVertexArray(cube_vao);

        // 1. Draw Edges as wireframe lines (White)
        glUniform4f(overlay_color_loc, 1.0f, 1.0f, 1.0f, 0.8f);
        // Draw using line list without diagonals
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wire_ebo);
        glDrawElements(GL_LINES, g_wire_indices.size(), GL_UNSIGNED_INT, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube_ebo); // restore normal index buffer

        // 2. Draw Vertices as white points (White)
        glUniform4f(overlay_color_loc, 1.0f, 1.0f, 1.0f, 1.0f);
        glDrawArrays(GL_POINTS, 0, mesh.vertices.size());

        // Highlight selected vertex if TOOL_MOVE is active
        if (selected_vertex_id != -1 && selected_vertex_id < (int)mesh.vertices.size()) {
            glUniform4f(overlay_color_loc, 1.0f, 0.5f, 0.0f, 1.0f); // Orange vertex point
            glDrawArrays(GL_POINTS, selected_vertex_id, 1);
        }

        glBindVertexArray(0);
    }

    // Render 3D Translation Gizmo if we have selection
    if (selected_face_id != -1 || selected_vertex_id != -1) {
        kr_gizmo_render(mesh, selected_vertex_id, selected_face_id, current_edit_mode, mvp, overlay_program);
    }

    // Limit to 60 FPS maximum
    const auto targetFrameTime = std::chrono::milliseconds(16);
    auto frameEnd = frame_start + targetFrameTime;
    std::this_thread::sleep_until(frameEnd);
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeOrbit(JNIEnv* env, jobject obj, jfloat dx, jfloat dy) {
    // Attempt dragging via Gizmo translation first
    bool dragged = kr_gizmo_drag(mesh, selected_vertex_id, selected_face_id, current_edit_mode, dx, dy, screen_width, screen_height, camera.distance, camera.azimuth, camera.elevation);
    if (!dragged) {
        if (current_tool_mode == TOOL_MOVE && current_edit_mode == EDIT_MODE && selected_vertex_id != -1) {
            // Fallback: Drag active selected vertex on horizontal XZ plane using dx and dy screen movement
            float scale = 0.005f * camera.distance;
            kr_mesh_move_vertex(mesh, selected_vertex_id, dx * scale, dy * scale);
            kr_mesh_rebuild_buffers();
        } else {
            // Touch drag sensitivity: orbit camera
            kr_camera_orbit(camera, dx * 0.15f, dy * 0.15f);
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeZoom(JNIEnv* env, jobject obj, jfloat delta) {
    kr_camera_zoom(camera, delta);
}

// Tap selection logic JNI delegate
extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeTap(JNIEnv* env, jobject obj, jfloat pixel_x, jfloat pixel_y) {
    // Convert click coordinates to NDC space
    float normalized_x = (2.0f * pixel_x / (float)screen_width) - 1.0f;
    float normalized_y = 1.0f - (2.0f * pixel_y / (float)screen_height);

    // 1. Calculate raycast in world space from camera position
    float aspect = (float)screen_width / (float)screen_height;
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    
    float rad_az = glm::radians(camera.azimuth);
    float rad_el = glm::radians(camera.elevation);
    glm::vec3 camera_pos;
    camera_pos.x = camera.distance * cos(rad_el) * sin(rad_az);
    camera_pos.y = camera.distance * sin(rad_el);
    camera_pos.z = camera.distance * cos(rad_el) * cos(rad_az);
    
    glm::mat4 view = glm::lookAt(camera_pos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 model = glm::mat4(1.0f);
    
    // Convert click coordinates to clip space
    glm::vec4 ray_clip = glm::vec4(normalized_x, normalized_y, -1.0f, 1.0f);
    glm::vec4 ray_eye = glm::inverse(projection) * ray_clip;
    ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);
    glm::vec3 ray_world = glm::normalize(glm::vec3(glm::inverse(view) * ray_eye));

    // Convert ray details to floats
    float origin_arr[3] = { camera_pos.x, camera_pos.y, camera_pos.z };
    float dir_arr[3] = { ray_world.x, ray_world.y, ray_world.z };
    
    float model_arr[16];
    float view_arr[16];
    float proj_arr[16];
    memcpy(model_arr, glm::value_ptr(model), 16 * sizeof(float));
    memcpy(view_arr, glm::value_ptr(view), 16 * sizeof(float));
    memcpy(proj_arr, glm::value_ptr(projection), 16 * sizeof(float));

    if (current_edit_mode == EDIT_MODE) {
        // Try selecting closest vertex
        float min_dist = std::numeric_limits<float>::max();
        int closest_vertex = -1;
        
        for (size_t i = 0; i < mesh.vertices.size(); ++i) {
            glm::vec3 v_world = glm::vec3(model * glm::vec4(mesh.vertices[i].x, mesh.vertices[i].y, mesh.vertices[i].z, 1.0f));
            // Point to line distance
            glm::vec3 cross_prod = glm::cross(v_world - camera_pos, ray_world);
            float dist = glm::length(cross_prod);
            if (dist < min_dist && dist < 0.2f) { // Selection radius threshold
                min_dist = dist;
                closest_vertex = (int)i;
            }
        }
        selected_vertex_id = closest_vertex;
    } else {
        // Select face in OBJECT_MODE
        selected_face_id = kr_mesh_raycast(mesh, model_arr, view_arr, proj_arr, origin_arr, dir_arr);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeGizmoDown(JNIEnv* env, jobject obj, jfloat x, jfloat y) {
    if (selected_face_id == -1 && selected_vertex_id == -1) return;
    
    float aspect = (float)screen_width / (float)screen_height;
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    float rad_az = glm::radians(camera.azimuth);
    float rad_el = glm::radians(camera.elevation);
    glm::vec3 camera_pos;
    camera_pos.x = camera.distance * cos(rad_el) * sin(rad_az);
    camera_pos.y = camera.distance * sin(rad_el);
    camera_pos.z = camera.distance * cos(rad_el) * cos(rad_az);
    glm::mat4 view = glm::lookAt(camera_pos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 mvp = projection * view * model;

    kr_gizmo_down(mesh, selected_vertex_id, selected_face_id, current_edit_mode, x, y, screen_width, screen_height, mvp, camera.distance, camera.azimuth, camera.elevation);
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeGizmoUp(JNIEnv* env, jobject obj) {
    g_active_gizmo_axis = -1;
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeToggleEditMode(JNIEnv* env, jobject obj) {
    if (current_edit_mode == OBJECT_MODE) {
        current_edit_mode = EDIT_MODE;
    } else {
        current_edit_mode = OBJECT_MODE;
        selected_vertex_id = -1;
        selected_face_id = -1;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeSetToolSelect(JNIEnv* env, jobject obj) {
    current_tool_mode = TOOL_SELECT;
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeSetToolMove(JNIEnv* env, jobject obj) {
    current_tool_mode = TOOL_MOVE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeExtrude(JNIEnv* env, jobject obj) {
    if (current_edit_mode == EDIT_MODE && selected_face_id != -1) {
        kr_mesh_extrude_face(mesh, selected_face_id, 1.0f);
        kr_mesh_rebuild_buffers();
        // Keep the extruded face selected so the user can immediately drag it with the Gizmo
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeSubdivide(JNIEnv* env, jobject obj) {
    if (selected_face_id != -1) {
        kr_mesh_subdivide_face(mesh, selected_face_id);
        kr_mesh_rebuild_buffers();
        // Keep selected to allow further manipulation
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeResetMesh(JNIEnv* env, jobject obj) {
    mesh = kr_mesh_create_cube();
    kr_mesh_rebuild_buffers();
    selected_face_id = -1;
    selected_vertex_id = -1;
}

extern "C" JNIEXPORT jfloat JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeGetFPS(JNIEnv* env, jobject obj) {
    return current_fps;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeGetVertCount(JNIEnv* env, jobject obj) {
    return (jint)mesh.vertices.size();
}

extern "C" JNIEXPORT jint JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeGetFaceCount(JNIEnv* env, jobject obj) {
    return (jint)mesh.faces.size();
}

const char* autosave_path = "/data/data/com.kronos3d/files/autosave.mesh";

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeSaveMesh(JNIEnv* env, jobject obj) {
    LOGI("Saving mesh to %s", autosave_path);
    std::ofstream out(autosave_path, std::ios::binary);
    if (!out) {
        LOGE("Failed to open autosave file for writing");
        return;
    }
    
    size_t vert_count = mesh.vertices.size();
    out.write(reinterpret_cast<const char*>(&vert_count), sizeof(vert_count));
    out.write(reinterpret_cast<const char*>(mesh.vertices.data()), vert_count * sizeof(KrVertex));
    
    size_t face_count = mesh.faces.size();
    out.write(reinterpret_cast<const char*>(&face_count), sizeof(face_count));
    out.write(reinterpret_cast<const char*>(mesh.faces.data()), face_count * sizeof(KrFace));
    
    out.close();
    LOGI("Mesh saved successfully");
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeLoadMesh(JNIEnv* env, jobject obj) {
    LOGI("Loading mesh from %s", autosave_path);
    std::ifstream in(autosave_path, std::ios::binary);
    if (!in) {
        LOGI("No autosave file found, starting with default cube");
        return;
    }
    
    size_t vert_count = 0;
    in.read(reinterpret_cast<char*>(&vert_count), sizeof(vert_count));
    if (vert_count == 0) return;
    mesh.vertices.resize(vert_count);
    in.read(reinterpret_cast<char*>(mesh.vertices.data()), vert_count * sizeof(KrVertex));
    
    size_t face_count = 0;
    in.read(reinterpret_cast<char*>(&face_count), sizeof(face_count));
    if (face_count == 0) return;
    
    // Quick validation: if the mesh has 12 faces but 24 vertices, it's the old triangle-only bugged cube
    if (vert_count == 24 && face_count == 12) {
        LOGI("Detected old bugged triangle mesh. Discarding load to prevent deformities.");
        in.close();
        mesh = kr_mesh_create_cube();
        kr_mesh_rebuild_buffers();
        return;
    }
    
    mesh.faces.resize(face_count);
    in.read(reinterpret_cast<char*>(mesh.faces.data()), face_count * sizeof(KrFace));
    
    in.close();
    LOGI("Mesh loaded successfully");
    
    // Rebuild GPU buffers
    kr_mesh_rebuild_buffers();
}

