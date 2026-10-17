#version 450
// One triangle with no vertex buffer: positions from gl_VertexIndex.
const vec2 POS[3] = vec2[3](
    vec2(-0.8, -0.8),
    vec2( 0.8, -0.8),
    vec2( 0.0,  0.8)
);
void main() {
    gl_Position = vec4(POS[gl_VertexIndex], 0.0, 1.0);
}
