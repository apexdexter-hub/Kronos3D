#include "mesh.h"
#include <map>
#include <utility>
#include <cmath>

struct KrVec3 {
    float x, y, z;
    KrVec3 operator-(const KrVec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
};

KrVec3 kr_cross_product(const KrVec3& a, const KrVec3& b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}

KrVec3 kr_normalize(const KrVec3& v) {
    float l = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    if(l < 0.0001f) return {0,1,0};
    return {v.x/l, v.y/l, v.z/l};
}

KrMesh kr_mesh_create_cube() {
    KrMesh mesh;
    mesh.vertices.resize(8);
    mesh.faces.resize(6);
    mesh.half_edges.resize(24);
    mesh.edges.reserve(12);

    float pos[8][3] = {{-1,-1,1}, {1,-1,1}, {1,1,1}, {-1,1,1}, {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1}};
    for(int i=0; i<8; i++){
        mesh.vertices[i].co[0]=pos[i][0]; mesh.vertices[i].co[1]=pos[i][1]; mesh.vertices[i].co[2]=pos[i][2];
        mesh.vertices[i].he = KR_NULL_INDEX; mesh.vertices[i].flag = 0;
    }

    uint32_t indices[6][4] = {{0,1,2,3}, {1,5,6,2}, {5,4,7,6}, {4,0,3,7}, {3,2,6,7}, {4,5,1,0}};
    std::map<std::pair<uint32_t,uint32_t>, uint32_t> emap;
    int he_idx = 0;

    for(int f=0; f<6; f++){
        mesh.faces[f].he_first = he_idx;
        mesh.faces[f].len = 4;
        mesh.faces[f].flag = 0;
        for(int i=0; i<4; i++){
            uint32_t v1 = indices[f][i], v2 = indices[f][(i+1)%4];
            KrHalfEdge& he = mesh.half_edges[he_idx+i];
            he.v = v2; he.f = f; he.next = he_idx+((i+1)%4); he.prev = he_idx+((i+3)%4);
            he.twin = KR_NULL_INDEX; he.e = KR_NULL_INDEX;
            mesh.vertices[v1].he = he_idx+i;
            
            std::pair<uint32_t,uint32_t> k = {std::min(v1,v2), std::max(v1,v2)};
            if(emap.count(k)){
                uint32_t t = emap[k];
                he.twin = t; mesh.half_edges[t].twin = he_idx+i;
                he.e = mesh.half_edges[t].e;
            } else {
                mesh.edges.push_back({v1,v2,(uint32_t)he_idx+i,0});
                he.e = mesh.edges.size()-1;
                emap[k] = he_idx+i;
            }
        }
        he_idx += 4;
    }
    mesh.isDirty = true;
    return mesh;
}

std::vector<float> kr_mesh_to_render_buffer(KrMesh& mesh) {
    std::vector<float> buf;
    buf.reserve(mesh.faces.size() * 36);
    for(auto& f : mesh.faces){
        if(f.len < 3 || f.flag == 1) continue;
        uint32_t first = f.he_first;
        const KrHalfEdge* curr = &mesh.half_edges[first];
        const KrVertex& v0 = mesh.vertices[mesh.half_edges[curr->prev].v];
        
        KrVec3 p0 = {v0.co[0], v0.co[1], v0.co[2]};
        KrVec3 p1 = {mesh.vertices[curr->v].co[0], mesh.vertices[curr->v].co[1], mesh.vertices[curr->v].co[2]};
        KrVec3 p2 = {mesh.vertices[mesh.half_edges[curr->next].v].co[0], mesh.vertices[mesh.half_edges[curr->next].v].co[1], mesh.vertices[mesh.half_edges[curr->next].v].co[2]};
        KrVec3 norm = kr_normalize(kr_cross_product(p1-p0, p2-p0));

        for(uint32_t i=0; i<f.len-2; i++){
            const KrVertex& cv = mesh.vertices[curr->v];
            const KrVertex& nv = mesh.vertices[mesh.half_edges[curr->next].v];
            
            buf.push_back(v0.co[0]); buf.push_back(v0.co[1]); buf.push_back(v0.co[2]);
            buf.push_back(norm.x); buf.push_back(norm.y); buf.push_back(norm.z);
            buf.push_back(cv.co[0]); buf.push_back(cv.co[1]); buf.push_back(cv.co[2]);
            buf.push_back(norm.x); buf.push_back(norm.y); buf.push_back(norm.z);
            buf.push_back(nv.co[0]); buf.push_back(nv.co[1]); buf.push_back(nv.co[2]);
            buf.push_back(norm.x); buf.push_back(norm.y); buf.push_back(norm.z);
            curr = &mesh.half_edges[curr->next];
        }
    }
    mesh.isDirty = false;
    return buf;
}
