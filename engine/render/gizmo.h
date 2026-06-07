#ifndef KRONOS_GIZMO_H
#define KRONOS_GIZMO_H

#include <glm/glm.hpp>
#include <GLES3/gl32.h>
#include "engine/mesh/mesh.h"

// Gizmo state: -1 = None, 0 = X, 1 = Y, 2 = Z
extern int g_active_gizmo_axis;

glm::vec3 kr_gizmo_get_selection_center(const KrMesh& mesh, int selected_vertex_id, int selected_face_id, KrEditMode edit_mode);
void kr_gizmo_down(const KrMesh& mesh, int selected_vertex_id, int selected_face_id, KrEditMode edit_mode, float pixel_x, float pixel_y, int screen_width, int screen_height, const glm::mat4& mvp, float distance, float azimuth, float elevation);
bool kr_gizmo_drag(KrMesh& mesh, int selected_vertex_id, int selected_face_id, KrEditMode edit_mode, float dx, float dy, int screen_width, int screen_height, float distance, float azimuth, float elevation);
void kr_gizmo_render(const KrMesh& mesh, int selected_vertex_id, int selected_face_id, KrEditMode edit_mode, const glm::mat4& mvp, GLuint overlay_program, float distance);

#endif // KRONOS_GIZMO_H
