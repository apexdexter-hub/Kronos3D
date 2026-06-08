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
out vec3 v_Normal;
out vec3 v_FragPos;
void main() {
    v_FragPos = a_Position;
    v_Normal = a_Normal;
    gl_Position = u_MVP * vec4(a_Position, 1.0);
})";

const char* mesh_frag_src = R"(#version 300 es
precision mediump float;
in vec3 v_Normal;
in vec3 v_FragPos;

uniform vec3 u_CameraPos;
uniform vec3 u_LightPos;
uniform vec3 u_LightColor;

out vec4 FragColor;

const float PI = 3.14159265359;

// GGX Normal Distribution Function
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return num / max(denom, 0.0000001);
}

// Schlick-GGX Geometry function
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return num / max(denom, 0.0000001);
}

// Smith Geometry function
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

// Fresnel Schlick approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec3 N = normalize(v_Normal);
    vec3 V = normalize(u_CameraPos - v_FragPos);
    
    // PBR Material Properties (Nomad Sculpt Default Lit look: Sleek clay silver)
    vec3 albedo = vec3(0.74, 0.76, 0.78);
    float metallic = 0.55;
    float roughness = 0.38;
    float ao = 1.0;
    
    // Lights configuration (Warm key light + Cool fill light)
    vec3 lightPos1 = u_LightPos;
    vec3 lightColor1 = u_LightColor;
    
    vec3 lightPos2 = vec3(-4.0, -2.0, -4.0);
    vec3 lightColor2 = vec3(0.25, 0.35, 0.45);
    
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);
    
    vec3 Lo = vec3(0.0);
    
    // Light 1 (Key Light)
    {
        vec3 L = normalize(lightPos1 - v_FragPos);
        vec3 H = normalize(V + L);
        
        float D = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F  = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
        vec3 numerator     = D * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular     = numerator / denominator;
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        
        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * lightColor1 * NdotL;
    }
    
    // Light 2 (Fill Light)
    {
        vec3 L = normalize(lightPos2 - v_FragPos);
        vec3 H = normalize(V + L);
        
        float D = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F  = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
        vec3 numerator     = D * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular     = numerator / denominator;
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        
        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * lightColor2 * NdotL;
    }
    
    vec3 ambient = vec3(0.06) * albedo * ao;
    vec3 color = ambient + Lo;
    
    // Tone mapping & Gamma Correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));
    
    FragColor = vec4(color, 1.0);
})";

const char* overlay_vert_src = R"(#version 300 es
layout(location = 0) in vec3 a_Position;
uniform mat4 u_MVP;
void main() {
    gl_Position = u_MVP * vec4(a_Position, 1.0);
    gl_PointSize = 18.0;
})";

const char* overlay_frag_src = R"(#version 300 es
precision mediump float;
uniform vec4 u_Color;
out vec4 fragColor;
void main() {
    fragColor = u_Color;
})";

const char* vtx_vert_src = R"(#version 300 es
layout(location = 0) in vec3 a_Position;
uniform mat4 u_MVP;
void main() {
    gl_Position = u_MVP * vec4(a_Position, 1.0);
    gl_PointSize = 18.0;
})";

const char* vtx_frag_src = R"(#version 300 es
precision mediump float;
uniform vec4 u_Color;
out vec4 fragColor;
void main() {
    float r = length(gl_PointCoord - vec2(0.5));
    if (r > 0.5) {
        discard;
    }
    vec3 col = u_Color.rgb;
    if (r > 0.38) {
        col = vec3(0.1, 0.1, 0.1); // subtle dark border
    }
    float alpha = smoothstep(0.5, 0.43, r);
    fragColor = vec4(col, u_Color.a * alpha);
})";

// Light Ball global state (extern in gizmo.h)
glm::vec3 g_light_pos = glm::vec3(3.0f, 4.0f, 3.0f);
bool g_light_selected = false;
glm::vec3 g_light_color = glm::vec3(1.0f, 1.0f, 0.9f); // warm yellow

// Camera/View parameters (Fase 2)
static KrCamera camera;
static KrMesh mesh;
static KrOverlay overlay;

static GLuint mesh_program = 0;
static GLuint overlay_program = 0;
static GLuint vtx_program = 0;

static GLuint cube_vao = 0;
static GLuint cube_vbo = 0;
static GLuint cube_ebo = 0;
static GLuint wire_vao = 0;
static GLuint wire_vbo = 0;
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
static std::atomic<bool> g_NeedGPUUpdate{false};

// Grid wireframe shaders
static GLuint wire_program = 0;

// Render buffers
static std::vector<float> g_render_buffer;
static std::vector<unsigned int> g_wire_indices;

// Undo history stack
static std::vector<KrMesh> g_undo_stack;

static void kr_save_undo_state() {
    if (g_undo_stack.size() > 30) {
        g_undo_stack.erase(g_undo_stack.begin()); // Limit undo levels
    }
    g_undo_stack.push_back(mesh);
}

static void kr_mesh_rebuild_buffers() {
    g_render_buffer = kr_mesh_to_render_buffer(mesh);
    glBindBuffer(GL_ARRAY_BUFFER, cube_vbo);
    glBufferData(GL_ARRAY_BUFFER, g_render_buffer.size() * sizeof(float), g_render_buffer.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, wire_vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(KrVertex), mesh.vertices.data(), GL_DYNAMIC_DRAW);

    g_wire_indices.clear();
    for (const auto& e : mesh.edges) {
        if(e.flag != 1) {
            g_wire_indices.push_back(e.v1);
            g_wire_indices.push_back(e.v2);
        }
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wire_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, g_wire_indices.size() * sizeof(unsigned int), g_wire_indices.data(), GL_DYNAMIC_DRAW);
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
    vtx_program = kr_program_create(vtx_vert_src, vtx_frag_src);

    // Setup Cube mesh buffers
    glGenVertexArrays(1, &cube_vao);
    glGenBuffers(1, &cube_vbo);
    glGenBuffers(1, &cube_ebo);
    glGenVertexArrays(1, &wire_vao);
    glGenBuffers(1, &wire_vbo);
    glGenBuffers(1, &wire_ebo);

    kr_mesh_rebuild_buffers();

    glBindVertexArray(cube_vao);
    glBindBuffer(GL_ARRAY_BUFFER, cube_vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 24, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    glBindVertexArray(wire_vao);
    glBindBuffer(GL_ARRAY_BUFFER, wire_vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(KrVertex), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wire_ebo);
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
    if (mesh_program == 0 || overlay_program == 0 || vtx_program == 0) {
        LOGE("nativeRender: Shaders failed compilation. Skipping render to prevent GPU driver crash.");
        return;
    }

    if (g_NeedGPUUpdate) {
        kr_mesh_rebuild_buffers();
        g_NeedGPUUpdate = false;
    }
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
    GLint light_col_loc = glGetUniformLocation(mesh_program, "u_LightColor");
    GLint cam_pos_loc = glGetUniformLocation(mesh_program, "u_CameraPos");

    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform3f(light_pos_loc, g_light_pos.x, g_light_pos.y, g_light_pos.z);
    glUniform3f(light_col_loc, g_light_color.r, g_light_color.g, g_light_color.b);
    glUniform3f(cam_pos_loc, camera_pos.x, camera_pos.y, camera_pos.z);

    glBindVertexArray(cube_vao);
    // Explicitly bind the correct index buffer to fix initial mangled shape
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube_ebo);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, g_render_buffer.size() / 6);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindVertexArray(0);

    // 2b. Draw Light Ball (represented as a bright glowing point using vtx_program)
    glUseProgram(vtx_program);
    GLint vtx_mvp_loc = glGetUniformLocation(vtx_program, "u_MVP");
    GLint vtx_color_loc = glGetUniformLocation(vtx_program, "u_Color");
    glUniformMatrix4fv(vtx_mvp_loc, 1, GL_FALSE, glm::value_ptr(mvp));

    glBindVertexArray(wire_vao);
    glBindBuffer(GL_ARRAY_BUFFER, wire_vbo);
    float l_pos[] = { g_light_pos.x, g_light_pos.y, g_light_pos.z };
    glBufferData(GL_ARRAY_BUFFER, sizeof(l_pos), l_pos, GL_STREAM_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    if (g_light_selected) {
        glUniform4f(vtx_color_loc, 1.0f, 0.6f, 0.0f, 1.0f); // Selected: orange glow
    } else {
        glUniform4f(vtx_color_loc, g_light_color.r, g_light_color.g, g_light_color.b, 1.0f); // Warm light color
    }
    glDrawArrays(GL_POINTS, 0, 1);

    // Restore wire_vao buffers so we don't break subsequent rendering
    glBindBuffer(GL_ARRAY_BUFFER, wire_vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(KrVertex), mesh.vertices.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(KrVertex), (void*)0);
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
            idx_offset += mesh.faces[i].len == 4 ? 6 : 3;
        }
        count = mesh.faces[selected_face_id].len == 4 ? 6 : 3;
        
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, (void*)(idx_offset * sizeof(unsigned int)));
        glBindVertexArray(0);
    }

    // Draw Edit Mode overlays (Vertices and Edges)
    if (current_edit_mode == EDIT_MODE) {
        glUseProgram(overlay_program);
        GLint overlay_mvp_loc = glGetUniformLocation(overlay_program, "u_MVP");
        GLint overlay_color_loc = glGetUniformLocation(overlay_program, "u_Color");
        glUniformMatrix4fv(overlay_mvp_loc, 1, GL_FALSE, glm::value_ptr(mvp));

        glBindVertexArray(wire_vao);

        // 1. Draw Edges as wireframe lines (White)
        glLineWidth(3.0f);
        glUniform4f(overlay_color_loc, 1.0f, 1.0f, 1.0f, 0.8f);
        glDrawElements(GL_LINES, g_wire_indices.size(), GL_UNSIGNED_INT, 0);
        glLineWidth(1.0f);

        // 2. Draw Vertices as white points (White)
        glUseProgram(vtx_program);
        GLint vtx_mvp_loc = glGetUniformLocation(vtx_program, "u_MVP");
        GLint vtx_color_loc = glGetUniformLocation(vtx_program, "u_Color");
        glUniformMatrix4fv(vtx_mvp_loc, 1, GL_FALSE, glm::value_ptr(mvp));

        glUniform4f(vtx_color_loc, 1.0f, 1.0f, 1.0f, 1.0f);
        glDrawArrays(GL_POINTS, 0, mesh.vertices.size());

        // Highlight selected vertices (those with flag == 1)
        for (size_t i = 0; i < mesh.vertices.size(); ++i) {
            if (mesh.vertices[i].flag == 1) {
                glUniform4f(vtx_color_loc, 1.0f, 0.5f, 0.0f, 1.0f); // Orange selected point
                glDrawArrays(GL_POINTS, i, 1);
            }
        }

        glBindVertexArray(0);
    }

    // Render 3D Translation/Rotation/Scale Gizmo if selected_vertex_id, selected_face_id, or object vertices are active
    kr_gizmo_render(mesh, selected_vertex_id, selected_face_id, current_edit_mode, current_tool_mode, mvp, overlay_program, camera.distance);

    // Limit to 60 FPS maximum
    const auto targetFrameTime = std::chrono::milliseconds(16);
    auto frameEnd = frame_start + targetFrameTime;
    std::this_thread::sleep_until(frameEnd);
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeOrbit(JNIEnv* env, jobject obj, jfloat dx, jfloat dy) {
    // Attempt dragging via Gizmo first
    bool dragged = kr_gizmo_drag(mesh, selected_vertex_id, selected_face_id, current_edit_mode, current_tool_mode, dx, dy, screen_width, screen_height, camera.distance, camera.azimuth, camera.elevation);
    if (dragged) {
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
    glm::mat4 mvp = projection * view * model;
    
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

    // Check if user tapped the Light Ball first! (screen-space check)
    glm::vec2 l_screen = project_to_screen(g_light_pos, mvp, screen_width, screen_height);
    float l_dist = glm::distance(l_screen, glm::vec2(pixel_x, pixel_y));
    
    if (l_dist < 50.0f) { // 50px tap radius threshold
        g_light_selected = true;
        selected_vertex_id = -1;
        selected_face_id = -1;
        for (auto& v : mesh.vertices) {
            v.flag = 0;
        }
        return;
    }
    g_light_selected = false; // deselect light if tapped elsewhere

    if (current_edit_mode == EDIT_MODE) {
        // Try selecting closest vertex (mathematically precise screen-space check)
        float min_dist_px = std::numeric_limits<float>::max();
        int closest_vertex = -1;
        
        for (size_t i = 0; i < mesh.vertices.size(); ++i) {
            glm::vec3 v_world = glm::vec3(model * glm::vec4(mesh.vertices[i].co[0], mesh.vertices[i].co[1], mesh.vertices[i].co[2], 1.0f));
            glm::vec2 s_pos = project_to_screen(v_world, mvp, screen_width, screen_height);
            float dist = glm::distance(s_pos, glm::vec2(pixel_x, pixel_y));
            if (dist < min_dist_px && dist < 45.0f) { // 45px radius threshold
                min_dist_px = dist;
                closest_vertex = (int)i;
            }
        }
        
        if (closest_vertex != -1) {
            // Toggle selection flag on this vertex (1 = selected, 0 = unselected)
            mesh.vertices[closest_vertex].flag = (mesh.vertices[closest_vertex].flag == 1) ? 0 : 1;
            selected_vertex_id = closest_vertex;
            selected_face_id = -1; // clear face selection
        } else {
            // Fallback: try selecting a face inside EDIT_MODE
            int face_id = kr_mesh_raycast(mesh, model_arr, view_arr, proj_arr, origin_arr, dir_arr);
            if (face_id != -1) {
                selected_face_id = face_id;
                // Clear vertex flags when a face is selected
                for (auto& v : mesh.vertices) {
                    v.flag = 0;
                }
                selected_vertex_id = -1;
            } else {
                // Touched empty space: clear all selections
                for (auto& v : mesh.vertices) {
                    v.flag = 0;
                }
                selected_vertex_id = -1;
                selected_face_id = -1;
            }
        }
    } else {
        // Select face in OBJECT_MODE
        selected_face_id = kr_mesh_raycast(mesh, model_arr, view_arr, proj_arr, origin_arr, dir_arr);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeGizmoDown(JNIEnv* env, jobject obj, jfloat x, jfloat y) {
    kr_save_undo_state();
    bool has_selection = false;
    for (const auto& v : mesh.vertices) {
        if (v.flag == 1) {
            has_selection = true;
            break;
        }
    }
    if (selected_face_id == -1 && selected_vertex_id == -1 && !has_selection && !g_light_selected) return;
    
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

    kr_gizmo_down(mesh, selected_vertex_id, selected_face_id, current_edit_mode, current_tool_mode, x, y, screen_width, screen_height, mvp, camera.distance, camera.azimuth, camera.elevation);
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeGizmoUp(JNIEnv* env, jobject obj) {
    g_active_gizmo_axis = -1;
}

static std::atomic<bool> g_MeshDirty{false};

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeToggleEditMode(JNIEnv* env, jobject obj) {
    if (g_MeshDirty) return;
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
    g_active_gizmo_axis = -1;
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeSetToolMove(JNIEnv* env, jobject obj) {
    current_tool_mode = TOOL_MOVE;
    g_active_gizmo_axis = -1;
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeSetToolRotate(JNIEnv* env, jobject obj) {
    current_tool_mode = TOOL_ROTATE;
    g_active_gizmo_axis = -1;
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeSetToolScale(JNIEnv* env, jobject obj) {
    current_tool_mode = TOOL_SCALE;
    g_active_gizmo_axis = -1;
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeExtrude(JNIEnv* env, jobject obj) {
    if (current_edit_mode == EDIT_MODE && selected_face_id != -1) {
        kr_save_undo_state();
        g_MeshDirty = true;
        kr_mesh_extrude_face(mesh, selected_face_id, 1.0f);
        if ((int)mesh.faces.size() >= 5) {
            selected_face_id = (int)mesh.faces.size() - 5;
        }
        kr_mesh_rebuild_buffers();
        g_MeshDirty = false;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeSubdivide(JNIEnv* env, jobject obj) {
    if (current_edit_mode == EDIT_MODE) {
        if (g_MeshDirty) return;
        kr_save_undo_state();
        g_MeshDirty = true;
        std::thread([]() {
            kr_mesh_subdivide_all(mesh);
            g_NeedGPUUpdate = true;
            g_MeshDirty = false;
        }).detach();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeUndo(JNIEnv* env, jobject obj) {
    if (!g_undo_stack.empty()) {
        mesh = g_undo_stack.back();
        g_undo_stack.pop_back();
        kr_mesh_rebuild_buffers();
        selected_face_id = -1;
        selected_vertex_id = -1;
    }
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
    LOGI("Saving mesh topology to %s", autosave_path);
    std::ofstream out(autosave_path, std::ios::binary);
    if (!out) {
        LOGE("Failed to open autosave file for writing");
        return;
    }
    
    size_t vert_count = mesh.vertices.size();
    out.write(reinterpret_cast<const char*>(&vert_count), sizeof(vert_count));
    out.write(reinterpret_cast<const char*>(mesh.vertices.data()), vert_count * sizeof(KrVertex));
    
    size_t edge_count = mesh.edges.size();
    out.write(reinterpret_cast<const char*>(&edge_count), sizeof(edge_count));
    out.write(reinterpret_cast<const char*>(mesh.edges.data()), edge_count * sizeof(KrEdge));
    
    size_t he_count = mesh.half_edges.size();
    out.write(reinterpret_cast<const char*>(&he_count), sizeof(he_count));
    out.write(reinterpret_cast<const char*>(mesh.half_edges.data()), he_count * sizeof(KrHalfEdge));
    
    size_t face_count = mesh.faces.size();
    out.write(reinterpret_cast<const char*>(&face_count), sizeof(face_count));
    out.write(reinterpret_cast<const char*>(mesh.faces.data()), face_count * sizeof(KrFace));
    
    out.close();
    LOGI("Mesh topology saved successfully");
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeLoadMesh(JNIEnv* env, jobject obj) {
    LOGI("Loading mesh from %s", autosave_path);
    std::ifstream in(autosave_path, std::ios::binary);
    if (!in) {
        LOGI("No autosave file found, starting with default cube");
        return;
    }
    
    // Get file size for validation
    in.seekg(0, std::ios::end);
    size_t file_size = in.tellg();
    in.seekg(0, std::ios::beg);
    
    if (file_size < sizeof(size_t) * 4) {
        in.close();
        std::remove(autosave_path);
        return;
    }
    
    // Read counts sequentially by seeking
    size_t vert_count = 0;
    in.read(reinterpret_cast<char*>(&vert_count), sizeof(vert_count));
    if (vert_count == 0 || vert_count > 1000000) {
        in.close();
        std::remove(autosave_path);
        mesh = kr_mesh_create_cube();
        kr_mesh_rebuild_buffers();
        return;
    }
    
    in.seekg(vert_count * sizeof(KrVertex), std::ios::cur);
    size_t edge_count = 0;
    in.read(reinterpret_cast<char*>(&edge_count), sizeof(edge_count));
    
    in.seekg(edge_count * sizeof(KrEdge), std::ios::cur);
    size_t he_count = 0;
    in.read(reinterpret_cast<char*>(&he_count), sizeof(he_count));
    
    in.seekg(he_count * sizeof(KrHalfEdge), std::ios::cur);
    size_t face_count = 0;
    in.read(reinterpret_cast<char*>(&face_count), sizeof(face_count));
    
    // Validate final expected size
    size_t expected_full_size = sizeof(size_t) * 4 + 
                                vert_count * sizeof(KrVertex) +
                                edge_count * sizeof(KrEdge) +
                                he_count * sizeof(KrHalfEdge) +
                                face_count * sizeof(KrFace);
                                
    if (file_size != expected_full_size) {
        LOGE("Autosave file format mismatch (expected %zu, got %zu). Deleting file.", expected_full_size, file_size);
        in.close();
        std::remove(autosave_path);
        mesh = kr_mesh_create_cube();
        kr_mesh_rebuild_buffers();
        return;
    }
    
    // Seek back to start and read all data
    in.seekg(0, std::ios::beg);
    
    in.read(reinterpret_cast<char*>(&vert_count), sizeof(vert_count));
    mesh.vertices.resize(vert_count);
    in.read(reinterpret_cast<char*>(mesh.vertices.data()), vert_count * sizeof(KrVertex));
    
    in.read(reinterpret_cast<char*>(&edge_count), sizeof(edge_count));
    mesh.edges.resize(edge_count);
    in.read(reinterpret_cast<char*>(mesh.edges.data()), edge_count * sizeof(KrEdge));
    
    in.read(reinterpret_cast<char*>(&he_count), sizeof(he_count));
    mesh.half_edges.resize(he_count);
    in.read(reinterpret_cast<char*>(mesh.half_edges.data()), he_count * sizeof(KrHalfEdge));
    
    in.read(reinterpret_cast<char*>(&face_count), sizeof(face_count));
    mesh.faces.resize(face_count);
    in.read(reinterpret_cast<char*>(mesh.faces.data()), face_count * sizeof(KrFace));
    
    in.close();

    // Index safety check
    bool corrupted = false;
    for (const auto& e : mesh.edges) {
        if (e.v1 >= vert_count || e.v2 >= vert_count) {
            corrupted = true;
            break;
        }
    }
    if (!corrupted) {
        for (const auto& he : mesh.half_edges) {
            if (he.v != KR_NULL_INDEX && he.v >= vert_count) {
                corrupted = true;
                break;
            }
        }
    }

    if (corrupted) {
        LOGE("Autosave file contains out-of-bounds indices. Deleting.");
        std::remove(autosave_path);
        mesh = kr_mesh_create_cube();
        kr_mesh_rebuild_buffers();
        return;
    }
    
    LOGI("Mesh topology loaded successfully");
    kr_mesh_rebuild_buffers();
}

extern "C" JNIEXPORT void JNICALL
Java_com_kronos3d_GLSurfaceManager_nativeCycleLightColor(JNIEnv* env, jobject obj) {
    static int color_index = 0;
    color_index = (color_index + 1) % 5;
    if (color_index == 0) g_light_color = glm::vec3(1.0f, 1.0f, 0.9f); // Warm White
    else if (color_index == 1) g_light_color = glm::vec3(0.0f, 1.0f, 1.0f); // Cyan
    else if (color_index == 2) g_light_color = glm::vec3(1.0f, 0.0f, 1.0f); // Magenta
    else if (color_index == 3) g_light_color = glm::vec3(0.0f, 1.0f, 0.0f); // Green
    else if (color_index == 4) g_light_color = glm::vec3(1.0f, 0.2f, 0.2f); // Red
}

