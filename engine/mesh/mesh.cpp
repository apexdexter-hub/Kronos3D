#include "mesh.h"

KrMesh kr_mesh_create_cube() {
    KrMesh mesh;
    mesh.vertices.reserve(24);
    mesh.faces.reserve(12);

    // Front Face (Z+)
    mesh.vertices.push_back({-0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f});
    mesh.vertices.push_back({ 0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f});
    mesh.vertices.push_back({ 0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f});
    mesh.vertices.push_back({-0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f});
    
    // Back Face (Z-)
    mesh.vertices.push_back({ 0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f});
    mesh.vertices.push_back({-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f});
    mesh.vertices.push_back({-0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f});
    mesh.vertices.push_back({ 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f});

    // Top Face (Y+)
    mesh.vertices.push_back({-0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f});
    mesh.vertices.push_back({ 0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f});
    mesh.vertices.push_back({ 0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f});
    mesh.vertices.push_back({-0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f});

    // Bottom Face (Y-)
    mesh.vertices.push_back({-0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f});
    mesh.vertices.push_back({ 0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f});
    mesh.vertices.push_back({ 0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f});
    mesh.vertices.push_back({-0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f});

    // Right Face (X+)
    mesh.vertices.push_back({ 0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f});
    mesh.vertices.push_back({ 0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f});
    mesh.vertices.push_back({ 0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f});
    mesh.vertices.push_back({ 0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f});

    // Left Face (X-)
    mesh.vertices.push_back({-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f});
    mesh.vertices.push_back({-0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f});
    mesh.vertices.push_back({-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f});
    mesh.vertices.push_back({-0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f});

    // Add indices (2 triangles per face)
    for (unsigned int i = 0; i < 6; ++i) {
        unsigned int offset = i * 4;
        mesh.faces.push_back({offset + 0, offset + 1, offset + 2});
        mesh.faces.push_back({offset + 0, offset + 2, offset + 3});
    }

    return mesh;
}
