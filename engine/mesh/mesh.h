#ifndef KRONOS_MESH_H
#define KRONOS_MESH_H

#include <vector>
#include <atomic>
#include <cstdint>

constexpr uint32_t KR_NULL_INDEX = 0xFFFFFFFF;

struct alignas(16) KrVertex {
    float co[3];
    float no[3];
    uint32_t he;
    uint32_t flag;
};

struct alignas(16) KrEdge {
    uint32_t v1, v2;
    uint32_t he;
    uint32_t flag;
};

struct alignas(16) KrHalfEdge {
    uint32_t v;
    uint32_t e;
    uint32_t f;
    uint32_t next;
    uint32_t prev;
    uint32_t twin;
    uint32_t _pad[2];
};

struct alignas(16) KrFace {
    uint32_t he_first;
    uint32_t len;
    float no[3];
    uint32_t flag;
    uint32_t _pad[2];
};

struct KrMesh {
    std::vector<KrVertex> vertices;
    std::vector<KrEdge> edges;
    std::vector<KrHalfEdge> half_edges;
    std::vector<KrFace> faces;
    bool isDirty = false;
};

enum KrEditMode { OBJECT_MODE, EDIT_MODE };
enum KrToolMode { TOOL_SELECT, TOOL_MOVE, TOOL_ROTATE, TOOL_SCALE, TOOL_EXTRUDE, TOOL_SUBDIVIDE };

KrMesh kr_mesh_create_cube();
int kr_mesh_raycast(const KrMesh& mesh, const float* model, const float* view, const float* proj, const float* ro, const float* rd);
void kr_mesh_subdivide_face(KrMesh& mesh, int face_id);
void kr_mesh_subdivide_all(KrMesh& mesh);
void kr_mesh_extrude_face(KrMesh& mesh, int face_id, float distance);
void kr_mesh_move_vertex(KrMesh& mesh, int vertex_id, float dx, float dz);
std::vector<float> kr_mesh_to_render_buffer(KrMesh& mesh);

#endif
