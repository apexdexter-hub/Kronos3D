#include "gizmo.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <limits>
#include <android/log.h>

#define LOG_TAG "Kronos3D_Gizmo"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

int g_active_gizmo_axis = -1;

static glm::vec2 project_to_screen(const glm::vec3& world_pos, const glm::mat4& mvp, int width, int height) {
    glm::vec4 clip_pos = mvp * glm::vec4(world_pos, 1.0f);
    if (clip_pos.w == 0.0f) return glm::vec2(0.0f);
    glm::vec3 ndc = glm::vec3(clip_pos) / clip_pos.w;
    float screen_x = (ndc.x + 1.0f) * 0.5f * width;
    float screen_y = (1.0f - ndc.y) * 0.5f * height;
    return glm::vec2(screen_x, screen_y);
}

glm::vec3 kr_gizmo_get_selection_center(const KrMesh& mesh, int selected_vertex_id, int selected_face_id, KrEditMode edit_mode) {
    if (edit_mode == EDIT_MODE && selected_vertex_id != -1 && selected_vertex_id < (int)mesh.vertices.size()) {
        return glm::vec3(mesh.vertices[selected_vertex_id].x, mesh.vertices[selected_vertex_id].y, mesh.vertices[selected_vertex_id].z);
    } else if (selected_face_id != -1 && selected_face_id < (int)mesh.faces.size()) {
        const KrFace& f = mesh.faces[selected_face_id];
        glm::vec3 v0(mesh.vertices[f.v0].x, mesh.vertices[f.v0].y, mesh.vertices[f.v0].z);
        glm::vec3 v1(mesh.vertices[f.v1].x, mesh.vertices[f.v1].y, mesh.vertices[f.v1].z);
        glm::vec3 v2(mesh.vertices[f.v2].x, mesh.vertices[f.v2].y, mesh.vertices[f.v2].z);
        return (v0 + v1 + v2) / 3.0f;
    }
    return glm::vec3(0.0f);
}

void kr_gizmo_down(const KrMesh& mesh, int selected_vertex_id, int selected_face_id, KrEditMode edit_mode, float pixel_x, float pixel_y, int screen_width, int screen_height, const glm::mat4& mvp, float distance, float azimuth, float elevation) {
    g_active_gizmo_axis = -1;
    if (selected_face_id == -1 && selected_vertex_id == -1) return;

    glm::vec3 center = kr_gizmo_get_selection_center(mesh, selected_vertex_id, selected_face_id, edit_mode);
    
    // Scale gizmo size with camera distance
    float scale = 0.15f * distance;
    if (scale < 0.2f) scale = 0.2f;

    glm::vec2 s_center = project_to_screen(center, mvp, screen_width, screen_height);
    glm::vec2 s_x = project_to_screen(center + glm::vec3(scale, 0.0f, 0.0f), mvp, screen_width, screen_height);
    glm::vec2 s_y = project_to_screen(center + glm::vec3(0.0f, scale, 0.0f), mvp, screen_width, screen_height);
    glm::vec2 s_z = project_to_screen(center + glm::vec3(0.0f, 0.0f, scale), mvp, screen_width, screen_height);

    glm::vec2 touch_pt(pixel_x, pixel_y);

    // Distance to axis tips
    float dist_x = glm::distance(touch_pt, s_x);
    float dist_y = glm::distance(touch_pt, s_y);
    float dist_z = glm::distance(touch_pt, s_z);

    // Click threshold in pixels
    const float threshold = 80.0f;
    float min_dist = threshold;

    if (dist_x < min_dist) { min_dist = dist_x; g_active_gizmo_axis = 0; }
    if (dist_y < min_dist) { min_dist = dist_y; g_active_gizmo_axis = 1; }
    if (dist_z < min_dist) { min_dist = dist_z; g_active_gizmo_axis = 2; }
    
    LOGI("Gizmo click checked. Touch: (%.1f, %.1f), Active axis: %d", pixel_x, pixel_y, g_active_gizmo_axis);
}

bool kr_gizmo_drag(KrMesh& mesh, int selected_vertex_id, int selected_face_id, KrEditMode edit_mode, float dx, float dy, int screen_width, int screen_height, float distance, float azimuth, float elevation) {
    if (g_active_gizmo_axis == -1) return false;

    glm::vec3 dir(0.0f);
    if (g_active_gizmo_axis == 0) dir = glm::vec3(1.0f, 0.0f, 0.0f);
    else if (g_active_gizmo_axis == 1) dir = glm::vec3(0.0f, 1.0f, 0.0f);
    else if (g_active_gizmo_axis == 2) dir = glm::vec3(0.0f, 0.0f, 1.0f);

    float aspect = (float)screen_width / (float)screen_height;
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    float rad_az = glm::radians(azimuth);
    float rad_el = glm::radians(elevation);
    glm::vec3 camera_pos;
    camera_pos.x = distance * cos(rad_el) * sin(rad_az);
    camera_pos.y = distance * sin(rad_el);
    camera_pos.z = distance * cos(rad_el) * cos(rad_az);
    glm::mat4 view = glm::lookAt(camera_pos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 mvp = projection * view * model;

    glm::vec3 center = kr_gizmo_get_selection_center(mesh, selected_vertex_id, selected_face_id, edit_mode);
    glm::vec2 s_center = project_to_screen(center, mvp, screen_width, screen_height);
    glm::vec2 s_tip = project_to_screen(center + dir, mvp, screen_width, screen_height);

    glm::vec2 axis_vec = s_tip - s_center;
    float axis_len = glm::length(axis_vec);
    if (axis_len > 0.001f) {
        glm::vec2 drag_vec(dx, -dy);
        float proj = glm::dot(drag_vec, axis_vec / axis_len);
        
        // Displace scaling factor
        float displacement = proj * 0.005f * distance;
        glm::vec3 delta = dir * displacement;

        if (edit_mode == EDIT_MODE && selected_vertex_id != -1 && selected_vertex_id < (int)mesh.vertices.size()) {
            glm::vec3 target_pos(mesh.vertices[selected_vertex_id].x, mesh.vertices[selected_vertex_id].y, mesh.vertices[selected_vertex_id].z);
            for (auto& v : mesh.vertices) {
                if (glm::distance(glm::vec3(v.x, v.y, v.z), target_pos) < 0.0001f) {
                    v.x += delta.x;
                    v.y += delta.y;
                    v.z += delta.z;
                }
            }
        } else if (selected_face_id != -1 && selected_face_id < (int)mesh.faces.size()) {
            const KrFace& f = mesh.faces[selected_face_id];
            glm::vec3 target_pos_v0(mesh.vertices[f.v0].x, mesh.vertices[f.v0].y, mesh.vertices[f.v0].z);
            glm::vec3 target_pos_v1(mesh.vertices[f.v1].x, mesh.vertices[f.v1].y, mesh.vertices[f.v1].z);
            glm::vec3 target_pos_v2(mesh.vertices[f.v2].x, mesh.vertices[f.v2].y, mesh.vertices[f.v2].z);
            for (auto& v : mesh.vertices) {
                glm::vec3 curr(v.x, v.y, v.z);
                if (glm::distance(curr, target_pos_v0) < 0.0001f ||
                    glm::distance(curr, target_pos_v1) < 0.0001f ||
                    glm::distance(curr, target_pos_v2) < 0.0001f) {
                    v.x += delta.x;
                    v.y += delta.y;
                    v.z += delta.z;
                }
            }
        }
        return true;
    }
    return false;
}

void kr_gizmo_render(const KrMesh& mesh, int selected_vertex_id, int selected_face_id, KrEditMode edit_mode, const glm::mat4& mvp, GLuint overlay_program) {
    if (selected_face_id == -1 && selected_vertex_id == -1) return;

    glDisable(GL_DEPTH_TEST);
    glUseProgram(overlay_program);
    GLint overlay_mvp_loc = glGetUniformLocation(overlay_program, "u_MVP");
    GLint overlay_color_loc = glGetUniformLocation(overlay_program, "u_Color");
    glUniformMatrix4fv(overlay_mvp_loc, 1, GL_FALSE, glm::value_ptr(mvp));

    glm::vec3 center = kr_gizmo_get_selection_center(mesh, selected_vertex_id, selected_face_id, edit_mode);

    // Compute dynamic gizmo scale based on projection matrix properties
    // Default length is 1.0 unit
    float scale = 0.8f;

    // Draw X axis (Red)
    glUniform4f(overlay_color_loc, 1.0f, 0.0f, 0.0f, 1.0f);
    float line_x[6] = { center.x, center.y, center.z, center.x + scale, center.y, center.z };
    GLuint g_vbo;
    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(line_x), line_x, GL_STREAM_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_LINES, 0, 2);

    // Draw Y axis (Green)
    glUniform4f(overlay_color_loc, 0.0f, 1.0f, 0.0f, 1.0f);
    float line_y[6] = { center.x, center.y, center.z, center.x, center.y + scale, center.z };
    glBufferData(GL_ARRAY_BUFFER, sizeof(line_y), line_y, GL_STREAM_DRAW);
    glDrawArrays(GL_LINES, 0, 2);

    // Draw Z axis (Blue)
    glUniform4f(overlay_color_loc, 0.0f, 0.0f, 1.0f, 1.0f);
    float line_z[6] = { center.x, center.y, center.z, center.x, center.y, center.z + scale };
    glBufferData(GL_ARRAY_BUFFER, sizeof(line_z), line_z, GL_STREAM_DRAW);
    glDrawArrays(GL_LINES, 0, 2);

    // Draw highlight tip points
    glUniform4f(overlay_color_loc, 1.0f, 0.0f, 0.0f, 1.0f);
    float pt_x[3] = { center.x + scale, center.y, center.z };
    glBufferData(GL_ARRAY_BUFFER, sizeof(pt_x), pt_x, GL_STREAM_DRAW);
    glDrawArrays(GL_POINTS, 0, 1);

    glUniform4f(overlay_color_loc, 0.0f, 1.0f, 0.0f, 1.0f);
    float pt_y[3] = { center.x, center.y + scale, center.z };
    glBufferData(GL_ARRAY_BUFFER, sizeof(pt_y), pt_y, GL_STREAM_DRAW);
    glDrawArrays(GL_POINTS, 0, 1);

    glUniform4f(overlay_color_loc, 0.0f, 0.0f, 1.0f, 1.0f);
    float pt_z[3] = { center.x, center.y, center.z + scale };
    glBufferData(GL_ARRAY_BUFFER, sizeof(pt_z), pt_z, GL_STREAM_DRAW);
    glDrawArrays(GL_POINTS, 0, 1);

    glDeleteBuffers(1, &g_vbo);
    glEnable(GL_DEPTH_TEST);
}
