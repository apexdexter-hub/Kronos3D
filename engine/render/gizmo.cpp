#include "gizmo.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <limits>
#include <vector>
#include <android/log.h>

#define LOG_TAG "Kronos3D_Gizmo"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

int g_active_gizmo_axis = -1;

static float g_gizmo_last_x = 0;
static float g_gizmo_last_y = 0;

glm::vec2 project_to_screen(const glm::vec3& world_pos, const glm::mat4& mvp, int width, int height) {
    glm::vec4 clip_pos = mvp * glm::vec4(world_pos, 1.0f);
    if (clip_pos.w == 0.0f) return glm::vec2(0.0f);
    glm::vec3 ndc = glm::vec3(clip_pos) / clip_pos.w;
    float screen_x = (ndc.x + 1.0f) * 0.5f * width;
    float screen_y = (1.0f - ndc.y) * 0.5f * height;
    return glm::vec2(screen_x, screen_y);
}

glm::vec3 kr_gizmo_get_selection_center(const KrMesh& mesh, int selected_vertex_id, int selected_face_id, KrEditMode edit_mode) {
    if (g_light_selected) {
        return g_light_pos;
    }
    if (edit_mode == OBJECT_MODE) {
        glm::vec3 sum(0.0f);
        for (const auto& v : mesh.vertices) {
            sum += glm::vec3(v.co[0], v.co[1], v.co[2]);
        }
        if (!mesh.vertices.empty()) sum /= (float)mesh.vertices.size();
        return sum;
    }
    if (edit_mode == EDIT_MODE) {
        glm::vec3 sum(0.0f);
        int count = 0;
        for (const auto& v : mesh.vertices) {
            if (v.flag == 1) {
                sum += glm::vec3(v.co[0], v.co[1], v.co[2]);
                count++;
            }
        }
        if (count > 0) return sum / (float)count;

        if (selected_vertex_id != -1 && selected_vertex_id < (int)mesh.vertices.size()) {
            return glm::vec3(mesh.vertices[selected_vertex_id].co[0], mesh.vertices[selected_vertex_id].co[1], mesh.vertices[selected_vertex_id].co[2]);
        } else if (selected_face_id != -1 && selected_face_id < (int)mesh.faces.size()) {
            const KrFace& f = mesh.faces[selected_face_id];
            glm::vec3 sum_f(0.0f);
            uint32_t curr = f.he_first;
            for (uint32_t i = 0; i < f.len; ++i) {
                uint32_t v_idx = mesh.half_edges[mesh.half_edges[curr].prev].v;
                sum_f += glm::vec3(mesh.vertices[v_idx].co[0], mesh.vertices[v_idx].co[1], mesh.vertices[v_idx].co[2]);
                curr = mesh.half_edges[curr].next;
            }
            return sum_f / (float)f.len;
        }
    }
    return glm::vec3(0.0f);
}

glm::mat3 kr_gizmo_get_local_orientation(const KrMesh& mesh, int selected_vertex_id, int selected_face_id, KrEditMode edit_mode) {
    if (g_light_selected) {
        return glm::mat3(1.0f);
    }
    glm::vec3 normal(0.0f, 1.0f, 0.0f); 
    if (edit_mode == OBJECT_MODE) return glm::mat3(1.0f);
    if (edit_mode == EDIT_MODE) {
        glm::vec3 sum_no(0.0f);
        int count = 0;
        for (const auto& v : mesh.vertices) {
            if (v.flag == 1) {
                sum_no += glm::vec3(v.no[0], v.no[1], v.no[2]);
                count++;
            }
        }
        if (count > 0) {
            normal = glm::normalize(sum_no);
            if (glm::length(normal) < 0.01f) normal = glm::vec3(0.0f, 1.0f, 0.0f);
        } else if (selected_vertex_id != -1 && selected_vertex_id < (int)mesh.vertices.size()) {
            const auto& v = mesh.vertices[selected_vertex_id];
            normal = glm::normalize(glm::vec3(v.no[0], v.no[1], v.no[2]));
        } else if (selected_face_id != -1 && selected_face_id < (int)mesh.faces.size()) {
            const KrFace& f = mesh.faces[selected_face_id];
            uint32_t curr = f.he_first;
            uint32_t v0_idx = mesh.half_edges[mesh.half_edges[curr].prev].v;
            uint32_t v1_idx = mesh.half_edges[curr].v;
            uint32_t v2_idx = mesh.half_edges[mesh.half_edges[curr].next].v;
            glm::vec3 v0(mesh.vertices[v0_idx].co[0], mesh.vertices[v0_idx].co[1], mesh.vertices[v0_idx].co[2]);
            glm::vec3 v1(mesh.vertices[v1_idx].co[0], mesh.vertices[v1_idx].co[1], mesh.vertices[v1_idx].co[2]);
            glm::vec3 v2(mesh.vertices[v2_idx].co[0], mesh.vertices[v2_idx].co[1], mesh.vertices[v2_idx].co[2]);
            normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        }
    }
    glm::vec3 up = (std::abs(normal.y) < 0.999f) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 tangent = glm::normalize(glm::cross(up, normal));
    glm::vec3 bitangent = glm::cross(normal, tangent);
    return glm::mat3(tangent, normal, bitangent);
}

// Draw a Solid Cone along a local direction vector
static void draw_cone(const glm::vec3& base, const glm::vec3& tip, const glm::vec3& color, float radius, GLuint color_loc, GLuint vbo) {
    std::vector<float> vertices;
    glm::vec3 dir = tip - base;
    float len = glm::length(dir);
    if (len < 0.0001f) return;
    dir = glm::normalize(dir);

    // Coordinate system for circle base
    glm::vec3 up = (std::abs(dir.y) < 0.99f) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(up, dir));
    glm::vec3 local_up = glm::cross(dir, right);

    const int subdivisions = 12;
    std::vector<glm::vec3> base_pts(subdivisions);
    for (int i = 0; i < subdivisions; ++i) {
        float angle = (float)i * 2.0f * M_PI / (float)subdivisions;
        base_pts[i] = base + (right * cos(angle) + local_up * sin(angle)) * radius;
    }

    // Build triangles for cone body (tip to base)
    for (int i = 0; i < subdivisions; ++i) {
        int next = (i + 1) % subdivisions;
        // Tip triangle
        vertices.push_back(tip.x); vertices.push_back(tip.y); vertices.push_back(tip.z);
        vertices.push_back(base_pts[i].x); vertices.push_back(base_pts[i].y); vertices.push_back(base_pts[i].z);
        vertices.push_back(base_pts[next].x); vertices.push_back(base_pts[next].y); vertices.push_back(base_pts[next].z);

        // Base circle triangle
        vertices.push_back(base.x); vertices.push_back(base.y); vertices.push_back(base.z);
        vertices.push_back(base_pts[next].x); vertices.push_back(base_pts[next].y); vertices.push_back(base_pts[next].z);
        vertices.push_back(base_pts[i].x); vertices.push_back(base_pts[i].y); vertices.push_back(base_pts[i].z);
    }

    glUniform4f(color_loc, color.r, color.g, color.b, 1.0f);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 3);
}

// Draw a Solid Cube centered at center with dimensions half_size
static void draw_cube(const glm::vec3& center, float half_size, const glm::vec3& color, GLuint color_loc, GLuint vbo) {
    float vertices[] = {
        // Front face
        center.x - half_size, center.y - half_size, center.z + half_size,
        center.x + half_size, center.y - half_size, center.z + half_size,
        center.x + half_size, center.y + half_size, center.z + half_size,
        center.x - half_size, center.y - half_size, center.z + half_size,
        center.x + half_size, center.y + half_size, center.z + half_size,
        center.x - half_size, center.y + half_size, center.z + half_size,

        // Back face
        center.x - half_size, center.y - half_size, center.z - half_size,
        center.x - half_size, center.y + half_size, center.z - half_size,
        center.x + half_size, center.y + half_size, center.z - half_size,
        center.x - half_size, center.y - half_size, center.z - half_size,
        center.x + half_size, center.y + half_size, center.z - half_size,
        center.x + half_size, center.y - half_size, center.z - half_size,

        // Left face
        center.x - half_size, center.y + half_size, center.z + half_size,
        center.x - half_size, center.y + half_size, center.z - half_size,
        center.x - half_size, center.y - half_size, center.z - half_size,
        center.x - half_size, center.y + half_size, center.z + half_size,
        center.x - half_size, center.y - half_size, center.z - half_size,
        center.x - half_size, center.y - half_size, center.z + half_size,

        // Right face
        center.x + half_size, center.y + half_size, center.z + half_size,
        center.x + half_size, center.y - half_size, center.z + half_size,
        center.x + half_size, center.y - half_size, center.z - half_size,
        center.x + half_size, center.y + half_size, center.z + half_size,
        center.x + half_size, center.y - half_size, center.z - half_size,
        center.x + half_size, center.y + half_size, center.z - half_size,

        // Top face
        center.x - half_size, center.y + half_size, center.z - half_size,
        center.x - half_size, center.y + half_size, center.z + half_size,
        center.x + half_size, center.y + half_size, center.z + half_size,
        center.x - half_size, center.y + half_size, center.z - half_size,
        center.x + half_size, center.y + half_size, center.z + half_size,
        center.x + half_size, center.y + half_size, center.z - half_size,

        // Bottom face
        center.x - half_size, center.y - half_size, center.z - half_size,
        center.x + half_size, center.y - half_size, center.z - half_size,
        center.x + half_size, center.y - half_size, center.z + half_size,
        center.x - half_size, center.y - half_size, center.z - half_size,
        center.x + half_size, center.y - half_size, center.z + half_size,
        center.x - half_size, center.y - half_size, center.z + half_size
    };

    glUniform4f(color_loc, color.r, color.g, color.b, 1.0f);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

// Draw planar quad handles near the center
static void draw_plane_handle(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, const glm::vec3& p4, const glm::vec3& color, GLuint color_loc, GLuint vbo) {
    float vertices[] = {
        p1.x, p1.y, p1.z,
        p2.x, p2.y, p2.z,
        p3.x, p3.y, p3.z,
        p1.x, p1.y, p1.z,
        p3.x, p3.y, p3.z,
        p4.x, p4.y, p4.z
    };
    glUniform4f(color_loc, color.r, color.g, color.b, 0.4f); // Transparent fill
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Outline
    float outline[] = {
        p1.x, p1.y, p1.z, p2.x, p2.y, p2.z,
        p2.x, p2.y, p2.z, p3.x, p3.y, p3.z,
        p3.x, p3.y, p3.z, p4.x, p4.y, p4.z,
        p4.x, p4.y, p4.z, p1.x, p1.y, p1.z
    };
    glUniform4f(color_loc, color.r, color.g, color.b, 1.0f); // Solid borders
    glBufferData(GL_ARRAY_BUFFER, sizeof(outline), outline, GL_STREAM_DRAW);
    glLineWidth(4.0f);
    glDrawArrays(GL_LINES, 0, 8);
}

// Draw a circular orientation ring in 3D using lines
static void draw_circle(const glm::vec3& center, const glm::mat3& orientation, int normal_axis, float radius, const glm::vec3& color, GLuint color_loc, GLuint vbo) {
    std::vector<float> line_pts;
    const int subdivisions = 48;
    
    // Choose local axis base vectors for circle plane
    glm::vec3 u = orientation[(normal_axis + 1) % 3];
    glm::vec3 v = orientation[(normal_axis + 2) % 3];

    for (int i = 0; i <= subdivisions; ++i) {
        float angle = (float)i * 2.0f * M_PI / (float)subdivisions;
        glm::vec3 pt = center + (u * cos(angle) + v * sin(angle)) * radius;
        line_pts.push_back(pt.x);
        line_pts.push_back(pt.y);
        line_pts.push_back(pt.z);
        if (i > 0 && i < subdivisions) {
            // Duplicate intermediate points to satisfy GL_LINES format (each segment has 2 points)
            line_pts.push_back(pt.x);
            line_pts.push_back(pt.y);
            line_pts.push_back(pt.z);
        }
    }

    glUniform4f(color_loc, color.r, color.g, color.b, 1.0f);
    glBufferData(GL_ARRAY_BUFFER, line_pts.size() * sizeof(float), line_pts.data(), GL_STREAM_DRAW);
    glLineWidth(6.0f);
    glDrawArrays(GL_LINES, 0, line_pts.size() / 3);
}

void kr_gizmo_down(const KrMesh& mesh, int selected_vertex_id, int selected_face_id, KrEditMode edit_mode, KrToolMode tool_mode, float pixel_x, float pixel_y, int screen_width, int screen_height, const glm::mat4& mvp, float distance, float azimuth, float elevation) {
    if (tool_mode == TOOL_SELECT) {
        tool_mode = TOOL_MOVE;
    }
    g_active_gizmo_axis = -1;
    if (edit_mode == EDIT_MODE) {
        bool has_selection = false;
        for (const auto& v : mesh.vertices) {
            if (v.flag == 1) {
                has_selection = true;
                break;
            }
        }
        if (!has_selection && selected_vertex_id == -1 && selected_face_id == -1 && !g_light_selected) return;
    } else {
        // In OBJECT_MODE, allow dragging the mesh origin center freely without selection guards
    }

    g_gizmo_last_x = pixel_x;
    g_gizmo_last_y = pixel_y;

    glm::vec3 center = kr_gizmo_get_selection_center(mesh, selected_vertex_id, selected_face_id, edit_mode);
    glm::mat3 local_rot = kr_gizmo_get_local_orientation(mesh, selected_vertex_id, selected_face_id, edit_mode);
    float scale = distance * 0.25f;

    glm::vec2 touch_pt(pixel_x, pixel_y);

    if (tool_mode == TOOL_MOVE) {
        // Project center and axis tips
        glm::vec2 s_center = project_to_screen(center, mvp, screen_width, screen_height);
        glm::vec2 s_x = project_to_screen(center + local_rot[0] * scale, mvp, screen_width, screen_height);
        glm::vec2 s_y = project_to_screen(center + local_rot[1] * scale, mvp, screen_width, screen_height);
        glm::vec2 s_z = project_to_screen(center + local_rot[2] * scale, mvp, screen_width, screen_height);

        // Project plane handle midpoints
        glm::vec2 s_xy = project_to_screen(center + (local_rot[0] + local_rot[1]) * scale * 0.35f, mvp, screen_width, screen_height);
        glm::vec2 s_yz = project_to_screen(center + (local_rot[1] + local_rot[2]) * scale * 0.35f, mvp, screen_width, screen_height);
        glm::vec2 s_xz = project_to_screen(center + (local_rot[0] + local_rot[2]) * scale * 0.35f, mvp, screen_width, screen_height);

        float dist_xy = glm::distance(touch_pt, s_xy);
        float dist_yz = glm::distance(touch_pt, s_yz);
        float dist_xz = glm::distance(touch_pt, s_xz);

        float dist_x = glm::distance(touch_pt, s_x);
        float dist_y = glm::distance(touch_pt, s_y);
        float dist_z = glm::distance(touch_pt, s_z);

        const float plane_threshold = 40.0f;
        const float axis_threshold = 60.0f;

        // Plane handles have priority (closer to center, smaller target)
        if (dist_xy < plane_threshold && dist_xy < dist_xz && dist_xy < dist_yz) {
            g_active_gizmo_axis = 3; // XY plane
            return;
        }
        if (dist_yz < plane_threshold && dist_yz < dist_xy && dist_yz < dist_xz) {
            g_active_gizmo_axis = 4; // YZ plane
            return;
        }
        if (dist_xz < plane_threshold && dist_xz < dist_xy && dist_xz < dist_yz) {
            g_active_gizmo_axis = 5; // XZ plane
            return;
        }

        // Axis handles
        float min_dist = axis_threshold;
        if (dist_x < min_dist) { min_dist = dist_x; g_active_gizmo_axis = 0; }
        if (dist_y < min_dist) { min_dist = dist_y; g_active_gizmo_axis = 1; }
        if (dist_z < min_dist) { min_dist = dist_z; g_active_gizmo_axis = 2; }
    }
    else if (tool_mode == TOOL_SCALE) {
        // Project center (scale all) and axis tips
        glm::vec2 s_center = project_to_screen(center, mvp, screen_width, screen_height);
        glm::vec2 s_x = project_to_screen(center + local_rot[0] * scale, mvp, screen_width, screen_height);
        glm::vec2 s_y = project_to_screen(center + local_rot[1] * scale, mvp, screen_width, screen_height);
        glm::vec2 s_z = project_to_screen(center + local_rot[2] * scale, mvp, screen_width, screen_height);

        float dist_c = glm::distance(touch_pt, s_center);
        float dist_x = glm::distance(touch_pt, s_x);
        float dist_y = glm::distance(touch_pt, s_y);
        float dist_z = glm::distance(touch_pt, s_z);

        const float center_threshold = 50.0f;
        const float axis_threshold = 60.0f;

        if (dist_c < center_threshold) {
            g_active_gizmo_axis = 6; // Scale All Uniformly
            return;
        }

        float min_dist = axis_threshold;
        if (dist_x < min_dist) { min_dist = dist_x; g_active_gizmo_axis = 0; }
        if (dist_y < min_dist) { min_dist = dist_y; g_active_gizmo_axis = 1; }
        if (dist_z < min_dist) { min_dist = dist_z; g_active_gizmo_axis = 2; }
    }
    else if (tool_mode == TOOL_ROTATE) {
        // For rotation, we match the touch coordinate to the perimeter of the projected axis circles
        float min_circle_dist = 50.0f; // Selection threshold for perimeter
        int best_axis = -1;

        for (int axis = 0; axis < 3; ++axis) {
            glm::vec3 u = local_rot[(axis + 1) % 3];
            glm::vec3 v = local_rot[(axis + 2) % 3];

            // Sample circle perimeter and find closest distance to touch
            float closest_sample = std::numeric_limits<float>::max();
            for (int step = 0; step < 24; ++step) {
                float angle = (float)step * 2.0f * M_PI / 24.0f;
                glm::vec3 pt = center + (u * cos(angle) + v * sin(angle)) * scale;
                glm::vec2 s_pt = project_to_screen(pt, mvp, screen_width, screen_height);
                float d = glm::distance(touch_pt, s_pt);
                if (d < closest_sample) closest_sample = d;
            }

            if (closest_sample < min_circle_dist) {
                min_circle_dist = closest_sample;
                best_axis = axis;
            }
        }
        g_active_gizmo_axis = best_axis;
    }
}

bool kr_gizmo_drag(KrMesh& mesh, int selected_vertex_id, int selected_face_id, KrEditMode edit_mode, KrToolMode tool_mode, float dx, float dy, int screen_width, int screen_height, float distance, float azimuth, float elevation) {
    if (tool_mode == TOOL_SELECT) {
        tool_mode = TOOL_MOVE;
    }
    if (g_active_gizmo_axis == -1) return false;

    glm::vec3 center = kr_gizmo_get_selection_center(mesh, selected_vertex_id, selected_face_id, edit_mode);
    glm::mat3 local_rot = kr_gizmo_get_local_orientation(mesh, selected_vertex_id, selected_face_id, edit_mode);

    // Setup projection & view matrices to cast rays
    float aspect = (float)screen_width / (float)screen_height;
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    float rad_az = glm::radians(azimuth);
    float rad_el = glm::radians(elevation);
    glm::vec3 camera_pos;
    camera_pos.x = distance * cos(rad_el) * sin(rad_az);
    camera_pos.y = distance * sin(rad_el);
    camera_pos.z = distance * cos(rad_el) * cos(rad_az);
    glm::mat4 view = glm::lookAt(camera_pos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 mvp = projection * view;

    glm::vec3 eye_dir = glm::normalize(camera_pos - center);

    glm::vec3 plane_normal(0.0f);
    glm::vec3 drag_dir(0.0f);
    bool plane_mode = false;
    bool scale_uniform = false;

    if (tool_mode == TOOL_MOVE) {
        if (g_active_gizmo_axis == 0) {
            drag_dir = local_rot[0];
            plane_normal = glm::normalize(glm::cross(drag_dir, glm::cross(eye_dir, drag_dir)));
        } else if (g_active_gizmo_axis == 1) {
            drag_dir = local_rot[1];
            plane_normal = glm::normalize(glm::cross(drag_dir, glm::cross(eye_dir, drag_dir)));
        } else if (g_active_gizmo_axis == 2) {
            drag_dir = local_rot[2];
            plane_normal = glm::normalize(glm::cross(drag_dir, glm::cross(eye_dir, drag_dir)));
        } else if (g_active_gizmo_axis == 3) {
            plane_normal = local_rot[2];
            plane_mode = true;
        } else if (g_active_gizmo_axis == 4) {
            plane_normal = local_rot[0];
            plane_mode = true;
        } else if (g_active_gizmo_axis == 5) {
            plane_normal = local_rot[1];
            plane_mode = true;
        }
    }
    else if (tool_mode == TOOL_SCALE) {
        if (g_active_gizmo_axis == 0) {
            drag_dir = local_rot[0];
            plane_normal = glm::normalize(glm::cross(drag_dir, glm::cross(eye_dir, drag_dir)));
        } else if (g_active_gizmo_axis == 1) {
            drag_dir = local_rot[1];
            plane_normal = glm::normalize(glm::cross(drag_dir, glm::cross(eye_dir, drag_dir)));
        } else if (g_active_gizmo_axis == 2) {
            drag_dir = local_rot[2];
            plane_normal = glm::normalize(glm::cross(drag_dir, glm::cross(eye_dir, drag_dir)));
        } else if (g_active_gizmo_axis == 6) {
            plane_normal = eye_dir;
            scale_uniform = true;
        }
    }
    else if (tool_mode == TOOL_ROTATE) {
        // Rotation axis matches selection: 0 = local X axis (perpendicular to YZ plane)
        // rotation plane normal is the rotation axis itself
        plane_normal = local_rot[g_active_gizmo_axis];
    }

    if (glm::length(plane_normal) < 0.001f) return false;

    float current_x = g_gizmo_last_x + dx;
    float current_y = g_gizmo_last_y + dy;

    // Angle calculation for rotation: compare angles relative to center in screen space
    if (tool_mode == TOOL_ROTATE) {
        glm::vec2 s_center = project_to_screen(center, mvp, screen_width, screen_height);
        glm::vec2 prev_pt(g_gizmo_last_x, g_gizmo_last_y);
        glm::vec2 curr_pt(current_x, current_y);

        glm::vec2 v1 = prev_pt - s_center;
        glm::vec2 v2 = curr_pt - s_center;

        if (glm::length(v1) > 1e-4f && glm::length(v2) > 1e-4f) {
            float angle_prev = atan2(v1.y, v1.x);
            float angle_curr = atan2(v2.y, v2.x);
            float d_theta = angle_curr - angle_prev;
            
            // Normalize d_theta
            while (d_theta > M_PI) d_theta -= 2.0f * M_PI;
            while (d_theta < -M_PI) d_theta += 2.0f * M_PI;

            // Decide rotation direction relative to screen orientation/normal sign
            float dot_normal = glm::dot(plane_normal, eye_dir);
            if (dot_normal < 0.0f) d_theta = -d_theta;

            // Generate rotation matrix around the selected local axis
            glm::mat4 rot_mat = glm::rotate(glm::mat4(1.0f), d_theta, plane_normal);

            auto apply_rotation = [&](float co[3]) {
                glm::vec3 p(co[0], co[1], co[2]);
                glm::vec3 local_p = p - center;
                glm::vec4 rotated = rot_mat * glm::vec4(local_p, 1.0f);
                glm::vec3 result = center + glm::vec3(rotated);
                co[0] = result.x;
                co[1] = result.y;
                co[2] = result.z;
            };

            if (edit_mode == EDIT_MODE) {
                if (selected_vertex_id != -1 && selected_vertex_id < (int)mesh.vertices.size()) {
                    apply_rotation(mesh.vertices[selected_vertex_id].co);
                } else if (selected_face_id != -1 && selected_face_id < (int)mesh.faces.size()) {
                    KrFace& f = mesh.faces[selected_face_id];
                    uint32_t curr = f.he_first;
                    for (uint32_t i = 0; i < f.len; ++i) {
                        uint32_t v_idx = mesh.half_edges[mesh.half_edges[curr].prev].v;
                        apply_rotation(mesh.vertices[v_idx].co);
                        curr = mesh.half_edges[curr].next;
                    }
                }
            } else if (edit_mode == OBJECT_MODE) {
                for (auto& v : mesh.vertices) {
                    apply_rotation(v.co);
                }
            }
        }

        g_gizmo_last_x = current_x;
        g_gizmo_last_y = current_y;
        return true;
    }

    auto get_ray = [&](float px, float py) -> glm::vec3 {
        float nx = (2.0f * px / (float)screen_width) - 1.0f;
        float ny = 1.0f - (2.0f * py / (float)screen_height);
        glm::vec4 clip = glm::vec4(nx, ny, -1.0f, 1.0f);
        glm::vec4 ray_eye = glm::inverse(projection) * clip;
        ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);
        return glm::normalize(glm::vec3(glm::inverse(view) * ray_eye));
    };

    glm::vec3 ray1_dir = get_ray(g_gizmo_last_x, g_gizmo_last_y);
    glm::vec3 ray2_dir = get_ray(current_x, current_y);

    auto intersect = [&](glm::vec3 rdir) -> glm::vec3 {
        float denom = glm::dot(plane_normal, rdir);
        if (std::abs(denom) > 1e-6) {
            float t = glm::dot(plane_normal, center - camera_pos) / denom;
            return camera_pos + t * rdir;
        }
        return center;
    };

    glm::vec3 p1 = intersect(ray1_dir);
    glm::vec3 p2 = intersect(ray2_dir);
    glm::vec3 delta = p2 - p1;

    g_gizmo_last_x = current_x;
    g_gizmo_last_y = current_y;

    if (tool_mode == TOOL_MOVE) {
        if (!plane_mode) {
            float displacement = glm::dot(delta, drag_dir);
            delta = drag_dir * displacement;
        }

        if (g_light_selected) {
            g_light_pos += delta;
        } else if (edit_mode == EDIT_MODE) {
            bool moved_selection = false;
            for (auto& v : mesh.vertices) {
                if (v.flag == 1) {
                    v.co[0] += delta.x;
                    v.co[1] += delta.y;
                    v.co[2] += delta.z;
                    moved_selection = true;
                }
            }
            if (!moved_selection) {
                if (selected_vertex_id != -1 && selected_vertex_id < (int)mesh.vertices.size()) {
                    mesh.vertices[selected_vertex_id].co[0] += delta.x;
                    mesh.vertices[selected_vertex_id].co[1] += delta.y;
                    mesh.vertices[selected_vertex_id].co[2] += delta.z;
                } else if (selected_face_id != -1 && selected_face_id < (int)mesh.faces.size()) {
                    KrFace& f = mesh.faces[selected_face_id];
                    uint32_t curr = f.he_first;
                    for (uint32_t i = 0; i < f.len; ++i) {
                        uint32_t v_idx = mesh.half_edges[mesh.half_edges[curr].prev].v;
                        mesh.vertices[v_idx].co[0] += delta.x;
                        mesh.vertices[v_idx].co[1] += delta.y;
                        mesh.vertices[v_idx].co[2] += delta.z;
                        curr = mesh.half_edges[curr].next;
                    }
                }
            }
        } else if (edit_mode == OBJECT_MODE) {
            for (auto& v : mesh.vertices) {
                v.co[0] += delta.x;
                v.co[1] += delta.y;
                v.co[2] += delta.z;
            }
        }
    }
    else if (tool_mode == TOOL_SCALE) {
        glm::vec3 scale_factor(1.0f);
        if (scale_uniform) {
            float factor = 1.0f + (dx + dy) * 0.01f;
            scale_factor = glm::vec3(factor);
        } else {
            float displacement = glm::dot(delta, drag_dir);
            float factor = 1.0f + displacement * 0.25f;
            if (g_active_gizmo_axis == 0) scale_factor.x = factor;
            else if (g_active_gizmo_axis == 1) scale_factor.y = factor;
            else if (g_active_gizmo_axis == 2) scale_factor.z = factor;
        }

        auto apply_scale = [&](float co[3]) {
            glm::vec3 p(co[0], co[1], co[2]);
            glm::vec3 diff = p - center;
            diff.x *= scale_factor.x;
            diff.y *= scale_factor.y;
            diff.z *= scale_factor.z;
            glm::vec3 scaled = center + diff;
            co[0] = scaled.x;
            co[1] = scaled.y;
            co[2] = scaled.z;
        };

        if (edit_mode == EDIT_MODE) {
            bool scaled_selection = false;
            for (auto& v : mesh.vertices) {
                if (v.flag == 1) {
                    apply_scale(v.co);
                    scaled_selection = true;
                }
            }
            if (!scaled_selection) {
                if (selected_vertex_id != -1 && selected_vertex_id < (int)mesh.vertices.size()) {
                    apply_scale(mesh.vertices[selected_vertex_id].co);
                } else if (selected_face_id != -1 && selected_face_id < (int)mesh.faces.size()) {
                    KrFace& f = mesh.faces[selected_face_id];
                    uint32_t curr = f.he_first;
                    for (uint32_t i = 0; i < f.len; ++i) {
                        uint32_t v_idx = mesh.half_edges[mesh.half_edges[curr].prev].v;
                        apply_scale(mesh.vertices[v_idx].co);
                        curr = mesh.half_edges[curr].next;
                    }
                }
            }
        } else if (edit_mode == OBJECT_MODE) {
            for (auto& v : mesh.vertices) {
                apply_scale(v.co);
            }
        }
    }
    return true;
}

void kr_gizmo_render(const KrMesh& mesh, int selected_vertex_id, int selected_face_id, KrEditMode edit_mode, KrToolMode tool_mode, const glm::mat4& mvp, GLuint overlay_program, float distance) {
    if (tool_mode == TOOL_SELECT) {
        tool_mode = TOOL_MOVE;
    }
    if (edit_mode == EDIT_MODE) {
        bool has_selection = false;
        for (const auto& v : mesh.vertices) {
            if (v.flag == 1) {
                has_selection = true;
                break;
            }
        }
        if (!has_selection && selected_vertex_id == -1 && selected_face_id == -1 && !g_light_selected) return;
    } else {
        // In OBJECT_MODE, we show the Gizmo at the object's origin center regardless of selection to allow full body transformations
    }

    glDisable(GL_DEPTH_TEST);
    glUseProgram(overlay_program);
    GLint overlay_mvp_loc = glGetUniformLocation(overlay_program, "u_MVP");
    GLint overlay_color_loc = glGetUniformLocation(overlay_program, "u_Color");
    glUniformMatrix4fv(overlay_mvp_loc, 1, GL_FALSE, glm::value_ptr(mvp));

    glm::vec3 center = kr_gizmo_get_selection_center(mesh, selected_vertex_id, selected_face_id, edit_mode);
    glm::mat3 local_rot = kr_gizmo_get_local_orientation(mesh, selected_vertex_id, selected_face_id, edit_mode);

    float scale = distance * 0.25f;

    GLuint g_vbo;
    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    if (tool_mode == TOOL_MOVE) {
        glm::vec3 col_x = (g_active_gizmo_axis == 0) ? glm::vec3(1.0f, 1.0f, 0.0f) : glm::vec3(0.85f, 0.15f, 0.15f);
        glm::vec3 col_y = (g_active_gizmo_axis == 1) ? glm::vec3(1.0f, 1.0f, 0.0f) : glm::vec3(0.15f, 0.85f, 0.15f);
        glm::vec3 col_z = (g_active_gizmo_axis == 2) ? glm::vec3(1.0f, 1.0f, 0.0f) : glm::vec3(0.15f, 0.15f, 0.85f);

        glm::vec3 col_xy = (g_active_gizmo_axis == 3) ? glm::vec3(1.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.8f, 0.8f);
        glm::vec3 col_yz = (g_active_gizmo_axis == 4) ? glm::vec3(1.0f, 1.0f, 0.0f) : glm::vec3(0.8f, 0.8f, 0.0f);
        glm::vec3 col_xz = (g_active_gizmo_axis == 5) ? glm::vec3(1.0f, 1.0f, 0.0f) : glm::vec3(0.8f, 0.0f, 0.8f);

        glm::vec3 axis_x = local_rot[0] * scale;
        glm::vec3 axis_y = local_rot[1] * scale;
        glm::vec3 axis_z = local_rot[2] * scale;

        glLineWidth(5.0f);
        
        glUniform4f(overlay_color_loc, col_x.r, col_x.g, col_x.b, 1.0f);
        float line_x[] = { center.x, center.y, center.z, center.x + axis_x.x * 0.8f, center.y + axis_x.y * 0.8f, center.z + axis_x.z * 0.8f };
        glBufferData(GL_ARRAY_BUFFER, sizeof(line_x), line_x, GL_STREAM_DRAW);
        glDrawArrays(GL_LINES, 0, 2);

        glUniform4f(overlay_color_loc, col_y.r, col_y.g, col_y.b, 1.0f);
        float line_y[] = { center.x, center.y, center.z, center.x + axis_y.x * 0.8f, center.y + axis_y.y * 0.8f, center.z + axis_y.z * 0.8f };
        glBufferData(GL_ARRAY_BUFFER, sizeof(line_y), line_y, GL_STREAM_DRAW);
        glDrawArrays(GL_LINES, 0, 2);

        glUniform4f(overlay_color_loc, col_z.r, col_z.g, col_z.b, 1.0f);
        float line_z[] = { center.x, center.y, center.z, center.x + axis_z.x * 0.8f, center.y + axis_z.y * 0.8f, center.z + axis_z.z * 0.8f };
        glBufferData(GL_ARRAY_BUFFER, sizeof(line_z), line_z, GL_STREAM_DRAW);
        glDrawArrays(GL_LINES, 0, 2);

        float cone_rad = scale * 0.07f;
        draw_cone(center + axis_x * 0.8f, center + axis_x, col_x, cone_rad, overlay_color_loc, g_vbo);
        draw_cone(center + axis_y * 0.8f, center + axis_y, col_y, cone_rad, overlay_color_loc, g_vbo);
        draw_cone(center + axis_z * 0.8f, center + axis_z, col_z, cone_rad, overlay_color_loc, g_vbo);

        float p_sz = scale * 0.3f;
        glm::vec3 origin_xy = center + (local_rot[0] + local_rot[1]) * p_sz * 0.2f;
        glm::vec3 origin_yz = center + (local_rot[1] + local_rot[2]) * p_sz * 0.2f;
        glm::vec3 origin_xz = center + (local_rot[0] + local_rot[2]) * p_sz * 0.2f;

        draw_plane_handle(
            origin_xy, 
            origin_xy + local_rot[0] * p_sz, 
            origin_xy + local_rot[0] * p_sz + local_rot[1] * p_sz, 
            origin_xy + local_rot[1] * p_sz, 
            col_xy, overlay_color_loc, g_vbo
        );

        draw_plane_handle(
            origin_yz, 
            origin_yz + local_rot[1] * p_sz, 
            origin_yz + local_rot[1] * p_sz + local_rot[2] * p_sz, 
            origin_yz + local_rot[2] * p_sz, 
            col_yz, overlay_color_loc, g_vbo
        );

        draw_plane_handle(
            origin_xz, 
            origin_xz + local_rot[0] * p_sz, 
            origin_xz + local_rot[0] * p_sz + local_rot[2] * p_sz, 
            origin_xz + local_rot[2] * p_sz, 
            col_xz, overlay_color_loc, g_vbo
        );
    }
    else if (tool_mode == TOOL_SCALE) {
        glm::vec3 col_x = (g_active_gizmo_axis == 0) ? glm::vec3(1.0f, 1.0f, 0.0f) : glm::vec3(0.85f, 0.15f, 0.15f);
        glm::vec3 col_y = (g_active_gizmo_axis == 1) ? glm::vec3(1.0f, 1.0f, 0.0f) : glm::vec3(0.15f, 0.85f, 0.15f);
        glm::vec3 col_z = (g_active_gizmo_axis == 2) ? glm::vec3(1.0f, 1.0f, 0.0f) : glm::vec3(0.15f, 0.15f, 0.85f);
        glm::vec3 col_center = (g_active_gizmo_axis == 6) ? glm::vec3(1.0f, 1.0f, 0.0f) : glm::vec3(0.85f, 0.85f, 0.85f);

        glm::vec3 axis_x = local_rot[0] * scale;
        glm::vec3 axis_y = local_rot[1] * scale;
        glm::vec3 axis_z = local_rot[2] * scale;

        glLineWidth(5.0f);

        glUniform4f(overlay_color_loc, col_x.r, col_x.g, col_x.b, 1.0f);
        float line_x[] = { center.x, center.y, center.z, center.x + axis_x.x * 0.9f, center.y + axis_x.y * 0.9f, center.z + axis_x.z * 0.9f };
        glBufferData(GL_ARRAY_BUFFER, sizeof(line_x), line_x, GL_STREAM_DRAW);
        glDrawArrays(GL_LINES, 0, 2);

        glUniform4f(overlay_color_loc, col_y.r, col_y.g, col_y.b, 1.0f);
        float line_y[] = { center.x, center.y, center.z, center.x + axis_y.x * 0.9f, center.y + axis_y.y * 0.9f, center.z + axis_y.z * 0.9f };
        glBufferData(GL_ARRAY_BUFFER, sizeof(line_y), line_y, GL_STREAM_DRAW);
        glDrawArrays(GL_LINES, 0, 2);

        glUniform4f(overlay_color_loc, col_z.r, col_z.g, col_z.b, 1.0f);
        float line_z[] = { center.x, center.y, center.z, center.x + axis_z.x * 0.9f, center.y + axis_z.y * 0.9f, center.z + axis_z.z * 0.9f };
        glBufferData(GL_ARRAY_BUFFER, sizeof(line_z), line_z, GL_STREAM_DRAW);
        glDrawArrays(GL_LINES, 0, 2);

        float cube_sz = scale * 0.05f;
        draw_cube(center + axis_x, cube_sz, col_x, overlay_color_loc, g_vbo);
        draw_cube(center + axis_y, cube_sz, col_y, overlay_color_loc, g_vbo);
        draw_cube(center + axis_z, cube_sz, col_z, overlay_color_loc, g_vbo);

        draw_cube(center, scale * 0.06f, col_center, overlay_color_loc, g_vbo);
    }
    else if (tool_mode == TOOL_ROTATE) {
        // Red circle (X axis normal)
        glm::vec3 col_x = (g_active_gizmo_axis == 0) ? glm::vec3(1.0f, 1.0f, 0.0f) : glm::vec3(0.85f, 0.15f, 0.15f);
        // Green circle (Y axis normal)
        glm::vec3 col_y = (g_active_gizmo_axis == 1) ? glm::vec3(1.0f, 1.0f, 0.0f) : glm::vec3(0.15f, 0.85f, 0.15f);
        // Blue circle (Z axis normal)
        glm::vec3 col_z = (g_active_gizmo_axis == 2) ? glm::vec3(1.0f, 1.0f, 0.0f) : glm::vec3(0.15f, 0.15f, 0.85f);

        draw_circle(center, local_rot, 0, scale, col_x, overlay_color_loc, g_vbo);
        draw_circle(center, local_rot, 1, scale, col_y, overlay_color_loc, g_vbo);
        draw_circle(center, local_rot, 2, scale, col_z, overlay_color_loc, g_vbo);
    }

    glDeleteBuffers(1, &g_vbo);
    glEnable(GL_DEPTH_TEST);
}
