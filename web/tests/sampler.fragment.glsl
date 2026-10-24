#version 300 es
precision highp float;
@SAMPLER@
uniform sampler2D pixels;
uniform vec2 sampleUv;
out vec4 color;
void main() {
    color = webSample(pixels, sampleUv, uWebBorder0);
}
