export type BuildInfo = {
    schema: 1;
    project: string;
    commit: string;
    dirty: boolean;
    builder: string;
    build_number: number;
    build_date: string;
    built_at: string;
    source_url: string;
    workflow_url: string;
    downloads_url: string;
};
export type RuntimeManifest = {
    schema: 1;
    base: string;
    version: string;
    wasmBytes: number;
    wasmSHA256: string;
    build: BuildInfo;
};

function isBuildInfo(value: unknown): value is BuildInfo {
    if (!value || typeof value !== 'object') return false;
    const build = value as Record<string, unknown>;
    const textFields = ['project', 'builder', 'built_at'];
    const linkFields = ['source_url', 'workflow_url', 'downloads_url'];
    return (
        build.schema === 1 &&
        textFields.every((key) => typeof build[key] === 'string' && build[key].length > 0) &&
        typeof build.commit === 'string' &&
        /^(?:[a-f0-9]{40}|nogit)$/.test(build.commit) &&
        typeof build.dirty === 'boolean' &&
        typeof build.build_number === 'number' &&
        Number.isSafeInteger(build.build_number) &&
        build.build_number >= 0 &&
        typeof build.build_date === 'string' &&
        /^\d{8}$/.test(build.build_date) &&
        linkFields.every(
            (key) =>
                typeof build[key] === 'string' &&
                (build[key] === '' || /^https:\/\//.test(build[key])),
        )
    );
}

export function parseRuntimeManifest(value: unknown): RuntimeManifest {
    if (!value || typeof value !== 'object')
        throw new Error('The game build information is unavailable. Reload to try again.');
    const manifest = value as Partial<RuntimeManifest>;
    if (
        manifest.schema !== 1 ||
        typeof manifest.version !== 'string' ||
        !/^[a-f0-9]{16}$/.test(manifest.version) ||
        manifest.base !== `runtime/${manifest.version}/` ||
        !Number.isSafeInteger(manifest.wasmBytes) ||
        !manifest.wasmBytes ||
        manifest.wasmBytes < 1 ||
        manifest.wasmBytes > 64 * 1024 * 1024 ||
        typeof manifest.wasmSHA256 !== 'string' ||
        !/^[a-f0-9]{64}$/.test(manifest.wasmSHA256) ||
        !isBuildInfo(manifest.build)
    ) {
        throw new Error('This game build uses an unsupported format. Reload the page to update.');
    }
    return manifest as RuntimeManifest;
}
