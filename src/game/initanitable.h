#ifndef _INITANITABLE_H_
#define _INITANITABLE_H_
#include <ultra64.h>
#include <assets/animationtable_data.h>



/**
 * Struct to hold animation data. This is never instantiated.
 * Instead, only a pointer to this will exist.
 */
struct animation_table_data {
    /**
     * Array length is arbitrary and shouldn't matter. The largest offset
     * into this is for the last animation pointer 0xE7C0, so just choosing
     * a value bigger than that, like u16_max_value.
    */
    u8 data[0xffff];
};

/**
 * Data holder for animations.
 */
extern struct animation_table_data* ptr_animation_table;

/**
 * LP64-002: the animation at a byte offset (a PTR_ANIM_* value) in the loaded
 * animation table, replacing `(ModelAnimation *)(off + (s32)&ptr_animation_table->data)`.
 */
#define ANIM_FROM_TABLE(offset) ((struct ModelAnimation *) (ptr_animation_table->data + (offset)))

/**
 * LP64-002: a PTR_ANIM_* offset held in a ModelAnimation pointer slot until
 * expand_ani_table_entries adds the table's address in place (LP64-004
 * replaces that relocation).
 */
#define ANIM_TABLE_OFFSET(offset) ((struct ModelAnimation *) (uintptr_t) (offset))

/**
 * Contains offsets into ptr_animation_table for player and guard animations.
 * The index of each value corresponds to `enum ANIMATION`.
 * The value corresponds to (e.g. index=0) PTR_ANIM_idle (same as ANIM_DATA_idle)
 * expand_ani_table_entries rewrites each offset to the animation's address in
 * place, so the elements are pointer-sized (LP64-002).
*/
extern intptr_t animation_table_ptrs1[];

/**
 * Contains offsets into ptr_animation_table for object/vehicle animations.
 * The index of each value corresponds to `enum AIRCRAFT_ANIMATION`.
 * The value corresponds to (e.g. index=0) PTR_ANIM_helicopter_cradle (same as ANIM_DATA_helicopter_cradle)
*/
extern struct ModelAnimation * animation_table_ptrs2[];

#endif
