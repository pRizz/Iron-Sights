#ifndef PORT_HD_TEXTURES_H
#define PORT_HD_TEXTURES_H
/* Player-installed HD texture packs (FORK-053): native Windows and Linux only. */
#if defined(GE_VIDEO) && !defined(__EMSCRIPTEN__) && !defined(GE_XBOX)
#ifdef __cplusplus
extern "C" {
#endif
/* The folder the player puts a pack in (Data/TexturePacks under the root). */
void portTexturePackRoot(const char *directory);
/* [display] texture_pack: 0 Original, 1 HD (a player-installed pack). */
void portTexturePackSet(int mode);
int portTexturePackGet(void);
/* The Texture Pack row's HD text once a search found no pack: "HD (Get)"
 * where the HD Textures page can download one, "HD (not found)" elsewhere;
 * otherwise text. */
const char *portTexturePackValueText(const char *text);
#ifdef __cplusplus
}
#endif
#endif
#endif
