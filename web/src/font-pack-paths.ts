// The Enhanced fonts the game downloads on the player's request (FORK-052,
// font styles plan FONT-4). They persist with the saves, so a reload keeps
// them, but stay out of save backups: they are not the player's data and can
// be downloaded again.
const FONT_PACK = /^Data\/FontPacks\/gepd\/(GOLDENEYE#[0-9A-F]{8}#4#1_rgb\.png(\.part)?|SOURCE\.txt)$/;

export function fontPackPath(path: string): boolean {
  return FONT_PACK.test(path);
}
