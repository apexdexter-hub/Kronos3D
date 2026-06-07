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

// Subdivide face operator (simple center split)
void kr_mesh_subdivide_face(KrMesh& mesh, int face_id) {
    if (face_id < 0 || face_id >= (int)mesh.faces.size()) return;

    KrFace f = mesh.faces[face_id];
    bool is_quad = f.is_quad();
    int count = is_quad ? 4 : 3;
    unsigned int v[4] = {f.v0, f.v1, f.v2, f.v3};

    KrVertex center;
    center.x = 0; center.y = 0; center.z = 0;
    center.nx = mesh.vertices[v[0]].nx; center.ny = mesh.vertices[v[0]].ny; center.nz = mesh.vertices[v[0]].nz;

    for (int i = 0; i < count; i++) {
        center.x += mesh.vertices[v[i]].x;
        center.y += mesh.vertices[v[i]].y;
        center.z += mesh.vertices[v[i]].z;
    }
    center.x /= count; center.y /= count; center.z /= count;
    
    unsigned int idx_center = mesh.vertices.size();
    mesh.vertices.push_back(center);

    if (is_quad) {
        mesh.faces[face_id] = {v[0], v[1], idx_center, (unsigned int)-1};
        mesh.faces.push_back({v[1], v[2], idx_center, (unsigned int)-1});
        mesh.faces.push_back({v[2], v[3], idx_center, (unsigned int)-1});
        mesh.faces.push_back({v[3], v[0], idx_center, (unsigned int)-1});
    } else {
        mesh.faces[face_id] = {v[0], v[1], idx_center, (unsigned int)-1};
        mesh.faces.push_back({v[1], v[2], idx_center, (unsigned int)-1});
        mesh.faces.push_back({v[2], v[0], idx_center, (unsigned int)-1});
    }
}

// Extrude face operator
void kr_mesh_extrude_face(KrMesh& mesh, int face_id, float distance) {
    if (face_id < 0 || face_id >= (int)mesh.faces.size()) return;

    KrFace f = mesh.faces[face_id];
    bool is_quad = f.is_quad();
    int count = is_quad ? 4 : 3;
    unsigned int v[4] = {f.v0, f.v1, f.v2, f.v3};

    // Face normal direction via cross product of first two edges
    glm::vec3 p0(mesh.vertices[v[0]].x, mesh.vertices[v[0]].y, mesh.vertices[v[0]].z);
    glm::vec3 p1(mesh.vertices[v[1]].x, mesh.vertices[v[1]].y, mesh.vertices[v[1]].z);
    glm::vec3 p2(mesh.vertices[v[2]].x, mesh.vertices[v[2]].y, mesh.vertices[v[2]].z);
    glm::vec3 e1 = p1 - p0;
    glm::vec3 e2 = p2 - p0;
    glm::vec3 normal = glm::normalize(glm::cross(e1, e2));
    glm::vec3 offset = normal * distance;

    // Create extruded vertices
    unsigned int ev[4];
    for (int i = 0; i < count; i++) {
        KrVertex evi = mesh.vertices[v[i]];
        evi.x += offset.x; evi.y += offset.y; evi.z += offset.z;
        ev[i] = mesh.vertices.size();
        mesh.vertices.push_back(evi);
    }

    // Reassign top face (cap)
    if (is_quad) {
        mesh.faces[face_id] = {ev[0], ev[1], ev[2], ev[3]};
    } else {
        mesh.faces[face_id] = {ev[0], ev[1], ev[2], (unsigned int)-1};
    }

    // Create lateral faces (quads) to bridge extrusion
    for (int i = 0; i < count; i++) {
        int next = (i + 1) % count;
        // Lateral quad: v[next], ev[next], ev[i], v[i]
        mesh.faces.push_back({v[next], ev[next], ev[i], v[i]});
    }
}
