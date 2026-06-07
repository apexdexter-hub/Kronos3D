#version 300 es
precision mediump float;
in vec3 v_Normal;
in vec3 v_FragPos;
uniform vec3 u_CameraPos;
out vec4 FragColor;

void main() {
    vec3 baseColor = vec3(0.85, 0.85, 0.85);
    vec3 norm = normalize(v_Normal);
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));
    
    // Ambient
    vec3 ambient = 0.4 * baseColor;
    
    // Diffuse suave (no completamente negro en sombra)
    float diff = max(dot(norm, lightDir), 0.0);
    float diffSoft = diff * 0.5 + 0.5; // wrap lighting
    vec3 diffuse = diffSoft * baseColor * 0.6;
    
    // Specular sutil
    vec3 viewDir = normalize(u_CameraPos - v_FragPos);
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfDir), 0.0), 32.0);
    vec3 specular = spec * vec3(0.1);
    
    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
