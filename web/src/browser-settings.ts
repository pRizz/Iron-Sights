import type { GameModule } from './runtime-types';

const DEFAULT_SETTINGS =
    '[display]\nfullscreen=windowed\nwindow_width=960\nwindow_height=540\n' +
    'render_thread=0\nvsync=0\nmaxfps=60\ntickhz=60\nmsaa=1\nmusic_volume=70\nsfx_volume=70\nlook_updown=1\n' +
    '[input]\nmouse=1\n[controls]\nstart=Return,P\n';

export function browserLookDefault(settings: string): string {
    // Explicit preferences (including Reverse) belong to the player.
    if (/^[ \t]*look_updown[ \t]*=/im.test(settings)) return settings;
    const newline = settings.includes('\r\n') ? '\r\n' : '\n';
    const display = /(^[ \t]*\[display\][ \t]*)(\r?\n|$)/im;
    if (display.test(settings))
        return settings.replace(
            display,
            (_, header: string, end: string) =>
                `${header}${end || newline}look_updown=1${end || newline}`,
        );
    return `${settings}${settings.endsWith('\n') ? '' : newline}[display]${newline}look_updown=1${newline}`;
}

export function ensureBrowserSettings(game: GameModule): void {
    game.FS.mkdirTree('/user/Data/Saves');
    const path = '/user/midas.ini';
    if (!game.FS.analyzePath(path).exists) {
        game.FS.writeFile(path, DEFAULT_SETTINGS);
        return;
    }
    const original = new TextDecoder().decode(game.FS.readFile(path));
    const next = browserLookDefault(original);
    if (next !== original) game.FS.writeFile(path, next);
}
