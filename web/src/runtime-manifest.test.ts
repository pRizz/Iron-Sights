import { describe, expect, test } from 'bun:test';
import { parseRuntimeManifest, type RuntimeManifest } from './runtime-manifest';

const manifest = {
    schema: 1,
    version: '1234567890abcdef',
    base: 'runtime/1234567890abcdef/',
    wasmBytes: 1234,
    wasmSHA256: 'a'.repeat(64),
    build: {
        schema: 1,
        project: 'Project Midas ADS',
        commit: 'b'.repeat(40),
        dirty: false,
        builder: 'GitHub Actions',
        build_number: 42,
        build_date: '20261024',
        built_at: '2026-10-24T22:28:40Z',
        source_url: 'https://github.com/pRizz/Project-Midas-ADS',
        workflow_url: '',
        downloads_url: '',
    },
} satisfies RuntimeManifest;

describe('runtime manifest boundary', () => {
    test('accepts the pinned assembly format', () => {
        expect(parseRuntimeManifest(manifest)).toEqual(manifest);
    });
    test.each([
        { base: '../private/' },
        { wasmBytes: -1 },
        { wasmBytes: 1.5 },
        { wasmBytes: 65 * 1024 * 1024 },
        { wasmSHA256: 'damaged' },
        { schema: 2 },
    ])('rejects invalid download metadata %j', (change) => {
        expect(() => parseRuntimeManifest({ ...manifest, ...change })).toThrow();
    });
    test.each([
        { commit: undefined },
        { dirty: 'false' },
        { build_number: -1 },
        { builder: null },
        { build_date: 'yesterday' },
        { built_at: '' },
        { source_url: 'javascript:alert(1)' },
        { workflow_url: 42 },
    ])('rejects malformed provenance %j', (change) => {
        expect(() =>
            parseRuntimeManifest({ ...manifest, build: { ...manifest.build, ...change } }),
        ).toThrow();
    });
});
