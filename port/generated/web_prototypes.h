/* Generated from real Clang definitions by scripts/port/web_prototypes.py. */
#ifndef MIDAS_WEB_PROTOTYPES_H
#define MIDAS_WEB_PROTOTYPES_H
#include <bondtypes.h>
#include <bondconstants.h>
typedef struct textoverride textoverride;
typedef struct save_data save_data;

struct Mtxf;
struct ObjectRecord;
struct ShotData;
struct StanPrefixRecord;
struct bondstruct_unk_animation_related;
struct sImageTableEntry;
struct texpool;

/* src/game/propobj.c */
void add_ammo_to_inventory(AMMOTYPE, int, int, int);

/* src/game/chrai.c */
void ai(PropDefHeaderRecord *, PROP_TYPE);

/* src/game/deb_loadallmodels.c */
void alloc_false_GUARDdata_to_exec_global_action(void);

/* src/game/initguards.c */
void alloc_init_GUARDdata_entries(s32);

/* src/game/initanitable.c */
void alloc_load_expand_ani_table(void);

/* src/game/bg.c */
void bgFindRoomsAlongSegment(coord3d *, coord3d *, u8 *, u8 *, s32 *, s32 *, s32);

/* src/game/bondinv.c */
void bondinvAddTextOverride(textoverride *);

/* src/game/bondinv.c */
void bondinvDetermineEquippedItem(void);

/* src/game/bondinv.c */
f32 bondinvGetDepthForIndex(int);

/* src/game/bondinv.c */
f32 bondinvGetDepthWatchForIndex(int);

/* src/game/bondinv.c */
f32 bondinvGetHoffsetForIndex(int);

/* src/game/bondinv.c */
f32 bondinvGetHposWatchForIndex(int);

/* src/game/bondinv.c */
f32 bondinvGetVoffsetForIndex(int);

/* src/game/bondinv.c */
f32 bondinvGetVposWatchForIndex(int);

/* src/game/bondinv.c */
f32 bondinvGetXrotWatchForIndex(int);

/* src/game/bondinv.c */
f32 bondinvGetYrotWatchForIndex(int);

/* src/game/bondinv.c */
void bondinvIncrementHeldTime(s32, s32);

/* src/game/bondinv.c */
void bondinvSetAllGunsFlag(s32);

/* src/game/bondinv.c */
void bondinvSetCurEquippedItem(int);

/* src/game/bondview2.c */
void bondviewKillCurrentPlayer(void);

/* src/game/bondview2.c */
void bondviewRemovePlayerBody(void);

/* src/game/bondview2.c */
void bondviewSetCameraMode(s32);

/* src/game/bondview.c */
void bondviewSetCurrentPlayerPosition(coord3d *, coord3d *, coord3d *, StandTile *, coord3d *);

/* src/game/bondview2.c */
void bondviewSetVisibleToGuardsFlag(s32);

/* src/game/bondview2.c */
void bondviewUpdateCameraMatrices(coord3d *, coord3d *, coord3d *);

/* src/game/bondview.c */
void bondviewUpdateFrustumPlanes(void);

/* src/boss.c */
void bossEntry(void);

/* src/game/gunfire.c */
void bullet_path_from_screen_center(coord3d *, coord3d *, enum GUNHAND);

/* src/game/initactorpropstuff.c */
void casingsInit(void);

/* port/src/n64_random.c */
void chrObjRandomSetSeed(u32);

/* src/game/propobj.c */
void chrSetWeaponFlag4(ChrRecord *, GUNHAND);

/* src/game/propobj.c */
void chrobjSndCreatePostEventDefault(ALSoundState *, coord3d *);

/* src/game/chrprop.c */
void chrpropDeregisterRoom(PropRecord *, s16);

/* src/game/chrprop.c */
void chrpropDetach(PropRecord *);

/* src/game/cleanup_alarms.c */
void cleanupAlarms(void);

/* src/game/cleanexplosions.c */
void cleanupExplosions(void);

/* src/game/cleanup_guard_data.c */
void cleanupGuardData(void);

/* src/game/cleanup_object_sounds.c */
void cleanupObjectSounds(void);

/* src/game/cleanup_objectives.c */
void cleanupObjectives(void);

/* src/game/cleanupSFXRelated.c */
void cleanupSFXRelated(void);

/* src/game/null_007970.c */
void cleanup_REMOVED_(void);

/* src/game/bg.c */
void cleanup_rooms(void);

/* src/game/cleanwindowpieces.c */
void cleanup_window_pieces(void);

/* src/game/cleanplayersound.c */
void cleanupplayersoundrelated(void);

/* src/game/title.c */
void clearChrGunModelInstances(void);

/* src/game/lightfixture.c */
void clear_light_fixturetable_in_room(s32);

/* src/game/ramromreplay.c */
void clear_ramrom_block_buffer_heading_ptrs(void);

/* src/game/options.c */
void cur_player_set_aim_control(u32);

/* src/game/options.c */
void cur_player_set_ammo_onscreen_setting(u32);

/* src/game/options.c */
void cur_player_set_lookahead(u32);

/* src/game/options.c */
void cur_player_set_screen_setting(u32);

/* src/game/options.c */
void cur_player_set_sight_onscreen_control(u32);

/* src/game/gun.c */
void currentPlayerCreateRocket(GUNHAND);

/* src/game/bondview.c */
void currentPlayerSetCameraMode(s32);

/* src/game/bondview.c */
void currentPlayerSetMatrix10C4(Mtx *);

/* src/game/bondview.c */
void currentPlayerSetMatrix10C8(Mtx *);

/* src/game/bondview.c */
void currentPlayerSetProjectionMatrix(Mtx *);

/* src/game/bondview.c */
void currentPlayerSetProjectionMatrixF(Mtxf *);

/* src/game/bondview.c */
void currentPlayerSetViewToWorldMtxf(Mtxf *);

/* src/game/bondview2.c */
void currentPlayerSetXAutoAimEnabled(bool);

/* src/game/bondview2.c */
void currentPlayerSetYAutoAimEnabled(s32);

/* src/game/gun.c */
void currentPlayerUnEquipWeaponWrapper(enum GUNHAND, enum ITEM_IDS);

/* src/deb.c */
void debTryAdd(void *, const char *);

/* src/debugmenu.c */
void debmenuPrintString(const unsigned char *);

/* src/debugmenu.c */
void debmenuResetBuffer(void);

/* src/debugmenu.c */
void debmenuSetFgColour(s32, s32, s32, s32);

/* src/game/debugmenu_08FE00.c */
void debmenuSetMenu(char **, s32 (*)[2], s32 *);

/* src/debugmenu.c */
void debmenuSetPos(s32, s32);

/* src/game/deb_loadallmodels.c */
void debug_object_load_all_models(void);

/* src/game/deb_loadallmodels.c */
void debug_weapon_load_table(void);

/* src/game/player.c */
void default_player_perspective_and_height(void);

/* src/game/propobj.c */
void display_text_for_weapon_in_lower_left_corner(ITEM_IDS);

/* src/game/propobj.c */
void doorDeactivatePortal(DoorRecord *);

/* src/game/dyn.c */
s32 dynGetFreeGfx(Gfx *);

/* src/game/explosion.c */
void explosionClearBulletImpactRoom(PropRecord *);

/* src/game/explosion.c */
void explosionClearBulletImpactRoomByFlag(PropRecord *, s8);

/* src/game/file2.c */
void fileClearSavefileForFolder(s32);

/* src/game/file2.c */
void fileCopyDemoSaveToRamRomSave(u32, save_data *);

/* src/game/file2.c */
void fileCopyFolderToFirstFree(s32);

/* src/game/file2.c */
void fileCopySave(s32, save_data *);

/* src/game/crc.c */
void fileGenerateCRC(u8 *, u8 *, save_data *);

/* src/game/objecthandler_2.c */
void fileLoad(ModelFileHeader *, char *);

/* src/game/file2.c */
void fileResetRamRomSave(void);

/* src/game/file2.c */
void fileUpdateSelectedBondInSave(s32);

/* src/game/bgfog.c */
void fogLoadLevelEnvironment(s32, s32);

/* src/game/gun.c */
void generate_player_thrown_grenade(s32);

/* src/game/gun.c */
void generate_player_thrown_knife(s32);

/* src/game/gun.c */
void generate_player_thrown_object(s32);

/* src/game/gunfire.c */
f32 get_depth_offset_solo_watch_menu_inventory_page_for_item(ITEM_IDS);

/* src/game/gunfire.c */
f32 get_depth_on_solo_watch_menu_page_for_item(ITEM_IDS);

/* src/game/gunfire.c */
f32 get_horizontal_offset_on_solo_watch_menu_for_item(ITEM_IDS);

/* src/game/gunfire.c */
f32 get_lateral_position_solo_watch_menu_main_page_for_item(ITEM_IDS);

/* src/game/gunfire.c */
f32 get_vertical_offset_on_solo_watch_menu_for_item(ITEM_IDS);

/* src/game/gunfire.c */
f32 get_vertical_position_solo_watch_menu_main_page_for_item(ITEM_IDS);

/* src/game/gunfire.c */
f32 get_xrotation_solo_watch_menu_for_item(ITEM_IDS);

/* src/game/gunfire.c */
f32 get_yrotation_solo_watch_menu_for_item(ITEM_IDS);

/* src/game/debugmenu_08FE00.c */
void gotoAboveDebugOption(void);

/* src/game/debugmenu_08FE00.c */
void gotoBelowDebugOption(void);

/* src/game/debugmenu_08FE00.c */
void gotoLeftDebugOption(void);

/* src/game/debugmenu_08FE00.c */
void gotoRightDebugOption(void);

/* src/game/gun.c */
void gunSpawnGLGrenade(s32);

/* src/game/gun.c */
void gunUpdateAttachedRocket(s32);

/* src/game/propobj.c */
void handle_alarm_gas_timer_calldamage(void);

/* src/game/propobj.c */
void hatAssignToChr(HatRecord *, ChrRecord *);

/* src/game/initimages.c */
void image_entries_load(void);

/* src/game/gunfire.c */
void increment_num_deaths(void);

/* src/game/gunfire.c */
void increment_num_suicides_display_MP(void);

/* src/game/gunfire.c */
void increment_num_times_killed_MwtGC(void);

/* src/game/indy_comms.c */
void indycommHostRamRomLoad(char *, u8 *, s32);

/* src/game/indy_comms.c */
void indycommHostSaveFile(char *, s32, u8 *);

/* src/game/initunk_0009E0.c */
void initAnimationsBuffer(struct bondstruct_unk_animation_related *, OSMesgQueue *, s32);

/* src/game/initgamedata.c */
void initGameData(void);

/* src/game/initactorpropstuff.c */
void initWeaponAnimGroups(void);

/* src/game/initplayergaitobject.c */
void init_player_gait_object(void);

/* src/game/initweaponanigroups.c */
void init_weapon_animation_groups_maybe(void);

/* src/game/debug_camera.c */
void initializeDebugCameraPosition(void);

/* src/game/title.c */
void initializeGunBarrelIntro(u8 *, s32);

/* src/game/bgroomtrans.c */
void initializeRoomData(void);

/* src/game/language.c */
void langClearBank(s32);

/* src/game/language.c */
void langInit(void);

/* src/game/lightfixture.c */
void lightFixtureBreak(Gfx *, u32, s32);

/* src/game/lightfixture.c */
void lightFixtureInitTables(void);

/* src/game/ramromreplay.c */
void load_ramrom_from_devtool(void);

/* src/game/chrai.c */
void loop_set_sound_effect_all_slots(void);

/* src/game/matrixmath.c */
void matrix_4x4_7F058C64(void);

/* src/game/matrixmath.c */
void matrix_4x4_7F058C88(void);

/* src/game/matrixmath.c */
void matrix_4x4_invert_affine(Mtxf *, Mtxf *);

/* src/mema.c */
void memaFree(void *, s32);

/* src/game/chrprop.c */
void modelGetXYExtents(Model *, f32 *, f32 *, f32 *, f32 *);

/* src/game/initunk_005520.c */
void modelmgrAllocateAnimModelSlots(s32);

/* src/game/initunk_005520.c */
void modelmgrAllocateModelSlots(s32);

/* src/game/mpmenu.c */
void mpwatchMenuTick(void);

/* src/game/objective_status2.c */
void mtxLoadRandomRotation(Mtxf *);

/* src/game/vtxstore.c */
void null_init_main_1(void);

/* src/game/initnull_000BC0.c */
void null_init_main_2(void);

/* src/game/initnull_0009D0.c */
void null_init_main_3(void);

/* src/game/ob.c */
void obInit(void);

/* src/game/ob.c */
void obLoadBGFileBytesAtOffset(u8 *, u8 *, s32, s32);

/* src/game/propobj.c */
void objHit(ShotData *, BulletHit *);

/* src/game/objective_status.c */
void objectivestatusCheckRoomEntered(s32);

/* src/pi.c */
void piCreateManager(void);

/* src/game/bondwalk2.c */
void portDisplayCrosshairImage(Gfx **, f32 *, f32 *, s32, s32, u32, u32);

/* src/game/lightfixture.c */
void redarken_lights_in_room(s32);

/* src/game/chrprop.c */
void removed_debug_roomblocks_feature(void);

/* src/game/ramromreplay.c */
void replay_recorded_ramrom_from_indy(void);

/* src/game/debug_camera.c */
void resetDebugCameraToPlayerPosition(void);

/* src/game/initactorpropstuff.c */
void reset_counter_rand_body_head(void);

/* src/game/rle.c */
void rle_expand_8bit(u8 *, u8 *);

/* src/game/propobj.c */
void save_img_index_to_obj_ani_slot(MonitorRecord *, void *);

/* src/game/ramromreplay.c */
void save_ramrom_to_devtool(void);

/* src/game/ramromreplay.c */
void select_ramrom_to_play(void);

/* src/game/mp_weapon.c */
void setMPWeaponSet(s32);

/* src/game/ramromreplay.c */
void setRamRomRecordSlot(s32);

/* src/game/explosion.c */
void setSixExplosionAndSmokeEntries(void);

/* src/game/textrelated.c */
void setTextOrientation(s32);

/* src/game/bondview.c */
void set_BONDdata_field_10E0(s32);

/* src/game/bondview2.c */
void set_BONDdata_outside_watch_menu_flag(s32);

/* src/game/bondview2.c */
void set_bondata_invincible_flag(u32);

/* src/game/player.c */
void set_cur_player(s32);

/* src/game/options.c */
void set_cur_player_look_vertical_inverted(u32);

/* src/game/playerstats_007770.c */
void set_favorite_weapon_for_every_player(void);

/* src/game/setguscale.c */
void set_gu_scale(void);

/* src/game/gunfire.c */
void set_max_ammo_for_cur_player(void);

/* src/game/mp_music.c */
void set_missionstate(MISSION_STATE_ID);

/* src/game/mp_music.c */
void set_missionstate_zero(void);

/* src/game/bondview2.c */
void set_obj_collision_flag(s32);

/* src/game/front.c */
void set_players_team_or_scenario_item_flag(int, s32);

/* src/game/chr.c */
void set_show_patrols_flag(s32);

/* src/game/propobj.c */
void set_sound_effect_for_weapontype_collection(ITEM_IDS);

/* src/game/title.c */
void setupRarewareLogoData(s32, s32);

/* src/game/loadobjectmodel.c */
void setupUpdateObjectRoomPosition(struct ObjectRecord *);

/* src/game/sky.c */
void skySetStageNum(s32);

/* src/game/sky.c */
void skyTick(void);

/* src/speed_graph.c */
void speedgraphInit(void);

/* src/speed_graph.c */
void speedgraphMarkerUpdate(void);

/* src/game/stan.c */
void stanDetermineEOF(struct StanPrefixRecord *, s32, u8 *);

/* src/game/stan.c */
void stanLoadFile(struct StanPrefixRecord *);

/* src/game/ramromreplay.c */
void stop_recording_ramrom(void);

/* src/game/file.c */
void store_favorite_weapon_current_player(u32, u32);

/* src/game/frametiming.c */
void store_osgetcount(void);

/* src/game/title.c */
void sub_GAME_7F008DE4(u8 **, s32 *);

/* src/game/propobj.c */
void sub_GAME_7F04E9BC(PropRecord *, struct ShotData *);

/* src/game/objective_status2.c */
void sub_GAME_7F057DF8(struct Mtxf *, struct Mtxf *, s32);

/* src/game/matrixmath.c */
void sub_GAME_7F059334(Mtx *, Mtx *);

/* src/game/gun.c */
void sub_GAME_7F05C614(void);

/* src/game/gun.c */
void sub_GAME_7F05D690(void);

/* src/game/gun.c */
void sub_GAME_7F05E6B4(enum GUNHAND, s32);

/* src/game/gun.c */
void sub_GAME_7F05E83C(GUNHAND);

/* src/game/gun.c */
void sub_GAME_7F05E978(Model *, s32);

/* src/game/bondview.c */
void sub_GAME_7F078464(s32);

/* src/game/options.c */
void sub_GAME_7F0A6A80(void);

/* src/game/bg.c */
f32 sub_GAME_7F0B4F9C(s32);

/* src/game/lv.c */
void sub_GAME_7F0BD8FC(s32);

/* src/game/mp_music.c */
void sub_GAME_7F0C1310(void);

/* src/game/tex.c */
void texCopyGdls(Gfx *, Gfx *, s32);

/* src/game/image.c */
void texLoadFromDisplayList(Gfx *, struct texpool *);

/* src/game/othermodemicrocode.c */
void texSelect(Gfx **, struct sImageTableEntry *, u32, s32, u32);

/* src/game/bondview.c */
void transform3Dto2DWithZScaling(coord3d *, coord3d *);

/* src/game/debug_camera.c */
void updateDebugCameraWorldPosition(void);

/* src/game/frametiming.c */
void updateFrameCounters(s32);

/* src/game/gun.c */
void used_to_load_1st_person_model_on_demand(GUNHAND);

/* src/vi.c */
void viDebugRemoved(void);

/* src/fr.c */
void viSetColorMode16Bit(void);

/* src/fr.c */
void viSetColorMode32Bit(void);

/* src/game/viewport.c */
void zbufSetBuffer(s32, s32, s32);

#endif
