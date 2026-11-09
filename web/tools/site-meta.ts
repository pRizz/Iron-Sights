// Search and link-preview metadata that needs absolute URLs. Only the Pages
// workflow knows the deployed address (MIDAS_SITE_URL); local builds omit these
// tags rather than pointing at someone else's deployment.
export const SOCIAL_CARD = 'social-card.png';
export const WEB_MANIFEST = 'manifest.webmanifest';

export type SiteTag = { tag: 'link' | 'meta'; attrs: Record<string, string> };

export function siteUrl(value: string | undefined): URL | undefined {
    if (!value) return undefined;
    const url = new URL(value);
    if (url.protocol !== 'https:' || url.search || url.hash || !url.pathname.endsWith('/'))
        throw new Error('MIDAS_SITE_URL must be an https URL ending in / without a query');
    return url;
}

export function absoluteSiteTags(site: URL): SiteTag[] {
    const image = new URL(SOCIAL_CARD, site).href;
    return [
        { tag: 'link', attrs: { rel: 'canonical', href: site.href } },
        { tag: 'meta', attrs: { property: 'og:url', content: site.href } },
        { tag: 'meta', attrs: { property: 'og:image', content: image } },
        { tag: 'meta', attrs: { name: 'twitter:image', content: image } },
    ];
}

// The tags as they appear in the built page, for the artifact verifier.
export function renderedSiteTag({ tag, attrs }: SiteTag): string {
    const escape = (value: string) => value.replaceAll('&', '&amp;').replaceAll('"', '&quot;');
    return `<${tag} ${Object.entries(attrs)
        .map(([name, value]) => `${name}="${escape(value)}"`)
        .join(' ')}>`;
}
