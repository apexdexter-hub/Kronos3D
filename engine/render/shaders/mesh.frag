#version 300 es
precision mediump float;

in vec3 v_Normal;
in vec3 v_FragPos;

uniform vec3 u_CameraPos;

out vec4 fragColor;

void main() {
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));
    vec3 norm = normalize(v_Normal);
    vec3 baseColor = vec3(0.6, 0.6, 0.6);
    
    // Ambient
    vec3 ambient = 0.25 * baseColor;
    
    // Diffuse
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * baseColor;
    
    // Specular (Phong)
    vec3 viewDir = normalize(u_CameraPos - v_FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = 0.15 * vec3(1.0) * spec;
    
    fragColor = vec4(ambient + diffuse + specular, 1.0);
}
