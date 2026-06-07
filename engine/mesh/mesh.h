#ifndef KRONOS_MESH_H
#define KRONOS_MESH_H

#include <vector>

struct KrVertex {
    float x, y, z;
    float nx, ny, nz;
};

struct KrFace {
    unsigned int v0, v1, v2;
};

struct KrMesh {
    std::vector<KrVertex> vertices;
    std::vector<KrFace> faces;
};

// Generates a Phong-lit cube with distinct normals per face (24 vertices, 12 triangles)
KrMesh kr_mesh_create_cube();

#endif // KRONOS_MESH_H
