/* ROM-free native Start binding and legacy Return-only suppression regression. */
#include <assert.h>
#include <stdarg.h>
#include "../../port/src/video.c"

void portConLogf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    abort(); /* A valid test binding must not log a parser error. */
}

void osSyncPrintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    abort();
}

static void returnStartsWatch(void)
{
    /* Arrange: the actual compiled desktop Start default, held across a poll. */
    Uint8 keys[SDL_NUM_SCANCODES] = {0};
    char binding[64];
    SDL_strlcpy(binding, sBindInfo[BIND_START].defkeys, sizeof(binding));
    iniParseKeys(BIND_START, binding);
    keys[SDL_SCANCODE_RETURN] = 1;

    /* Act: use the same key lookup and tick marker as videoGetPad/joy.c. */
    sReturnStartPolled = videoCodeDown(sBindings[BIND_START][0], keys, 0);
    portReturnStartTickConsume();

    /* Assert: Return produces Start and cannot be suppressed at either watch hook. */
    assert(sReturnStartTick == 1);
    assert(portReturnStartThisTick() == 0);
    portReturnStartTickConsume();
    assert(sReturnStartTick == 0 && portReturnStartThisTick() == 0);
}

static void explicitBindingWins(void)
{
    /* Arrange: the player deliberately replaced Return with Space. */
    Uint8 keys[SDL_NUM_SCANCODES] = {0};
    char binding[] = "Space";
    iniParseKeys(BIND_START, binding);
    keys[SDL_SCANCODE_RETURN] = 1;

    /* Act/Assert: respecting that binding must not create a fixed Return shortcut. */
    assert(!videoCodeDown(sBindings[BIND_START][0], keys, 0));
    keys[SDL_SCANCODE_SPACE] = 1;
    assert(videoCodeDown(sBindings[BIND_START][0], keys, 0));
    SDL_memset(keys, 0, sizeof(keys));
    assert(!videoCodeDown(sBindings[BIND_START][0], keys, 0));
}

int main(void)
{
    returnStartsWatch();
    explicitBindingWins();
    puts("Native Start checks passed: Return watch eligibility, tick reset, explicit rebind and blanked input.");
    return 0;
}
