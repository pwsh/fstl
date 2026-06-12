#version 120

uniform float zoom;
uniform float model_alpha;

varying vec3 ec_pos;

void main() {
    gl_FragColor = vec4(1.0, 1.0, 1.0, model_alpha);
}
