#version 300 es
void main() {
    float x = gl_VertexID == 1 ? 3.0 : -1.0;
    float y = gl_VertexID == 2 ? 3.0 : -1.0;
    gl_Position = vec4(x, y, 0.0, 1.0);
}
