uniform sampler2D screen;
uniform float time;
uniform float motion;
uniform float power;
varying vec2 vUv;
void main() {
    vec2 corner = abs(vUv - 0.5) - vec2(0.455, 0.445);
    if (length(max(corner, 0.0)) + min(max(corner.x, corner.y), 0.0) > 0.045) discard;
    float roll = exp(-pow((fract(vUv.y + time * 0.045) - 0.1) * 32.0, 2.0)) * motion;
    vec2 uv = vUv + vec2(roll * sin(vUv.y * 160.0 + time * 9.0) * 0.002, 0.0);
    vec3 color = texture2D(screen, uv).rgb;
    color.r = texture2D(screen, uv + vec2(0.0008, 0.0)).r;
    color.b = texture2D(screen, uv - vec2(0.0008, 0.0)).b;
    float scan = 0.94 + 0.06 * sin(vUv.y * 480.0 * 3.14159265);
    float field = mod(floor(time * 30.0), 2.0);
    scan *= 1.0 - motion * 0.035 * mod(floor(vUv.y * 480.0) + field, 2.0);
    float vignette = 1.0 - 0.35 * pow(length(vUv - 0.5), 1.5);
    float noise = fract(sin(dot(vUv + fract(time), vec2(12.9898, 78.233))) * 43758.5453);
    color *= scan * vignette * (1.0 + sin(time * 23.0) * 0.012 * motion);
    color += (noise - 0.5) * 0.018 * motion + roll * 0.025;
    if (power > 0.01 && power < 0.42) {
        float line = exp(-pow((vUv.y - 0.5) * 95.0, 2.0));
        color = vec3(line * sin(power / 0.42 * 3.14159265) * 0.7);
    }
    gl_FragColor = vec4(color, 1.0);
    #include <tonemapping_fragment>
    #include <colorspace_fragment>
}
