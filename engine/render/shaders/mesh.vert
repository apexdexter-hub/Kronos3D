#version 300 es
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;

uniform mat4 u_MVP;
uniform mat4 u_Model;

out vec3 v_Normal;
out vec3 v_FragPos;

void main() {
    v_FragPos = vec3(u_Model * vec4(a_Position, 1.0));
    // Calculate normal matrix in shader to preserve lighting under deformation/rotation
    v_Normal = mat3(transpose(inverse(u_Model))) * a_Normal;
    gl_Position = u_MVP * vec4(a_Position, 1.0);
}
