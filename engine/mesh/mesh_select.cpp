#include "mesh.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <limits>

// Helper to check intersection of ray with a triangle
bool ray_triangle_intersect(
    const glm::vec3& ray_origin, const glm::vec3& ray_dir,
    const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
    float& t) 
{
    const float EPSILON = 0.0000001f;
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 h = glm::cross(ray_dir, edge2);
    float a = glm::dot(edge1, h);
    if (a > -EPSILON && a < EPSILON)
        return false; // Ray is parallel to this triangle.
    
    float f = 1.0f / a;
    glm::vec3 s = ray_origin - v0;
    float u = f * glm::dot(s, h);
    if (u < 0.0f || u > 1.0f)
        return false;
        
    glm::vec3 q = glm::cross(s, edge1);
    float v = f * glm::dot(ray_dir, q);
    if (v < 0.0f || u + v > 1.0f)
        return false;
        
    // At this stage we can compute t to find out where the intersection point is on the line.
    float temp_t = f * glm::dot(edge2, q);
    if (temp_t > EPSILON) { // ray intersection
        t = temp_t;
        return true;
    }
    return false;
}

int kr_mesh_raycast(
    const KrMesh& mesh, 
    const float* model_matrix, 
    const float* view_matrix, 
    const float* projection_matrix, 
    const float* ray_origin_in, 
    const float* ray_dir_in) 
{
    glm::mat4 model = glm::make_mat4(model_matrix);
    
    glm::vec3 ray_origin = glm::make_vec3(ray_origin_in);
    glm::vec3 ray_dir = glm::make_vec3(ray_dir_in);

    float min_t = std::numeric_limits<float>::max();
    int hit_face_idx = -1;

    for (size_t i = 0; i < mesh.faces.size(); ++i) {
        const KrFace& f = mesh.faces[i];
        
        // Transform vertices to world coordinates
        glm::vec3 v0 = glm::vec3(model * glm::vec4(mesh.vertices[f.v0].x, mesh.vertices[f.v0].y, mesh.vertices[f.v0].z, 1.0f));
        glm::vec3 v1 = glm::vec3(model * glm::vec4(mesh.vertices[f.v1].x, mesh.vertices[f.v1].y, mesh.vertices[f.v1].z, 1.0f));
        glm::vec3 v2 = glm::vec3(model * glm::vec4(mesh.vertices[f.v2].x, mesh.vertices[f.v2].y, mesh.vertices[f.v2].z, 1.0f));

        float t = 0.0f;
        if (ray_triangle_intersect(ray_origin, ray_dir, v0, v1, v2, t)) {
            if (t < min_t) {
                min_t = t;
                hit_face_idx = (int)i;
            }
        }
    }

    return hit_face_idx;
}
