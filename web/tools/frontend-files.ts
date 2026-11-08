// Parse Vite's output graph without permitting arbitrary extra public files.
// The only non-code assets are the launcher icons that
// scripts/port/make_brand_art.py generates into web/icons.
export const ICON_SOURCES = [
    { source: 'icons/favicon.ico', output: /^assets\/favicon-[a-zA-Z0-9_-]+\.ico$/ },
    {
        source: 'icons/apple-touch-icon.png',
        output: /^assets\/apple-touch-icon-[a-zA-Z0-9_-]+\.png$/,
    },
] as const;

export function frontendFiles(value: unknown): string[] {
    if (!value || typeof value !== 'object' || Array.isArray(value))
        throw new Error('Invalid frontend manifest');
    const entries = Object.entries(value);
    const keys = new Set(entries.map(([key]) => key));
    const files = new Set<string>();
    const explicitAssets = new Set<string>();
    for (const { source, output } of ICON_SOURCES) {
        const maybeIcon = entries.find(([name]) => name === source)?.[1];
        if (!maybeIcon) continue;
        if (
            maybeIcon.src !== source ||
            typeof maybeIcon.file !== 'string' ||
            !output.test(maybeIcon.file)
        )
            throw new Error('Invalid launcher icon artifact');
        explicitAssets.add(maybeIcon.file);
    }
    const add = (path: unknown) => {
        if (
            typeof path !== 'string' ||
            (!/^assets\/[a-zA-Z0-9_-]+\.(?:js|css)$/.test(path) && !explicitAssets.has(path))
        )
            throw new Error('Invalid frontend artifact path');
        files.add(path);
    };
    for (const [, entry] of entries) {
        if (!entry || typeof entry !== 'object' || Array.isArray(entry))
            throw new Error('Invalid frontend chunk');
        add(entry.file);
        if (entry.assets !== undefined) {
            if (
                !Array.isArray(entry.assets) ||
                entry.assets.some(
                    (path: unknown) => typeof path !== 'string' || !explicitAssets.has(path),
                )
            )
                throw new Error('Non-code frontend assets require an explicit asset record');
            entry.assets.forEach(add);
        }
        if (entry.css !== undefined) {
            if (!Array.isArray(entry.css)) throw new Error('Invalid frontend CSS');
            entry.css.forEach(add);
        }
        for (const kind of ['imports', 'dynamicImports']) {
            const references = entry[kind];
            if (references === undefined) continue;
            if (
                !Array.isArray(references) ||
                references.some((key: unknown) => typeof key !== 'string' || !keys.has(key))
            )
                throw new Error('Dangling frontend chunk reference');
        }
    }
    if (!files.size) throw new Error('Empty frontend manifest');
    return [...files];
}
