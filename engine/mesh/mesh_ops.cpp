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
    unsigned int orig_vert_count = mesh.vertices.size();

    // Helper lambda to find or create midpoints (avoid duplicates on shared edges)
    auto find_midpoint = [&](unsigned int a, unsigned int b) -> unsigned int {
        float ex = (mesh.vertices[a].x + mesh.vertices[b].x) * 0.5f;
        float ey = (mesh.vertices[a].y + mesh.vertices[b].y) * 0.5f;
        float ez = (mesh.vertices[a].z + mesh.vertices[b].z) * 0.5f;
        // Search in vertices added during this subdivision pass to check if shared edge midpoint already exists
        for (unsigned int i = orig_vert_count; i < mesh.vertices.size(); i++) {
            KrVertex& m = mesh.vertices[i];
            if (fabs(m.x - ex) < 0.0001f && fabs(m.y - ey) < 0.0001f && fabs(m.z - ez) < 0.0001f) {
                return i;
            }
        }
        KrVertex mv;
        mv.x = ex;
        mv.y = ey;
        mv.z = ez;
        mv.nx = (mesh.vertices[a].nx + mesh.vertices[b].nx) * 0.5f;
        mv.ny = (mesh.vertices[a].ny + mesh.vertices[b].ny) * 0.5f;
        mv.nz = (mesh.vertices[a].nz + mesh.vertices[b].nz) * 0.5f;
        float m_len = glm::length(glm::vec3(mv.nx, mv.ny, mv.nz));
        if (m_len > 0.0f) {
            mv.nx /= m_len; mv.ny /= m_len; mv.nz /= m_len;
        }
        unsigned int idx = mesh.vertices.size();
        mesh.vertices.push_back(mv);
        return idx;
    };

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
    unsigned int mid[4]; // mid[0] is mid of (v0,v1), mid[1] is mid of (v1,v2), mid[2] is mid of (v2,v3), mid[3] is mid of (v3,v0)
    for (int i = 0; i < count; i++) {
        int next = (i + 1) % count;
        mid[i] = find_midpoint(v[i], v[next]);
    }

    unsigned int idx_center = mesh.vertices.size();
    mesh.vertices.push_back(center);

    // 3. Reconstruct faces using center and midpoints
    if (is_quad) {
        // Quad face split into 4 quads matching standard indexing:
        // quad1: v0, m01, c, m30
        // quad2: m01, v1, m12, c
        // quad3: c, m12, v2, m23
        // quad4: m30, c, m23, v3
        mesh.faces[face_id] = {v[0], mid[0], idx_center, mid[3]};
        mesh.faces.push_back({mid[0], v[1], mid[1], idx_center});
        mesh.faces.push_back({idx_center, mid[1], v[2], mid[2]});
        mesh.faces.push_back({mid[3], idx_center, mid[2], v[3]});
    } else {
        // Triangle split (treating count = 3):
        // mid[0] (0-1), mid[1] (1-2), mid[2] (2-0)
        mesh.faces[face_id] = {v[0], mid[0], idx_center, mid[2]};
        mesh.faces.push_back({mid[0], v[1], mid[1], idx_center});
        mesh.faces.push_back({idx_center, mid[1], v[2], mid[2]});
    }
}

// Extrude face operator (Blender exact mathematical specification - PASO 6)
void kr_mesh_extrude_face(KrMesh& mesh, int face_id, float distance) {
    if (face_id < 0 || face_id >= (int)mesh.faces.size()) return;

    KrFace f = mesh.faces[face_id];
    bool is_quad = f.is_quad();
    if (!is_quad) return; // Spec specifies quad extrude
    unsigned int v[4] = {f.v0, f.v1, f.v2, f.v3};

    glm::vec3 v0(mesh.vertices[v[0]].x, mesh.vertices[v[0]].y, mesh.vertices[v[0]].z);
    glm::vec3 v1(mesh.vertices[v[1]].x, mesh.vertices[v[1]].y, mesh.vertices[v[1]].z);
    glm::vec3 v2(mesh.vertices[v[2]].x, mesh.vertices[v[2]].y, mesh.vertices[v[2]].z);
    glm::vec3 v3(mesh.vertices[v[3]].x, mesh.vertices[v[3]].y, mesh.vertices[v[3]].z);

    // 1. Calculate normal = normalize(cross(v1-v0, v3-v0))
    glm::vec3 normal = glm::normalize(glm::cross(v1 - v0, v3 - v0));
    glm::vec3 offset = normal * distance;

    // 2. Duplicate vertices to offset positions (nv0, nv1, nv2, nv3)
    unsigned int nv[4];
    for (int i = 0; i < 4; i++) {
        KrVertex evi = mesh.vertices[v[i]];
        evi.x += offset.x; evi.y += offset.y; evi.z += offset.z;
        // Cap normal points outward in normal direction
        evi.nx = normal.x; evi.ny = normal.y; evi.nz = normal.z;
        nv[i] = mesh.vertices.size();
        mesh.vertices.push_back(evi);
    }

    // 3. Keep original face at face_id as base.
    // Update original base vertices' normals to point inwards (opposite of normal)
    for (int i = 0; i < 4; i++) {
        mesh.vertices[v[i]].nx = -normal.x;
        mesh.vertices[v[i]].ny = -normal.y;
        mesh.vertices[v[i]].nz = -normal.z;
    }

    // 4. Add new cap face: nv0, nv1, nv2, nv3
    mesh.faces.push_back({nv[0], nv[1], nv[2], nv[3]});

    // 5. Create 4 lateral faces (quads with flat normals)
    unsigned int orig_v[4] = {v[0], v[1], v[2], v[3]};
    unsigned int new_v[4] = {nv[0], nv[1], nv[2], nv[3]};

    for (int i = 0; i < 4; i++) {
        int next = (i + 1) % 4;
        
        // Define lateral quad vertices: v[i], v[next], nv[next], nv[i]
        unsigned int lv0 = orig_v[i];
        unsigned int lv1 = orig_v[next];
        unsigned int lv2 = new_v[next];
        unsigned int lv3 = new_v[i];

        glm::vec3 lp0(mesh.vertices[lv0].x, mesh.vertices[lv0].y, mesh.vertices[lv0].z);
        glm::vec3 lp1(mesh.vertices[lv1].x, mesh.vertices[lv1].y, mesh.vertices[lv1].z);
        glm::vec3 lp2(mesh.vertices[lv2].x, mesh.vertices[lv2].y, mesh.vertices[lv2].z);
        glm::vec3 l_normal = glm::normalize(glm::cross(lp1 - lp0, lp2 - lp0));

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

void kr_mesh_subdivide_all(KrMesh& mesh) {
    int faceCount = mesh.faces.size();
    for (int i = 0; i < faceCount; i++) {
        kr_mesh_subdivide_face(mesh, i);
    }
}
