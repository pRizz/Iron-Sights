/* Synthetic campaign writes prove the real C stdio -> FS close notification. */
#include <stdio.h>
#include <emscripten.h>

EMSCRIPTEN_KEEPALIVE int testWriteCampaign(int seed)
{
    FILE *file = fopen("/user/Data/Saves/eeprom.bin", "wb");
    int index;
    if (!file) return -1;
    for (index = 0; index < 512; index++) {
        if (fputc((seed + index) & 255, file) == EOF) { fclose(file); return -1; }
    }
    return fclose(file);
}

int main(void) { return 0; }
