#ifndef KRONOS_SHADER_H
#define KRONOS_SHADER_H

#include <GLES3/gl32.h>

GLuint kr_shader_compile(GLenum type, const char* source);
GLuint kr_program_create(const char* vert_src, const char* frag_src);

#endif // KRONOS_SHADER_H
