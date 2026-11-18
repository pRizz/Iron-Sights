/* Layout checks behind the `stored` sites' verdicts (LP64-005 Task 3, batch 5;
 * the sites are in scripts/port/lp64_overlays_reviewed.txt, each `same` line
 * naming its assertion here by its message). Included by probe.c after
 * probe_overlays.h, whose IS_TYPE and POSITION_FIRST it uses, so it is built
 * at both pointer widths by scripts/port/test_abi_probe.sh. Each assertion
 * holds at both widths and fails when a member is inserted before the one it
 * pins. */

/* objecthandler.c sub_GAME_7F06B29C reads an Op16 node's data as three floats. */
POSITION_FIRST(ModelNode_Op16Record, pos, coord3d);

/* gunfire.c gunUpdateAndFire reads a gun's slot 28, an interlink node, as six
 * floats: pos then pos2. */
_Static_assert(IS_TYPE(((ModelRoData_InterlinkageRecord *) 0)->pos, coord3d)
                   && IS_TYPE(((ModelRoData_InterlinkageRecord *) 0)->pos2, coord3d)
                   && offsetof(ModelRoData_InterlinkageRecord, pos2) == 3 * sizeof(f32)
                   && IS_TYPE(((ModelRoData_InterlinkageRecord *) 0)->Scale, f32)
                   && offsetof(ModelRoData_InterlinkageRecord, Scale) == 6 * sizeof(f32),
               "an interlink record is two positions and a scale, all floats");

/* proplvreset2's 64-bit branch swaps one 32-bit word through a u32 * at the
 * first member named; the members named after it must fill the rest of that
 * aligned word, in order. */
#define SWAP_WORD_MESSAGE "the setup words proplvreset2 swaps by member are 4 aligned bytes at both widths"
#define SWAP_WORD_OF2(type, a, b)                                                                        \
    _Static_assert(offsetof(type, a) % 4 == 0 && sizeof(((type *) 0)->a) == 2                           \
                       && offsetof(type, b) == offsetof(type, a) + 2 && sizeof(((type *) 0)->b) == 2,   \
                   SWAP_WORD_MESSAGE)
#define SWAP_WORD_OF3(type, a, b, c)                                                                     \
    _Static_assert(offsetof(type, a) % 4 == 0 && sizeof(((type *) 0)->a) == 2                           \
                       && offsetof(type, b) == offsetof(type, a) + 2 && sizeof(((type *) 0)->b) == 1    \
                       && offsetof(type, c) == offsetof(type, a) + 3 && sizeof(((type *) 0)->c) == 1,   \
                   SWAP_WORD_MESSAGE)
SWAP_WORD_OF2(GuardRecord, Preset, chrpreset1);
_Static_assert(offsetof(ObjectRecord, damage) % 4 == 0 && sizeof(((ObjectRecord *) 0)->damage) == 4, SWAP_WORD_MESSAGE);
SWAP_WORD_OF3(struct objective_entry, unkC, unkD, difficulty);
SWAP_WORD_OF2(struct objective_entry, reserved, text);
SWAP_WORD_OF2(struct watchMenuObjectiveText, reserved, text);
SWAP_WORD_OF3(GuardAttributeRecord, unk8, unkA, GrenadeProb);

/* bgfog.c portFogServeFromRom copies each record whole from the ROM's bytes
 * by sizeof and swaps it at fixed file offsets. */
_Static_assert(sizeof(EnvironmentRecord) == 92 && sizeof(EnvironmentFoglessRecord) == 56,
               "the fog tables' records are the ROM's 92 and 56 bytes at both widths");
