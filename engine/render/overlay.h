#ifndef KRONOS_OVERLAY_H
#define KRONOS_OVERLAY_H

#include <GLES3/gl32.h>
#include <vector>

class KrOverlay {
public:
    KrOverlay();
    ~KrOverlay();
    void init();
    void render(GLuint program, const float* mvp_matrix);

private:
    GLuint grid_vao;
    GLuint grid_vbo;
    GLuint axis_vao;
    GLuint axis_vbo;
    int grid_vertex_count;
};

#endif // KRONOS_OVERLAY_H
