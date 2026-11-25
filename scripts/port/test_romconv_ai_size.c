/* The setup converter tests' stand-in for chrai.c's chraiitemsize, which the
 * converter calls to walk an AI list command by command (LP64-007 Task 7).
 * The tests cannot link chrai.c, which needs the whole game. Each size here is
 * the one chraiitemsize gives, from the same record types; an opcode the
 * fixtures do not use is 1 byte, as chraiitemsize's default. The converter
 * sizes AI_PRINT, a NUL-terminated string, itself, so that its scan stays in
 * the file; a call for it stops the test. */
#include <stdio.h>
#include <stdlib.h>
#include <ultra64.h>
#include <bondaicommands.h>
#include <bondtypes.h>
#include "chrai.h"

s32 chraiitemsize(u8 *AIList, s32 offset)
{
    switch (AIList[offset])
    {
    case AI_GotoNext:
        return sizeof(AiGotoNextRecord);
    case AI_Label:
        return sizeof(AiLabelRecord);
    case AI_Yield:
        return sizeof(AiYieldRecord);
    case AI_EndList:
        return sizeof(AiEndListRecord);
    case AI_PRINT:
        printf("test_romconv_ai_size: chraiitemsize called for AI_PRINT, which the converter must size\n");
        abort();
    default:
        return 1;
    }
}
