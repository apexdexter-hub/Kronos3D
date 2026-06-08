#include "shader.h"
#include <android/log.h>
#include <stdlib.h>

#define LOG_TAG "Kronos3D_Shader"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

GLuint kr_shader_compile(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint log_len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
        char* log = nullptr;
        if (log_len > 0) {
            log = (char*)malloc(log_len);
            if (log) {
                glGetShaderInfoLog(shader, log_len, nullptr, log);
            }
        }
        LOGE("Shader compile error: %s", log ? log : "Unknown compilation error");
        if (log) free(log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint kr_program_create(const char* vert_src, const char* frag_src) {
    GLuint vert = kr_shader_compile(GL_VERTEX_SHADER, vert_src);
    if (!vert) return 0;
    
    GLuint frag = kr_shader_compile(GL_FRAGMENT_SHADER, frag_src);
    if (!frag) {
        glDeleteShader(vert);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint log_len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
        char* log = nullptr;
        if (log_len > 0) {
            log = (char*)malloc(log_len);
            if (log) {
                glGetProgramInfoLog(program, log_len, nullptr, log);
            }
        }
        LOGE("Program linking error: %s", log ? log : "Unknown linking error");
        if (log) free(log);
        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
    return program;
}
