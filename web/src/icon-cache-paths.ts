// The weapon icon cache the game prepares from the player's ROM on the first
// launch (FORK-019's in-process icon stage). It persists with the saves, so a
// reload skips the stage, but stays out of save backups: it is derived from the
// ROM and the game prepares it again when it is missing.
const ICON_CACHE = /^Data\/Cache\/weapon-icons-v\d+-[0-9a-f]{8}\.cache$/;

export function iconCachePath(path: string): boolean {
  return ICON_CACHE.test(path);
}
