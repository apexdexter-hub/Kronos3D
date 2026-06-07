#version 300 es
precision mediump float;

in vec3 v_Normal;
in vec3 v_FragPos;

out vec4 fragColor;

void main() {
    // Light direction (0.5, 1.0, 0.5) normalized
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));
    vec3 norm = normalize(v_Normal);
    
    // Ambient
    float ambient = 0.3;
    
    // Diffuse
    float diff = max(dot(norm, lightDir), 0.0);
    
    // Base color is light gray (0.7, 0.7, 0.7)
    vec3 baseColor = vec3(0.7, 0.7, 0.7);
    vec3 result = baseColor * (ambient + diff);
    
    fragColor = vec4(result, 1.0);
}
