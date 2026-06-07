#version 300 es
precision mediump float;

in vec3 v_Normal;
in vec3 v_FragPos;

uniform vec3 u_CameraPos;

out vec4 fragColor;

void main() {
    // Light direcional from top-right-front
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));
    vec3 norm = normalize(v_Normal);
    vec3 baseColor = vec3(0.85, 0.85, 0.85);
    
    // Ambient (0.4 * baseColor)
    vec3 ambient = 0.4 * baseColor;
    
    // Diffuse (max(dot(normal, lightDir), 0.0) * baseColor)
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * baseColor;
    
    // Specular (pow(max(dot(viewDir, reflectDir), 0.0), 16.0) * 0.1)
    vec3 viewDir = normalize(u_CameraPos - v_FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);
    vec3 specular = 0.1 * vec3(1.0) * spec;
    
    fragColor = vec4(ambient + diffuse + specular, 1.0);
}
