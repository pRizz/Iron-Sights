/* Isolated visual audit: no input or gameplay changes without this knob.
 * Private to port_hud.c, included after its state and helpers. */
static void phVisualAudit(s32 p)
{
    static f32 time;
    static s32 fired;
    static s32 equipped;
    static s32 switched;
    static s32 fadeShot;
    extern s32 portJoyPortInvScrollItem(s32 id);
    const char *audit = getenv("GE_HUD_AUDIT");
    if (p == 0 && audit && fired < 4) {
        time += g_GlobalTimerDelta;
        /* Let opening AI finish resetting inventory (notably Bunker 2). */
        if (!equipped && time >= 60 && ((atoi(audit) >= ITEM_FIST && atoi(audit) <= ITEM_TASER
            && atoi(audit) != ITEM_TRIGGER) || portJoyPortInvScrollItem(atoi(audit)))) {
            const char *best = getenv("GE_HUD_AUDIT_BEST");
            /* RAM-only record fixture in isolated audit processes. */
            if (best && atoi(best) > 0 && atoi(best) <= 1023) {
                s32 entry;
                extern void fileSetDifficultyStageTime(save_data *, LEVEL_SOLO_SEQUENCE, DIFFICULTY, s32);
                save_data *save = fileGetSaveForFoldernum(selected_folder_num);
                if (!save) {
                    save = &saves[SAVESLOT1];
                    memset(save, 0, sizeof(*save));
                    save->completion_bitflags = selected_folder_num;
                }
                for (entry = 0; mission_folder_setup_entries[entry].folder_text_preset; entry++) {
                    const struct mission_folder_setup *mission = &mission_folder_setup_entries[entry];
                    if (mission->stage_id == bossGetStageNum() && mission->mission_num >= 0 && mission->mission_num < 20)
                        fileSetDifficultyStageTime(save, mission->mission_num, lvlGetSelectedDifficulty(), atoi(best));
                }
            }
            if (getenv("GE_HUD_AUDIT_CHOP")) {
                /* Surface starts with a sniper even without the guns
                 * cheat; remove it so this fixture exercises real chops. */
                bondinvSetAllGunsFlag(FALSE);
                bondinvRemoveItemByID(ITEM_SNIPERRIFLE);
                g_CurrentPlayer->cur_item_weapon_getname = ITEM_FIST;
            }
            if (portJoyPortInvScrollItem(atoi(audit))) {
                /* Match direct watch selection for model-less gadgets. */
                bondinvAddInvItem(atoi(audit));
                currentPlayerUnEquipWeaponWrapper(GUNRIGHT, atoi(audit));
                currentPlayerUnEquipWeaponWrapper(GUNLEFT, ITEM_UNARMED);
            } else {
                currentPlayerEquipWeaponWrapper(GUNLEFT, ITEM_UNARMED);
                currentPlayerEquipWeaponWrapper(GUNRIGHT, atoi(audit));
            }
            equipped = 1;
        }
        if (!fired && time >= 100.0f) {
            extern void portConLogf(const char *fmt, ...);
            portHudHit(0, getenv("GE_HUD_AUDIT_BODY") == NULL);
            g_CurrentPlayer->bondhealth = 0.625f;
            g_CurrentPlayer->bondarmour = 0.5f;
            /* Known attacker on Bond's right, using the game's movement
             * basis: right = (-cos(heading), -sin(heading)). */
            const f32 heading = g_CurrentPlayer->vv_theta * HUD_PI / 180.0f;
            portHudDamage(0, cosf(heading), sinf(heading));
            if (getenv("GE_HUD_AUDIT_DIALOGUE") && bossGetStageNum() == LEVELID_STATUE) {
                /* Real localized Janus dialogue through the native queue. */
                hudmsgTopShow((char *)langGet(getStringID(LSTAT, 25)));
                portHudObjective(1, 1, OBJECTIVESTATUS_COMPLETE,
                    (const char *)langGet(getStringID(LSTAT, 5)));
            } else {
                portHudObjective(0, 0, OBJECTIVESTATUS_COMPLETE, "Neutralise all alarms");
                portHudObjective(1, 1, OBJECTIVESTATUS_FAILED, "Install covert modem");
            }
            fired = 1;
            portConLogf("ge: hud-audit: hit, direction, completed objective, weapon strip\n");
        } else if (fired == 1 && time >= 104.0f) {
            extern int ge_shot_force;
            ge_shot_force = 1;
            fired = 2;
        } else if (fired == 2 && !fadeShot && getenv("GE_HUD_AUDIT_TOAST_FADE") && time >= 361.0f) {
            /* Halfway through the first alert's fade-out. */
            extern int ge_shot_force;
            extern void portConLogf(const char *fmt, ...);
            ge_shot_force = 1;
            fadeShot = 1;
            portConLogf("ge: hud-audit: toast fade opacity=%.2f\n", phToastOpacity(phState[p].toastTime));
        } else if (fired == 2 && time >= 380.0f) {
            extern int ge_shot_force;
            extern const char *portJoyPortGadgetHudName(void);
            extern void portConLogf(const char *fmt, ...);
            const char *maybeName = portJoyPortGadgetHudName();
            portConLogf("ge: hud-audit: right=%d left=%d ammo-hide=%x action=%d gadget=%s visible=%d\n",
                getCurrentPlayerWeaponId(GUNRIGHT), getCurrentPlayerWeaponId(GUNLEFT),
                g_CurrentPlayer->gunammooff, g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state,
                maybeName ? maybeName : "(none)", portGadgetHudNameShown());
            ge_shot_force = 1;
            fired = 3;
        } else if (fired == 3 && !switched && time >= 650.0f) {
            currentPlayerEquipWeaponWrapper(GUNRIGHT, atoi(audit) == ITEM_WPPK ? ITEM_AK47 : ITEM_WPPK);
            switched = 1;
        } else if (fired == 3 && time >= 670.0f) {
            extern int ge_shot_force;
            ge_shot_force = 1;
            fired = 4;
        }
    }
}
