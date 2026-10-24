// WebGL has no CLAMP_TO_BORDER. Match transparent-border sampling in the shader.
uniform vec3 uWebBorder0;
uniform vec3 uWebBorder1;
vec4 webSample(sampler2D tex, vec2 uv, vec3 border) {
    vec2 edge = min(uv, vec2(1.0) - uv);
    vec2 linearCoverage = clamp(edge * vec2(textureSize(tex, 0)) + vec2(0.5), 0.0, 1.0);
    vec2 pointCoverage = step(vec2(0.0), uv) * (vec2(1.0) - step(vec2(1.0), uv));
    vec2 coverage = mix(pointCoverage, linearCoverage, border.z);
    coverage = mix(vec2(1.0), coverage, border.xy);
    return texture(tex, uv) * coverage.x * coverage.y;
}
#undef SAMPLE_TEX
#define SAMPLE_TEX(tex, uv) webSample(tex, uv, webBorder)
#define BORDER_PARAM , vec3 webBorder
#define BORDER_ARG , webBorder
#define WEB_BORDER0 , uWebBorder0
#define WEB_BORDER1 , uWebBorder1
