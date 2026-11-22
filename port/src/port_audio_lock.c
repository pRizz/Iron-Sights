/* The sound lock (LP64-006 Task 9): see port_audio_lock.h. Compiled only in
 * a comparison build (-DGE_COMPARE=ON). */
#include <stdlib.h>
#include "port_audio_lock.h"

s32 portAudioLockFrames(s32 field_now)
{
    static s32 s_Read;
    static s32 s_On;
    static s32 s_Primed;
    static s32 s_LastField;
    s32 fields;

    if (!s_Read)
    {
        s_Read = 1;
        s_On = getenv("GE_AUDIO_LOCK") != NULL;
    }
    if (!s_On)
    {
        return -1;
    }
    if (!s_Primed)
    {
        s_Primed = 1;
        s_LastField = field_now;
        return 0;
    }
    fields = field_now - s_LastField;
    s_LastField = field_now;
    return fields > 0 ? fields : 0;
}
