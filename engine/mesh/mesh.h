#ifndef KRONOS_MESH_H
#define KRONOS_MESH_H

#include <vector>

struct KrVertex {
    float x, y, z;
    float nx, ny, nz;
};

struct KrFace {
    unsigned int v0, v1, v2, v3;
    KrFace() : v0(0), v1(0), v2(0), v3((unsigned int)-1) {}
    KrFace(unsigned int a, unsigned int b, unsigned int c) : v0(a), v1(b), v2(c), v3((unsigned int)-1) {}
    KrFace(unsigned int a, unsigned int b, unsigned int c, unsigned int d) : v0(a), v1(b), v2(c), v3(d) {}
    bool is_quad() const { return v3 != (unsigned int)-1; }
};

struct KrMesh {
    std::vector<KrVertex> vertices;
    std::vector<KrFace> faces;
};

enum KrEditMode {
    OBJECT_MODE,
    EDIT_MODE
};

enum KrToolMode {
    TOOL_SELECT,
    TOOL_MOVE,
    TOOL_EXTRUDE,
    TOOL_SUBDIVIDE
};

// Generates a Phong-lit cube with distinct normals per face (24 vertices, 12 triangles)
KrMesh kr_mesh_create_cube();

// Raycast: returns selected face index or -1 if no face is hit
int kr_mesh_raycast(const KrMesh& mesh, const float* model_matrix, const float* view_matrix, const float* projection_matrix, const float* ray_origin, const float* ray_dir);

// Operators
void kr_mesh_subdivide_face(KrMesh& mesh, int face_id);
void kr_mesh_subdivide_all(KrMesh& mesh);
void kr_mesh_extrude_face(KrMesh& mesh, int face_id, float distance);
void kr_mesh_move_vertex(KrMesh& mesh, int vertex_id, float dx, float dz);

#endif // KRONOS_MESH_H
