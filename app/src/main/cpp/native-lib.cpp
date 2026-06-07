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

#include <imgui.h>
#include <backends/imgui_impl_android.h>
#include <backends/imgui_impl_opengl3.h>

// Camera/View parameters (Fase 2)
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
static float frame_time_ms = 0.0f;
static auto last_frame_time = std::chrono::high_resolution_clock::now();

static bool imgui_initialized = false;

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

static void kr_mesh_rebuild_buffers() {
    glBindBuffer(GL_ARRAY_BUFFER, cube_vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(KrVertex), mesh.vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.faces.size() * sizeof(KrFace), mesh.faces.data(), GL_STATIC_DRAW);
}

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

    // Init ImGui
    if (!imgui_initialized) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr; // Disable ini saving
        
        ImGui::StyleColorsDark();
        
        ImGui_ImplAndroid_Init(nullptr); // No native window required for basic rendering
        ImGui_ImplOpenGL3_Init("#version 300 es");
        imgui_initialized = true;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeResize(JNIEnv* env, jobject obj, jint width, jint height) {
    screen_width = width;
    screen_height = height;
    glViewport(0, 0, width, height);
    ImGui::GetIO().DisplaySize = ImVec2((float)width, (float)height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeRender(JNIEnv* env, jobject obj) {
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
    GLint model_loc = glGetUniformLocation(mesh_program, "u_Model");
    GLint cam_pos_loc = glGetUniformLocation(mesh_program, "u_CameraPos");

    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(model));
    glUniform3f(cam_pos_loc, camera_pos.x, camera_pos.y, camera_pos.z);

    glBindVertexArray(cube_vao);
    glDrawElements(GL_TRIANGLES, mesh.faces.size() * 3, GL_UNSIGNED_INT, 0);
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
        glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, (void*)(selected_face_id * 3 * sizeof(unsigned int)));
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
        // Draw using line loop or line strips of the triangles
        for (size_t i = 0; i < mesh.faces.size(); ++i) {
            glDrawElements(GL_LINE_LOOP, 3, GL_UNSIGNED_INT, (void*)(i * 3 * sizeof(unsigned int)));
        }

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

    // 3. Draw ImGui HUD overlay and toolbars
    if (imgui_initialized) {
        ImGui_ImplAndroid_NewFrame();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();

        // Debug logging for toolbar rendering
        __android_log_print(ANDROID_LOG_DEBUG, "Kronos3D", 
            "Drawing toolbar, screen: %dx%d", screen_width, screen_height);

        // Left Vertical Toolbar
        ImGui::SetNextWindowPos(ImVec2(10.0f, 100.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(80.0f, 320.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.8f);

        ImGuiIO& io = ImGui::GetIO();
        io.FontGlobalScale = 1.3f;

        ImGui::Begin("Tools", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings);

        // MODE SWITCH (EDIT/OBJ)
        if (current_edit_mode == OBJECT_MODE) {
            if (ImGui::Button("EDIT", ImVec2(60.0f, 45.0f))) {
                current_edit_mode = EDIT_MODE;
            }
        } else {
            if (ImGui::Button("OBJ", ImVec2(60.0f, 45.0f))) {
                current_edit_mode = OBJECT_MODE;
                selected_vertex_id = -1;
                selected_face_id = -1;
            }
        }
        ImGui::Separator();

        // TOOLBAR BUTTONS
        if (ImGui::Button("SEL", ImVec2(60.0f, 45.0f))) {
            current_tool_mode = TOOL_SELECT;
        }
        if (ImGui::Button("MOV", ImVec2(60.0f, 45.0f))) {
            current_tool_mode = TOOL_MOVE;
        }
        if (ImGui::Button("EXT", ImVec2(60.0f, 45.0f))) {
            if (selected_face_id != -1) {
                kr_mesh_extrude_face(mesh, selected_face_id, 0.5f);
                kr_mesh_rebuild_buffers();
                selected_face_id = -1; // reset selection
            }
        }
        if (ImGui::Button("SUB", ImVec2(60.0f, 45.0f))) {
            if (selected_face_id != -1) {
                kr_mesh_subdivide_face(mesh, selected_face_id);
                kr_mesh_rebuild_buffers();
                selected_face_id = -1; // reset selection
            }
        }
        ImGui::Separator();
        if (ImGui::Button("RST", ImVec2(60.0f, 45.0f))) {
            mesh = kr_mesh_create_cube();
            kr_mesh_rebuild_buffers();
            selected_face_id = -1;
            selected_vertex_id = -1;
        }

        ImGui::End();

        // Performance HUD (top left)
        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.6f);
        ImGui::SetNextWindowSize(ImVec2(150.0f, 110.0f), ImGuiCond_Always);
        
        ImGui::Begin("Performance HUD", nullptr, 
            ImGuiWindowFlags_NoTitleBar | 
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove | 
            ImGuiWindowFlags_NoCollapse | 
            ImGuiWindowFlags_NoSavedSettings);

        ImGui::Text("FPS: %.1f", current_fps);
        ImGui::Text("Ms: %.1f", frame_time_ms);
        ImGui::Text("Verts: %d", (int)mesh.vertices.size());
        ImGui::Text("Faces: %d", (int)mesh.faces.size());
        
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeOrbit(JNIEnv* env, jobject obj, jfloat dx, jfloat dy) {
    if (current_tool_mode == TOOL_MOVE && current_edit_mode == EDIT_MODE && selected_vertex_id != -1) {
        // Drag active selected vertex on horizontal XZ plane using dx and dy screen movement
        float scale = 0.005f * camera.distance;
        kr_mesh_move_vertex(mesh, selected_vertex_id, dx * scale, dy * scale);
        kr_mesh_rebuild_buffers();
    } else {
        // Touch drag sensitivity: orbit camera
        kr_camera_orbit(camera, dx * 0.15f, dy * 0.15f);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeZoom(JNIEnv* env, jobject obj, jfloat delta) {
    kr_camera_zoom(camera, delta);
}

// Tap selection logic JNI delegate
extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeTap(JNIEnv* env, jobject obj, jfloat normalized_x, jfloat normalized_y) {
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
