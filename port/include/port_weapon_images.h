/* A fresh checkout can build the exporter before any ROM-derived icons exist.
 * The generated images and their metadata remain local, like other ROM assets. */
#ifndef PORT_WEAPON_IMAGES_BRIDGE_H
#define PORT_WEAPON_IMAGES_BRIDGE_H
#if defined(GE_EMBED_WEAPON_ICONS) && __has_include("assets/port/weapon_icons/weapon_icons.h")
#define PORT_WEAPON_IMAGES_AVAILABLE 1
#include "assets/port/weapon_icons/weapon_icons.h"
#else
#define PORT_WEAPON_IMAGES_AVAILABLE 0
#define PORT_WEAPON_IMAGE_WIDTH 512
#define PORT_WEAPON_IMAGE_HEIGHT 160
#define PORT_WEAPON_IMAGE_BYTES 0
typedef struct { unsigned int offset, size; } PortWeaponImage;
static const PortWeaponImage portWeaponImages[32] = {{0,0}};
#endif
/* Existing local packs can bootstrap the new exporter. */
#ifndef PORT_WEAPON_IMAGE_COUNT
#define PORT_WEAPON_IMAGE_COUNT 32
#endif
#endif
