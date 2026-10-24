// ROM-free GPU test of the exact GLSL embedded by CMake, with synthetic texels.
import { chromium } from '@playwright/test';
import { readFile } from 'node:fs/promises';
import assert from 'node:assert/strict';

const [vertex, template, sampler] = await Promise.all([
  readFile(new URL('../tests/sampler.vertex.glsl', import.meta.url), 'utf8'),
  readFile(new URL('../tests/sampler.fragment.glsl', import.meta.url), 'utf8'),
  readFile(new URL('../../port/web/sampler.glsl', import.meta.url), 'utf8'),
]);
const maybeChannel = process.env.MIDAS_BROWSER;
const browser = await chromium.launch({ ...(maybeChannel && maybeChannel !== 'chromium' ? { channel: maybeChannel } : {}), headless: true });
try {
  const page = await browser.newPage();
  await page.goto(process.argv[2] || 'about:blank');
  const results = await page.evaluate(({ vertex, fragment }) => {
    const canvas = document.createElement('canvas');
    canvas.width = canvas.height = 1;
    const gl = canvas.getContext('webgl2', { premultipliedAlpha: false });
    if (!gl) throw new Error('WebGL 2 is unavailable');
    function compile(type, source) {
      const shader = gl.createShader(type);
      gl.shaderSource(shader, source);
      gl.compileShader(shader);
      if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(shader));
      return shader;
    }
    const program = gl.createProgram();
    gl.attachShader(program, compile(gl.VERTEX_SHADER, vertex));
    gl.attachShader(program, compile(gl.FRAGMENT_SHADER, fragment));
    gl.linkProgram(program);
    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(program));
    gl.useProgram(program);
    const texture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, 2, 2, 0, gl.RGBA, gl.UNSIGNED_BYTE,
      Uint8Array.from([255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255]));
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    const samples = [];
    for (const [name, uv, border, expected] of [
      ['linear interior', [0.25, 0.25], [1, 1, 1], 255],
      ['linear left edge', [0, 0.25], [1, 1, 1], 128],
      ['linear right edge', [1, 0.25], [1, 1, 1], 128],
      ['linear corner', [0, 0], [1, 1, 1], 64],
      ['linear outside', [-0.25, 0.25], [1, 1, 1], 0],
      ['unbordered clamp', [-0.25, 0.25], [0, 0, 1], 255],
      ['point outside', [-0.01, 0.25], [1, 1, 0], 0],
      ['point left edge', [0, 0.25], [1, 1, 0], 255],
      ['point last texel', [0.999, 0.25], [1, 1, 0], 255],
      ['point right edge', [1, 0.25], [1, 1, 0], 0],
    ]) {
      const filter = border[2] ? gl.LINEAR : gl.NEAREST;
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, filter);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, filter);
      gl.uniform3fv(gl.getUniformLocation(program, 'uWebBorder0'), border);
      gl.uniform2fv(gl.getUniformLocation(program, 'sampleUv'), uv);
      gl.drawArrays(gl.TRIANGLES, 0, 3);
      const pixel = new Uint8Array(4);
      gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixel);
      samples.push({ name, expected, red: pixel[0], alpha: pixel[3], error: gl.getError() });
    }
    return samples;
  }, { vertex, fragment: template.replace('@SAMPLER@', sampler) });
  for (const result of results) {
    assert.equal(result.error, 0, `${result.name}: WebGL error`);
    assert.ok(Math.abs(result.red - result.expected) <= 1, `${result.name}: red=${result.red}, expected=${result.expected}`);
    assert.ok(Math.abs(result.alpha - result.expected) <= 1, `${result.name}: alpha=${result.alpha}, expected=${result.expected}`);
  }
  console.log(`WebGL transparent-border sampling: ${results.length} cases passed`);
} finally {
  await browser.close();
}
