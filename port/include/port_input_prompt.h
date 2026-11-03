#ifndef PORT_INPUT_PROMPT_H
#define PORT_INPUT_PROMPT_H

/* Player 1's most recently used device class, for on-screen control prompts. */
#define PORT_PROMPT_INPUT_NONE -1
#define PORT_PROMPT_INPUT_KBM 0
#define PORT_PROMPT_INPUT_PAD 1

/* Hybrid play lets both the keyboard/mouse and the player-1 pad drive player 1,
 * so prompts follow the device used last. Before any input, the intro claim
 * decides; a muted keyboard always means pad prompts. */
static inline int portPromptUsesPad(int lastInput, int p1Device, int kbmMuted)
{
    return kbmMuted || lastInput == PORT_PROMPT_INPUT_PAD
        || (lastInput == PORT_PROMPT_INPUT_NONE && p1Device > 0);
}

#endif
