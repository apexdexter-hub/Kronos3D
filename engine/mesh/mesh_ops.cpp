#include "mesh.h"
#include <glm/glm.hpp>
#include <map>

// Move vertex operator
void kr_mesh_move_vertex(KrMesh& mesh, int vertex_id, float dx, float dz) {
    if (vertex_id < 0 || vertex_id >= (int)mesh.vertices.size()) return;
    
    // We group vertices sharing the same physical location to preserve mesh topology during dragging
    glm::vec3 target_pos = glm::vec3(mesh.vertices[vertex_id].x, mesh.vertices[vertex_id].y, mesh.vertices[vertex_id].z);
    
    // Displace dx/dz on horizontal XZ plane
    for (auto& v : mesh.vertices) {
        float dist = glm::distance(glm::vec3(v.x, v.y, v.z), target_pos);
        if (dist < 0.0001f) {
            v.x += dx;
            v.z += dz;
        }
    }
}

// Subdivide face operator (catmull-clark pattern split into 4)
void kr_mesh_subdivide_face(KrMesh& mesh, int face_id) {
    if (face_id < 0 || face_id >= (int)mesh.faces.size()) return;

    KrFace f = mesh.faces[face_id];
    KrVertex v0 = mesh.vertices[f.v0];
    KrVertex v1 = mesh.vertices[f.v1];
    KrVertex v2 = mesh.vertices[f.v2];

    // Compute midpoints of edges and center point
    KrVertex edge01, edge12, edge20, center;
    
    edge01.x = (v0.x + v1.x) * 0.5f; edge01.y = (v0.y + v1.y) * 0.5f; edge01.z = (v0.z + v1.z) * 0.5f;
    edge01.nx = (v0.nx + v1.nx) * 0.5f; edge01.ny = (v0.ny + v1.ny) * 0.5f; edge01.nz = (v0.nz + v1.nz) * 0.5f;

    edge12.x = (v1.x + v2.x) * 0.5f; edge12.y = (v1.y + v2.y) * 0.5f; edge12.z = (v1.z + v2.z) * 0.5f;
    edge12.nx = (v1.nx + v2.nx) * 0.5f; edge12.ny = (v1.ny + v2.ny) * 0.5f; edge12.nz = (v1.nz + v2.nz) * 0.5f;

    edge20.x = (v2.x + v0.x) * 0.5f; edge20.y = (v2.y + v0.y) * 0.5f; edge20.z = (v2.z + v0.z) * 0.5f;
    edge20.nx = (v2.nx + v0.nx) * 0.5f; edge20.ny = (v2.ny + v0.ny) * 0.5f; edge20.nz = (v2.nz + v0.nz) * 0.5f;

    center.x = (v0.x + v1.x + v2.x) / 3.0f; center.y = (v0.y + v1.y + v2.y) / 3.0f; center.z = (v0.z + v1.z + v2.z) / 3.0f;
    center.nx = v0.nx; center.ny = v0.ny; center.nz = v0.nz; // Preserve normal orientation

    unsigned int idx_edge01 = mesh.vertices.size();
    mesh.vertices.push_back(edge01);

    unsigned int idx_edge12 = mesh.vertices.size();
    mesh.vertices.push_back(edge12);

    unsigned int idx_edge20 = mesh.vertices.size();
    mesh.vertices.push_back(edge20);

    unsigned int idx_center = mesh.vertices.size();
    mesh.vertices.push_back(center);

    // Replace original face with 3 subdivide faces
    mesh.faces[face_id] = { f.v0, f.v1, idx_center };
    mesh.faces.push_back({ f.v1, f.v2, idx_center });
    mesh.faces.push_back({ f.v2, f.v0, idx_center });
}

// Extrude face operator
void kr_mesh_extrude_face(KrMesh& mesh, int face_id, float distance) {
    if (face_id < 0 || face_id >= (int)mesh.faces.size()) return;

    KrFace f = mesh.faces[face_id];
    KrVertex v0 = mesh.vertices[f.v0];
    KrVertex v1 = mesh.vertices[f.v1];
    KrVertex v2 = mesh.vertices[f.v2];

    // Face normal direction
    glm::vec3 normal = glm::normalize(glm::vec3(v0.nx, v0.ny, v0.nz));
    glm::vec3 offset = normal * distance;

    // Create extruded vertices
    KrVertex ev0 = v0; ev0.x += offset.x; ev0.y += offset.y; ev0.z += offset.z;
    KrVertex ev1 = v1; ev1.x += offset.x; ev1.y += offset.y; ev1.z += offset.z;
    KrVertex ev2 = v2; ev2.x += offset.x; ev2.y += offset.y; ev2.z += offset.z;

    unsigned int idx_ev0 = mesh.vertices.size(); mesh.vertices.push_back(ev0);
    unsigned int idx_ev1 = mesh.vertices.size(); mesh.vertices.push_back(ev1);
    unsigned int idx_ev2 = mesh.vertices.size(); mesh.vertices.push_back(ev2);

    // Reassign top face
    mesh.faces[face_id] = { idx_ev0, idx_ev1, idx_ev2 };

    // Create lateral faces to bridge extrusion
    // Side 1 (v0 -> v1)
    mesh.faces.push_back({ f.v0, f.v1, idx_ev1 });
    mesh.faces.push_back({ f.v0, idx_ev1, idx_ev0 });

    // Side 2 (v1 -> v2)
    mesh.faces.push_back({ f.v1, f.v2, idx_ev2 });
    mesh.faces.push_back({ f.v1, idx_ev2, idx_ev1 });

    // Side 3 (v2 -> v0)
    mesh.faces.push_back({ f.v2, f.v0, idx_ev0 });
    mesh.faces.push_back({ f.v2, idx_ev0, idx_ev2 });
}
