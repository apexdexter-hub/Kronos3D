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

// Subdivide face operator (Catmull-Clark / BMesh style split into 4 smaller quads/triangles)
// We calculate all new vertices (face center and edge midpoints) in LOCAL coordinates.
void kr_mesh_subdivide_face(KrMesh& mesh, int face_id) {
    if (face_id < 0 || face_id >= (int)mesh.faces.size()) return;

    KrFace f = mesh.faces[face_id];
    bool is_quad = f.is_quad();
    int count = is_quad ? 4 : 3;
    unsigned int v[4] = {f.v0, f.v1, f.v2, f.v3};

    // 1. Calculate Face Center
    KrVertex center;
    center.x = 0; center.y = 0; center.z = 0;
    center.nx = 0; center.ny = 0; center.nz = 0;

    for (int i = 0; i < count; i++) {
        center.x += mesh.vertices[v[i]].x;
        center.y += mesh.vertices[v[i]].y;
        center.z += mesh.vertices[v[i]].z;
        center.nx += mesh.vertices[v[i]].nx;
        center.ny += mesh.vertices[v[i]].ny;
        center.nz += mesh.vertices[v[i]].nz;
    }
    center.x /= count; center.y /= count; center.z /= count;
    float len = glm::length(glm::vec3(center.nx, center.ny, center.nz));
    if (len > 0.0f) {
        center.nx /= len; center.ny /= len; center.nz /= len;
    } else {
        center.nx = mesh.vertices[v[0]].nx;
        center.ny = mesh.vertices[v[0]].ny;
        center.nz = mesh.vertices[v[0]].nz;
    }

    // 2. Calculate Edge Midpoints
    unsigned int mid[4];
    for (int i = 0; i < count; i++) {
        int next = (i + 1) % count;
        KrVertex m;
        m.x = (mesh.vertices[v[i]].x + mesh.vertices[v[next]].x) * 0.5f;
        m.y = (mesh.vertices[v[i]].y + mesh.vertices[v[next]].y) * 0.5f;
        m.z = (mesh.vertices[v[i]].z + mesh.vertices[v[next]].z) * 0.5f;
        
        m.nx = (mesh.vertices[v[i]].nx + mesh.vertices[v[next]].nx) * 0.5f;
        m.ny = (mesh.vertices[v[i]].ny + mesh.vertices[v[next]].ny) * 0.5f;
        m.nz = (mesh.vertices[v[i]].nz + mesh.vertices[v[next]].nz) * 0.5f;
        float m_len = glm::length(glm::vec3(m.nx, m.ny, m.nz));
        if (m_len > 0.0f) {
            m.nx /= m_len; m.ny /= m_len; m.nz /= m_len;
        }

        mid[i] = mesh.vertices.size();
        mesh.vertices.push_back(m);
    }

    unsigned int idx_center = mesh.vertices.size();
    mesh.vertices.push_back(center);

    // 3. Reconstruct faces using center and midpoints
    if (is_quad) {
        // Quad face split into 4 quads:
        // Quad 0: v0 -> mid0 -> center -> mid3
        // Quad 1: mid0 -> v1 -> mid1 -> center
        // Quad 2: center -> mid1 -> v2 -> mid2
        // Quad 3: mid3 -> center -> mid2 -> v3
        mesh.faces[face_id] = {v[0], mid[0], idx_center, mid[3]};
        mesh.faces.push_back({mid[0], v[1], mid[1], idx_center});
        mesh.faces.push_back({idx_center, mid[1], v[2], mid[2]});
        mesh.faces.push_back({mid[3], idx_center, mid[2], v[3]});
    } else {
        // Triangle split into 3 quads:
        // Quad 0: v0 -> mid0 -> center -> mid2
        // Quad 1: mid0 -> v1 -> mid1 -> center
        // Quad 2: center -> mid1 -> v2 -> mid2
        mesh.faces[face_id] = {v[0], mid[0], idx_center, mid[2]};
        mesh.faces.push_back({mid[0], v[1], mid[1], idx_center});
        mesh.faces.push_back({idx_center, mid[1], v[2], mid[2]});
    }
}

// Extrude face operator (BMesh method)
void kr_mesh_extrude_face(KrMesh& mesh, int face_id, float distance) {
    if (face_id < 0 || face_id >= (int)mesh.faces.size()) return;

    KrFace f = mesh.faces[face_id];
    bool is_quad = f.is_quad();
    int count = is_quad ? 4 : 3;
    unsigned int v[4] = {f.v0, f.v1, f.v2, f.v3};

    // Calculate face normal via cross product of edges
    glm::vec3 p0(mesh.vertices[v[0]].x, mesh.vertices[v[0]].y, mesh.vertices[v[0]].z);
    glm::vec3 p1(mesh.vertices[v[1]].x, mesh.vertices[v[1]].y, mesh.vertices[v[1]].z);
    glm::vec3 p2(mesh.vertices[v[2]].x, mesh.vertices[v[2]].y, mesh.vertices[v[2]].z);
    glm::vec3 e1 = p1 - p0;
    glm::vec3 e2 = p2 - p0;
    glm::vec3 normal = glm::normalize(glm::cross(e1, e2));
    glm::vec3 offset = normal * distance;

    // Duplicated vertices for the new cap face
    unsigned int ev[4];
    for (int i = 0; i < count; i++) {
        KrVertex evi = mesh.vertices[v[i]];
        evi.x += offset.x; evi.y += offset.y; evi.z += offset.z;
        // The normal of the top cap face remains the same normal
        evi.nx = normal.x; evi.ny = normal.y; evi.nz = normal.z;
        ev[i] = mesh.vertices.size();
        mesh.vertices.push_back(evi);
    }

    // Update original base vertices' normals to point outwards if needed, 
    // but typically we keep them or average them. For visual correctness, let's keep them.

    // Reassign top face (cap)
    if (is_quad) {
        mesh.faces[face_id] = {ev[0], ev[1], ev[2], ev[3]};
    } else {
        mesh.faces[face_id] = {ev[0], ev[1], ev[2], (unsigned int)-1};
    }

    // Create lateral bridge faces with correct winding order to point normals outwards:
    // For CCW winding on lateral faces: v[i] -> v[next] -> ev[next] -> ev[i]
    for (int i = 0; i < count; i++) {
        int next = (i + 1) % count;
        // Lateral quad
        unsigned int lv0 = v[i];
        unsigned int lv1 = v[next];
        unsigned int lv2 = ev[next];
        unsigned int lv3 = ev[i];
        
        // Let's compute a flat normal for the lateral face
        glm::vec3 lp0(mesh.vertices[lv0].x, mesh.vertices[lv0].y, mesh.vertices[lv0].z);
        glm::vec3 lp1(mesh.vertices[lv1].x, mesh.vertices[lv1].y, mesh.vertices[lv1].z);
        glm::vec3 lp2(mesh.vertices[lv2].x, mesh.vertices[lv2].y, mesh.vertices[lv2].z);
        glm::vec3 l_normal = glm::normalize(glm::cross(lp1 - lp0, lp2 - lp0));

        // Create duplicate vertices for lateral faces to have flat normals
        unsigned int final_l_idx[4];
        unsigned int src_idx[4] = {lv0, lv1, lv2, lv3};
        for (int k = 0; k < 4; k++) {
            KrVertex lv_vert = mesh.vertices[src_idx[k]];
            lv_vert.nx = l_normal.x; lv_vert.ny = l_normal.y; lv_vert.nz = l_normal.z;
            final_l_idx[k] = mesh.vertices.size();
            mesh.vertices.push_back(lv_vert);
        }

        mesh.faces.push_back({final_l_idx[0], final_l_idx[1], final_l_idx[2], final_l_idx[3]});
    }
}
