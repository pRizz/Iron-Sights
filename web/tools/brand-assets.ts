// Generated images that the site publishes at fixed paths from web/public. Their
// digests come from scripts/port/brand-art.sha256, which make_brand_art.py writes.
const PUBLIC_PREFIX = 'web/public/';

export type BrandAsset = { path: string; sha256: string };

export function publicBrandAssets(checksums: string): BrandAsset[] {
    const assets: BrandAsset[] = [];
    for (const line of checksums.split('\n')) {
        if (!line) continue;
        const match = /^([0-9a-f]{64}) {2}(\S+)$/.exec(line);
        if (!match) throw new Error('Invalid brand checksum line: ' + line);
        const [, sha256, source] = match;
        if (!source.startsWith(PUBLIC_PREFIX)) continue;
        const path = source.slice(PUBLIC_PREFIX.length);
        if (!/^(?:icons\/)?[a-z0-9-]+\.png$/.test(path))
            throw new Error('Unexpected public brand asset: ' + source);
        assets.push({ path, sha256 });
    }
    if (!assets.length) throw new Error('No public brand assets recorded');
    return assets;
}
