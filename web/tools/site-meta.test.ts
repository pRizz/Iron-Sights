import { expect, test } from 'bun:test';
import { absoluteSiteTags, renderedSiteTag, siteUrl } from './site-meta';

test('local builds without a site address get no absolute tags', () => {
    expect(siteUrl(undefined)).toBeUndefined();
    expect(siteUrl('')).toBeUndefined();
});

test('the Pages address yields canonical, page and preview-image URLs', () => {
    const site = siteUrl('https://pRizz.github.io/Iron-Sights/');
    expect(site?.href).toBe('https://prizz.github.io/Iron-Sights/');
    expect(absoluteSiteTags(site!).map(renderedSiteTag)).toEqual([
        '<link rel="canonical" href="https://prizz.github.io/Iron-Sights/">',
        '<meta property="og:url" content="https://prizz.github.io/Iron-Sights/">',
        '<meta property="og:image" content="https://prizz.github.io/Iron-Sights/social-card.png">',
        '<meta name="twitter:image" content="https://prizz.github.io/Iron-Sights/social-card.png">',
    ]);
});

for (const value of [
    'http://prizz.github.io/Iron-Sights/',
    'https://prizz.github.io/Iron-Sights',
    'https://prizz.github.io/Iron-Sights/?ref=x',
    'https://prizz.github.io/Iron-Sights/#top',
    'not a url',
])
    test('rejects site address ' + value, () => {
        expect(() => siteUrl(value)).toThrow();
    });
