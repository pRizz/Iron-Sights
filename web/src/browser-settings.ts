import type { GameModule } from './runtime-types';
import { containSize } from './presentation';

const MOBILE_RESOLUTION_KEY = 'midas-ads-mobile-resolution-v1';

const DEFAULT_SETTINGS =
    '[display]\nfullscreen=windowed\nwindow_width=960\nwindow_height=540\n' +
    'render_thread=0\nvsync=0\nmaxfps=60\ntickhz=60\nmsaa=2\nmusic_volume=70\nsfx_volume=70\nlook_updown=1\n' +
    '[input]\nmouse=1\n[controls]\nstart=Return,P\n';

function displayDefault(settings: string, key: string, value: number): string {
    // Explicit preferences (including Reverse) belong to the player.
    const newline = settings.includes('\r\n') ? '\r\n' : '\n';
    const display = /(^[ \t]*\[display\][ \t]*)(\r?\n|$)([\s\S]*?)(?=^[ \t]*\[|(?![\s\S]))/im;
    if (display.test(settings))
        return settings.replace(display, (original, header: string, end: string, body: string) =>
            new RegExp(`^[ \\t]*${key}[ \\t]*=`, 'im').test(body)
                ? original
                : `${header}${end || newline}${key}=${value}${end || newline}${body}`,
        );
    return `${settings}${settings.endsWith('\n') ? '' : newline}[display]${newline}${key}=${value}${newline}`;
}

export function browserLookDefault(settings: string): string {
    return displayDefault(settings, 'look_updown', 1);
}

export function mobileResolution(width: number, height: number, dpr: number) {
    const ratio = Number.isFinite(dpr) && dpr > 0 ? dpr : 1;
    const size = containSize(width, height, 16 / 9);
    return {
        width: Math.max(1, Math.round(size.width * ratio)),
        height: Math.max(1, Math.round(size.height * ratio)),
    };
}

export function browserMobileDefaults(
    settings: string,
    size: { width: number; height: number },
    migrateResolution: boolean,
): string {
    let next = displayDefault(settings, 'mouse_auto_aim', 1);
    next = displayDefault(next, 'window_width', size.width);
    next = displayDefault(next, 'window_height', size.height);
    // Restrict resolution migration to [display], preserving all other settings.
    return next.replace(
        /(^[ \t]*\[display\][ \t]*\r?$)([\s\S]*?)(?=^[ \t]*\[|(?![\s\S]))/im,
        (_, header: string, body: string) => {
            const maybeWidth = /^[ \t]*window_width[ \t]*=[ \t]*(\d+)[ \t]*\r?$/im.exec(body);
            const maybeHeight = /^[ \t]*window_height[ \t]*=[ \t]*(\d+)[ \t]*\r?$/im.exec(body);
            const legacy = Number(maybeWidth?.[1]) === 960 && Number(maybeHeight?.[1]) === 540;
            if (migrateResolution && legacy) {
                body = body.replace(
                    /(^[ \t]*window_width[ \t]*=)[^\r\n]*/im,
                    (_, prefix) => prefix + size.width,
                );
                body = body.replace(
                    /(^[ \t]*window_height[ \t]*=)[^\r\n]*/im,
                    (_, prefix) => prefix + size.height,
                );
            }
            return header + body;
        },
    );
}

// Commit the browser migration marker only after the caller flushes user files.
export function ensureBrowserSettings(game: GameModule, touch = false): () => void {
    game.FS.mkdirTree('/user/Data/Saves');
    const path = '/user/midas.ini';
    const exists = game.FS.analyzePath(path).exists;
    const original = exists ? new TextDecoder().decode(game.FS.readFile(path)) : DEFAULT_SETTINGS;
    let next = browserLookDefault(original);
    if (touch) {
        let migrated = false;
        try {
            migrated = localStorage.getItem(MOBILE_RESOLUTION_KEY) === 'done';
        } catch {
            /* Settings still work without browser preferences. */
        }
        const viewport = window.visualViewport;
        const size = mobileResolution(
            Math.min(innerWidth, viewport?.width || innerWidth),
            Math.min(innerHeight, viewport?.height || innerHeight),
            devicePixelRatio,
        );
        next = browserMobileDefaults(next, size, !exists || !migrated);
    }
    if (!exists || next !== original) game.FS.writeFile(path, next);
    return () => {
        if (!touch) return;
        try {
            localStorage.setItem(MOBILE_RESOLUTION_KEY, 'done');
        } catch {
            /* Non-default saved dimensions also make migration idempotent. */
        }
    };
}
