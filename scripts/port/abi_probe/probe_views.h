/* Layout checks for the types a 64-bit build reaches only through a pointer to
 * another record (LP64-005 Task 3, R11; `lp64_pointer_types.py scan` lists
 * them and FORK-089 has the table). Each view is judged over every member read
 * through it anywhere in the build; these pin the ones whose record is not
 * already pinned elsewhere. Included by probe.c after probe_stored.h, so it is
 * built at both pointer widths by scripts/port/test_abi_probe.sh; the .c-local
 * types come from the header it extracts. Each holds at both widths and fails
 * when a member is inserted before the one it pins. */

/* objective_status.c get_status_of_objective walks every objective command as
 * a MissionObjectiveRecord and reads ObjRefID, TextID and MinDificulty: a
 * command's words 1, 2 and 3, before any pointer. */
_Static_assert(offsetof(MissionObjectiveRecord, ObjRefID) == 4 && offsetof(MissionObjectiveRecord, TextID) == 8
                   && offsetof(MissionObjectiveRecord, MinDificulty) == 12
                   && sizeof(((MissionObjectiveRecord *) 0)->MinDificulty) == 4
                   && offsetof(struct criteria_picture, tag_id) == 4 && offsetof(struct criteria_picture, flag) == 8
                   && offsetof(struct criteria_roomentered, status) == 8
                   && offsetof(struct criteria_deposit, flag) == 12,
               "an objective command's words 1 to 3 are MissionObjectiveRecord's ObjRefID, TextID and MinDificulty");

/* chraction.c walks a PathRecord (pathFindById) as a struct patrol_path:
 * data, flags and len. */
_Static_assert(sizeof(struct patrol_path) == sizeof(PathRecord)
                   && offsetof(struct patrol_path, data) == offsetof(PathRecord, waypoints)
                   && offsetof(struct patrol_path, flags) == offsetof(PathRecord, isLoop)
                   && offsetof(struct patrol_path, len) == offsetof(PathRecord, len),
               "a patrol path is a PathRecord: data, flags and len over waypoints, isLoop and len");

/* bg.c sub_GAME_7F0B5864 reads table_for_portals[] as a PortalCache: count and
 * the bbox's four floats over unk0 to unk10. */
_Static_assert(sizeof(PortalCache) == sizeof(unk_portalstruct)
                   && offsetof(PortalCache, bbox) == offsetof(unk_portalstruct, unk4)
                   && sizeof(bbox2d) == 4 * sizeof(f32),
               "a PortalCache is an unk_portalstruct's five words");

/* model.c modelBuildGroupMatrices takes a group node as a ModelGroupMtxBuildArg
 * (flags, group, parentnode), and model_audit.c walks nodes as auditnode
 * (data, next, child). */
_Static_assert(offsetof(ModelGroupMtxBuildArg, flags) == offsetof(ModelNode, Opcode)
                   && sizeof(((ModelGroupMtxBuildArg *) 0)->flags) == sizeof(((ModelNode *) 0)->Opcode)
                   && offsetof(ModelGroupMtxBuildArg, group) == offsetof(ModelNode, Data)
                   && offsetof(ModelGroupMtxBuildArg, parentnode) == offsetof(ModelNode, Parent),
               "a ModelGroupMtxBuildArg is a ModelNode's Opcode, Data and Parent");
_Static_assert(offsetof(struct auditnode, data) == offsetof(ModelNode, Data)
                   && offsetof(struct auditnode, parent) == offsetof(ModelNode, Parent)
                   && offsetof(struct auditnode, next) == offsetof(ModelNode, Next)
                   && offsetof(struct auditnode, child) == offsetof(ModelNode, Child),
               "model_audit.c's auditnode is a ModelNode's links");

/* bg.c reads the bg file's global visibility and environment command lists,
 * left in place, as 8-byte records: a type byte, then a word at 4. */
_Static_assert(sizeof(GlobalVisCommand) == 8 && offsetof(GlobalVisCommand, arg) == 4
                   && sizeof(bg_envdata_entry_local) == 8 && offsetof(bg_envdata_entry_local, data) == 4,
               "the bg file's command records are 8 bytes, a word at 4");

/* front.c reads the briefing file as a BriefingData: u16 words only. */
_Static_assert(sizeof(struct BriefingDataSomething) == 2 * sizeof(u16)
                   && offsetof(struct BriefingData, objective) == 4 * sizeof(u16)
                   && sizeof(struct BriefingData) == (4 + 2 * OBJECTIVES_MAX) * sizeof(u16),
               "the briefing file is a BriefingData of u16 words");

/* model.c reads the staged animation data's bit descriptors in place. */
_Static_assert(sizeof(ModelAnimBitField) == 6 && offsetof(ModelAnimBitField, valueOffset) == 4,
               "an animation bit descriptor is the file's 6 bytes");

/* gun.c sub_GAME_7F05E978 writes ->DisplayList.unk00 into the read-write record of gun slots 8 to 13
 * and 35, and sub_GAME_7F05EA94 into slots 14 and 15: a switch's record (visible) in the stock guns; in the joypad, whose slots 8 to 12 hold group
 * nodes, modelGetNodeRwData returns record 0, the joypad's first switch (lp64_model_scan.py rwfirst).
 * A header's record there would take the s32 over unk00 to unk02 and the padding before ground. */
_Static_assert(offsetof(ModelRwData_DisplayListRecord, unk00) == offsetof(ModelRwData_SwitchRecord, visible)
                   && sizeof(((ModelRwData_DisplayListRecord *) 0)->unk00) == sizeof(((ModelRwData_SwitchRecord *) 0)->visible)
                   && sizeof(((ModelRwData_SwitchRecord *) 0)->visible) == 4
                   && offsetof(struct ModelRwData_HeaderRecord, ground) == 4,
               "a display-list read-write record's unk00 is a switch's visible, and the 4 bytes before a header's ground");
