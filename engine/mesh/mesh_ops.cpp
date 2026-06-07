#include "mesh.h"
#include <vector>
#include <map>
#include <utility>

void kr_mesh_move_vertex(KrMesh& mesh, int vertex_id, float dx, float dz) {
    if(vertex_id < 0 || vertex_id >= mesh.vertices.size()) return;
    mesh.vertices[vertex_id].co[0] += dx;
    mesh.vertices[vertex_id].co[2] += dz;
    mesh.isDirty = true;
}

void kr_mesh_extrude_face(KrMesh& mesh, int face_id, float distance) {
    // simplified stub to compile
    mesh.isDirty = true;
}

void kr_mesh_subdivide_face(KrMesh& mesh, int face_id) {
    // simplified stub to compile
    mesh.isDirty = true;
}

void kr_mesh_subdivide_all(KrMesh& mesh) {
    if (mesh.faces.empty()) return;

    uint32_t V = mesh.vertices.size();
    uint32_t E = mesh.edges.size();
    uint32_t F = mesh.faces.size();

    // 1. Create edge points
    for (uint32_t i = 0; i < E; ++i) {
        KrVertex edge_pt;
        KrVertex& v1 = mesh.vertices[mesh.edges[i].v1];
        KrVertex& v2 = mesh.vertices[mesh.edges[i].v2];
        edge_pt.co[0] = (v1.co[0] + v2.co[0]) * 0.5f;
        edge_pt.co[1] = (v1.co[1] + v2.co[1]) * 0.5f;
        edge_pt.co[2] = (v1.co[2] + v2.co[2]) * 0.5f;
        edge_pt.he = KR_NULL_INDEX;
        edge_pt.flag = 0;
        mesh.vertices.push_back(edge_pt);
    }

    // 2. Create face points
    for (uint32_t i = 0; i < F; ++i) {
        KrVertex face_pt;
        face_pt.co[0] = 0.0f; face_pt.co[1] = 0.0f; face_pt.co[2] = 0.0f;
        face_pt.he = KR_NULL_INDEX;
        face_pt.flag = 0;
        
        KrFace& f = mesh.faces[i];
        if (f.flag == 1) { // deleted face
            mesh.vertices.push_back(face_pt);
            continue;
        }

        uint32_t curr = f.he_first;
        for (uint32_t j = 0; j < f.len; ++j) {
            uint32_t v_idx = mesh.half_edges[mesh.half_edges[curr].prev].v;
            face_pt.co[0] += mesh.vertices[v_idx].co[0];
            face_pt.co[1] += mesh.vertices[v_idx].co[1];
            face_pt.co[2] += mesh.vertices[v_idx].co[2];
            curr = mesh.half_edges[curr].next;
        }
        face_pt.co[0] /= f.len;
        face_pt.co[1] /= f.len;
        face_pt.co[2] /= f.len;
        mesh.vertices.push_back(face_pt);
    }

    // 3. Build new topology
    std::vector<KrFace> new_faces;
    std::vector<KrHalfEdge> new_half_edges;
    std::vector<KrEdge> new_edges;
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> emap;

    auto add_quad = [&](uint32_t v0, uint32_t v1, uint32_t v2, uint32_t v3) {
        KrFace new_f;
        uint32_t f_idx = new_faces.size();
        uint32_t he_start = new_half_edges.size();
        new_f.he_first = he_start;
        new_f.len = 4;
        new_f.flag = 0;
        new_faces.push_back(new_f);

        uint32_t quad_v[4] = {v0, v1, v2, v3};
        for (int i = 0; i < 4; ++i) {
            KrHalfEdge he;
            he.v = quad_v[(i+1)%4];
            he.f = f_idx;
            he.next = he_start + ((i+1)%4);
            he.prev = he_start + ((i+3)%4);
            he.twin = KR_NULL_INDEX;
            he.e = KR_NULL_INDEX;
            
            mesh.vertices[quad_v[i]].he = he_start + i;
            
            uint32_t va = quad_v[i], vb = quad_v[(i+1)%4];
            std::pair<uint32_t, uint32_t> k = {std::min(va, vb), std::max(va, vb)};
            if (emap.count(k)) {
                uint32_t twin_idx = emap[k];
                he.twin = twin_idx;
                new_half_edges[twin_idx].twin = he_start + i;
                he.e = new_half_edges[twin_idx].e;
            } else {
                KrEdge e;
                e.v1 = va; e.v2 = vb;
                e.he = he_start + i;
                e.flag = 0;
                new_edges.push_back(e);
                he.e = new_edges.size() - 1;
                emap[k] = he_start + i;
            }
            new_half_edges.push_back(he);
        }
    };

    for (uint32_t i = 0; i < F; ++i) {
        KrFace& f = mesh.faces[i];
        if (f.flag == 1) continue;
        
        uint32_t curr = f.he_first;
        for (uint32_t j = 0; j < f.len; ++j) {
            KrHalfEdge& he = mesh.half_edges[curr];
            uint32_t v_orig = mesh.half_edges[he.prev].v;
            uint32_t e_curr = he.e;
            uint32_t e_prev = mesh.half_edges[he.prev].e;
            
            uint32_t v0 = v_orig;
            uint32_t v1 = V + e_curr;
            uint32_t v2 = V + E + i;
            uint32_t v3 = V + e_prev;
            
            add_quad(v0, v1, v2, v3);
            
            curr = he.next;
        }
    }

    mesh.faces = std::move(new_faces);
    mesh.half_edges = std::move(new_half_edges);
    mesh.edges = std::move(new_edges);
    mesh.isDirty = true;
}
