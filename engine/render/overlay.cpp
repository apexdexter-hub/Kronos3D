#include "overlay.h"

KrOverlay::KrOverlay() : grid_vao(0), grid_vbo(0), axis_vao(0), axis_vbo(0), grid_vertex_count(0) {}

KrOverlay::~KrOverlay() {
    if (grid_vbo) glDeleteBuffers(1, &grid_vbo);
    if (grid_vao) glDeleteVertexArrays(1, &grid_vao);
    if (axis_vbo) glDeleteBuffers(1, &axis_vbo);
    if (axis_vao) glDeleteVertexArrays(1, &axis_vao);
}

void KrOverlay::init() {
    // Generate Grid 20x20 on Y=0
    std::vector<float> grid_vertices;
    float step = 1.0f;
    int size = 10;
    for (int i = -size; i <= size; ++i) {
        // Line along X
        grid_vertices.push_back((float)i * step); grid_vertices.push_back(0.0f); grid_vertices.push_back(-(float)size * step);
        grid_vertices.push_back((float)i * step); grid_vertices.push_back(0.0f); grid_vertices.push_back((float)size * step);

        // Line along Z
        grid_vertices.push_back(-(float)size * step); grid_vertices.push_back(0.0f); grid_vertices.push_back((float)i * step);
        grid_vertices.push_back((float)size * step);  grid_vertices.push_back(0.0f); grid_vertices.push_back((float)i * step);
    }
    grid_vertex_count = grid_vertices.size() / 3;

    glGenVertexArrays(1, &grid_vao);
    glGenBuffers(1, &grid_vbo);

    glBindVertexArray(grid_vao);
    glBindBuffer(GL_ARRAY_BUFFER, grid_vbo);
    glBufferData(GL_ARRAY_BUFFER, grid_vertices.size() * sizeof(float), grid_vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Generate Axes (X: Red, Z: Blue)
    // Eje X: Red line from (-10, 0, 0) to (10, 0, 0)
    // Eje Z: Blue line from (0, 0, -10) to (0, 0, 10)
    float axis_vertices[] = {
        -10.0f, 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, // X
         0.0f, 0.0f, -10.0f, 0.0f, 0.0f, 10.0f  // Z
    };

    glGenVertexArrays(1, &axis_vao);
    glGenBuffers(1, &axis_vbo);

    glBindVertexArray(axis_vao);
    glBindBuffer(GL_ARRAY_BUFFER, axis_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(axis_vertices), axis_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void KrOverlay::render(GLuint program, const float* mvp_matrix) {
    glUseProgram(program);
    
    GLint mvp_loc = glGetUniformLocation(program, "u_MVP");
    GLint color_loc = glGetUniformLocation(program, "u_Color");
    
    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, mvp_matrix);

    // Draw Grid (Gray)
    glUniform4f(color_loc, 0.3f, 0.3f, 0.3f, 0.8f);
    glBindVertexArray(grid_vao);
    glDrawArrays(GL_LINES, 0, grid_vertex_count);

    // Draw Axis lines
    glBindVertexArray(axis_vao);
    
    // Draw Axis X (Red)
    glUniform4f(color_loc, 0.8f, 0.2f, 0.2f, 1.0f);
    glDrawArrays(GL_LINES, 0, 2);

    // Draw Axis Z (Blue)
    glUniform4f(color_loc, 0.2f, 0.2f, 0.8f, 1.0f);
    glDrawArrays(GL_LINES, 2, 2);

    glBindVertexArray(0);
}
