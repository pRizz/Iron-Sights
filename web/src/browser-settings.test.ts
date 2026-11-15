import { expect, test } from 'bun:test';
import {
    browserGraphicsDefaultMigration,
    browserLookDefault,
    browserMobileDefaults,
    browserTouchGraphicsDefaults,
    mobileResolution,
    ensureBrowserSettings,
} from './browser-settings';
import type { GameModule } from './runtime-types';

test('partial browser bootstrap config explicitly defaults to Upright', () => {
    expect(browserLookDefault('[display]\nmaxfps=60\n[controls]\nstart=Return,P\n')).toBe(
        '[display]\nlook_updown=1\nmaxfps=60\n[controls]\nstart=Return,P\n',
    );
});

test('mobile resolution uses the full pixel ratio with proportional presentation', () => {
    // Arrange / Act / Assert
    for (const dpr of [1, 2, 3, 1.5]) {
        expect(mobileResolution(1024, 768, dpr)).toEqual({ width: 1024 * dpr, height: 576 * dpr });
        expect(mobileResolution(390, 844, dpr)).toEqual({
            width: Math.round(390 * dpr),
            height: Math.round(219.375 * dpr),
        });
    }
    expect(mobileResolution(844, 390, 2)).toEqual({ width: 1387, height: 780 });
    for (const dpr of [NaN, Infinity, 0, -1])
        expect(mobileResolution(1024, 768, dpr)).toEqual({ width: 1024, height: 576 });
});

test('mobile defaults upgrade the legacy resolution without changing quality or aim preferences', () => {
    // Arrange
    const settings =
        '[display]\r\nwindow_width=960\r\nwindow_height=540\r\nmouse_auto_aim=0\r\nlow_res=240\r\nmsaa=0\r\n[input]\r\nmouse=1\r\n';
    // Act
    const next = browserMobileDefaults(settings, { width: 2048, height: 1152 }, true);
    // Assert
    expect(next).toBe(
        settings.replace('width=960', 'width=2048').replace('height=540', 'height=1152'),
    );
    expect(browserMobileDefaults(next, { width: 3072, height: 1728 }, true)).toBe(next);
});

test('mobile defaults preserve explicit aim choices and custom or migrated resolutions', () => {
    // Arrange / Act / Assert
    for (const aim of [0, 1]) {
        const custom = `[display]\nwindow_width=1280\nwindow_height=720\nmouse_auto_aim=${aim}\n`;
        expect(browserMobileDefaults(custom, { width: 2048, height: 1152 }, true)).toBe(custom);
        const deliberateLegacy = custom.replace('1280', '960').replace('720', '540');
        expect(browserMobileDefaults(deliberateLegacy, { width: 2048, height: 1152 }, false)).toBe(
            deliberateLegacy,
        );
    }
    expect(
        browserMobileDefaults('[input]\nmouse=1\n', { width: 2048, height: 1152 }, true),
    ).toContain('mouse_auto_aim=1\n');
});

test('mobile defaults belong to display even when another INI section uses the same keys', () => {
    // Arrange
    const original =
        '[input]\nmouse_auto_aim=0\nwindow_width=320\nwindow_height=240\n[display]\nmaxfps=60\n';
    // Act
    const next = browserMobileDefaults(original, { width: 2048, height: 1152 }, true);
    // Assert
    expect(next).toContain('[input]\nmouse_auto_aim=0\nwindow_width=320\nwindow_height=240\n');
    expect(next).toContain(
        '[display]\nwindow_height=1152\nwindow_width=2048\nmouse_auto_aim=1\nmaxfps=60\n',
    );
});

test('desktop bootstrap keeps the existing resolution and leaves mouse auto aim unset', () => {
    // Arrange
    const files = new Map<string, string>();
    const game = {
        FS: {
            mkdirTree() {},
            analyzePath: (path: string) => ({ exists: files.has(path) }),
            readFile: (path: string) => new TextEncoder().encode(files.get(path)),
            writeFile: (path: string, value: string) => files.set(path, value),
        },
    } as unknown as GameModule;
    // Act
    ensureBrowserSettings(game)();
    // Assert
    expect(files.get('/user/midas.ini')).toContain('window_width=960\nwindow_height=540');
    expect(files.get('/user/midas.ini')).not.toContain('mouse_auto_aim');
});

test('mobile migration is remembered after flush and fresh settings still receive Retina defaults', () => {
    // Arrange
    const path = '/user/midas.ini';
    const legacy = '[display]\nwindow_width=960\nwindow_height=540\nmouse_auto_aim=0\n';
    const files = new Map([[path, legacy]]);
    const preferences = new Map<string, string>();
    const globals = {
        window: { visualViewport: { width: 1024, height: 768 } },
        innerWidth: 1024,
        innerHeight: 768,
        devicePixelRatio: 2,
        localStorage: {
            getItem: (key: string) => preferences.get(key),
            setItem: (key: string, value: string) => preferences.set(key, value),
        },
    };
    const descriptors = Object.keys(globals).map(
        (key) => [key, Object.getOwnPropertyDescriptor(globalThis, key)] as const,
    );
    const game = {
        FS: {
            mkdirTree() {},
            analyzePath: (name: string) => ({ exists: files.has(name) }),
            readFile: (name: string) => new TextEncoder().encode(files.get(name)),
            writeFile: (name: string, value: string) => files.set(name, value),
        },
    } as unknown as GameModule;
    for (const [key, value] of Object.entries(globals))
        Object.defineProperty(globalThis, key, { configurable: true, value });
    try {
        // Act: migration is pending until the caller's successful flush.
        const remember = ensureBrowserSettings(game, true);
        // Assert
        expect(files.get(path)).toContain('window_width=2048\nwindow_height=1152');
        expect(files.get(path)).toContain('mouse_auto_aim=0');
        expect(preferences.size).toBe(0);
        remember();
        expect(preferences.get('midas-ads-mobile-resolution-v1')).toBe('done');

        files.set(path, legacy); // A later deliberate 960x540 selection is retained.
        ensureBrowserSettings(game, true)();
        expect(files.get(path)).toContain('window_width=960\nwindow_height=540');
        files.delete(path); // Reset settings with the browser marker still present.
        ensureBrowserSettings(game, true)();
        expect(files.get(path)).toContain('window_width=2048\nwindow_height=1152');
        expect(files.get(path)).toContain('mouse_auto_aim=1');
    } finally {
        for (const [key, maybeDescriptor] of descriptors) {
            if (maybeDescriptor) Object.defineProperty(globalThis, key, maybeDescriptor);
            else Reflect.deleteProperty(globalThis, key);
        }
    }
});
test('explicit Reverse, Upright and unrelated settings are preserved', () => {
    for (const value of [0, 1]) {
        const settings = `[display]\nlook_updown=${value}\n[controls]\nstart=Space\n`;
        expect(browserLookDefault(settings)).toBe(settings);
    }
});
test('default insertion is idempotent and handles CRLF or missing display sections', () => {
    for (const settings of [
        '[display]\r\nmaxfps=60\r\n',
        '[controls]\nstart=Return\n',
        '[display]',
    ]) {
        const next = browserLookDefault(settings);
        expect(next).toContain('look_updown=1');
        expect(browserLookDefault(next)).toBe(next);
    }
});

test('touch devices start with Tone Mapping and Baked Lighting off, keeping explicit choices', () => {
    // Arrange / Act
    const fresh = browserTouchGraphicsDefaults('[display]\nmaxfps=60\n');
    const chosen = '[display]\ntone_mapping=1\nbaked_lighting=1\n';
    // Assert
    expect(fresh).toContain('tone_mapping=0\n');
    expect(fresh).toContain('baked_lighting=0\n');
    expect(browserTouchGraphicsDefaults(fresh)).toBe(fresh);
    expect(browserTouchGraphicsDefaults(chosen)).toBe(chosen);
});

test('the first build saved Graphics off; the migration drops only that pair in display', () => {
    // Arrange
    const saved =
        '[display]\r\nmaxfps=60\r\ntone_mapping=0\r\ngraphics_look=2\r\nbaked_lighting=0\r\n[input]\r\ntone_mapping=0\r\n';
    // Act
    const next = browserGraphicsDefaultMigration(saved);
    // Assert
    expect(next).toBe('[display]\r\nmaxfps=60\r\ngraphics_look=2\r\n[input]\r\ntone_mapping=0\r\n');
    for (const kept of [
        '[display]\ntone_mapping=1\nbaked_lighting=0\n',
        '[display]\ntone_mapping=0\nbaked_lighting=1\n',
        '[display]\nmaxfps=60\n',
    ])
        expect(browserGraphicsDefaultMigration(kept)).toBe(kept);
});

test('desktop browsers run the Graphics migration once, after the caller flushes', () => {
    // Arrange
    const path = '/user/midas.ini';
    const files = new Map([[path, '[display]\ntone_mapping=0\nbaked_lighting=0\n']]);
    const preferences = new Map<string, string>();
    const previous = Object.getOwnPropertyDescriptor(globalThis, 'localStorage');
    Object.defineProperty(globalThis, 'localStorage', {
        configurable: true,
        value: {
            getItem: (key: string) => preferences.get(key),
            setItem: (key: string, value: string) => preferences.set(key, value),
        },
    });
    const game = {
        FS: {
            mkdirTree() {},
            analyzePath: (name: string) => ({ exists: files.has(name) }),
            readFile: (name: string) => new TextEncoder().encode(files.get(name)),
            writeFile: (name: string, value: string) => files.set(name, value),
        },
    } as unknown as GameModule;
    try {
        // Act
        const remember = ensureBrowserSettings(game);
        // Assert
        expect(files.get(path)).not.toContain('tone_mapping');
        expect(files.get(path)).not.toContain('baked_lighting');
        remember();
        expect(preferences.get('midas-graphics-default-v1')).toBe('done');
        files.set(path, '[display]\ntone_mapping=0\nbaked_lighting=0\n'); // a later choice
        ensureBrowserSettings(game)();
        expect(files.get(path)).toContain('tone_mapping=0\nbaked_lighting=0\n');
    } finally {
        if (previous) Object.defineProperty(globalThis, 'localStorage', previous);
        else Reflect.deleteProperty(globalThis, 'localStorage');
    }
});
