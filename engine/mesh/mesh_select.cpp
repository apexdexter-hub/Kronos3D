#include "mesh.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <limits>

bool ray_triangle_intersect(const glm::vec3& ro, const glm::vec3& rd, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, float& t) {
    glm::vec3 e1 = v1 - v0;
    glm::vec3 e2 = v2 - v0;
    glm::vec3 h = glm::cross(rd, e2);
    float a = glm::dot(e1, h);
    if(a > -0.00001f && a < 0.00001f) return false;
    float f = 1.0f / a;
    glm::vec3 s = ro - v0;
    float u = f * glm::dot(s, h);
    if(u < 0.0f || u > 1.0f) return false;
    glm::vec3 q = glm::cross(s, e1);
    float v = f * glm::dot(rd, q);
    if(v < 0.0f || u + v > 1.0f) return false;
    float tt = f * glm::dot(e2, q);
    if(tt > 0.00001f){ t = tt; return true; }
    return false;
}

int kr_mesh_raycast(const KrMesh& mesh, const float* model, const float* view, const float* proj, const float* ro, const float* rd) {
    glm::mat4 m = glm::make_mat4(model);
    glm::mat4 mInv = glm::inverse(m);
    glm::vec3 ro_w = glm::make_vec3(ro);
    glm::vec3 rd_w = glm::make_vec3(rd);
    glm::vec3 ro_l = glm::vec3(mInv * glm::vec4(ro_w, 1.0f));
    glm::vec3 rd_l = glm::normalize(glm::vec3(mInv * glm::vec4(rd_w, 0.0f)));

    float min_t = std::numeric_limits<float>::max();
    int hit = -1;
    for(size_t i=0; i<mesh.faces.size(); i++){
        if(mesh.faces[i].len < 3 || mesh.faces[i].flag == 1) continue;
        uint32_t curr = mesh.faces[i].he_first;
        uint32_t v0 = mesh.half_edges[mesh.half_edges[curr].prev].v;
        glm::vec3 p0(mesh.vertices[v0].co[0], mesh.vertices[v0].co[1], mesh.vertices[v0].co[2]);
        curr = mesh.half_edges[curr].next;
        for(uint32_t j=1; j<mesh.faces[i].len-1; j++){
            uint32_t v1 = mesh.half_edges[mesh.half_edges[curr].prev].v;
            uint32_t v2 = mesh.half_edges[curr].v;
            glm::vec3 p1(mesh.vertices[v1].co[0], mesh.vertices[v1].co[1], mesh.vertices[v1].co[2]);
            glm::vec3 p2(mesh.vertices[v2].co[0], mesh.vertices[v2].co[1], mesh.vertices[v2].co[2]);
            float t=0;
            if(ray_triangle_intersect(ro_l, rd_l, p0, p1, p2, t)){
                if(t < min_t){ min_t = t; hit = i; }
            }
            curr = mesh.half_edges[curr].next;
        }
    }
    return hit;
}
