/**
 * The model oracle (port_romconv.h; LP64-004 Task 11): converts every model
 * file the compiled header tables name by both paths, romfile_model.c's
 * in-place conversion and portModelConvert into new memory, and compares the
 * whole images, both before the game's promotion. At 32 bits the converter
 * keeps the file's order and every item's size, so the two must be equal byte
 * for byte.
 *
 * The header tables are the game's own (gun, chr and prop
 * ModelFileHeaders.inc.c), read here through a MODELFILEHEADER that keeps each
 * model's name and its two counts; a header named NAME is the file G, C or P
 * NAME Z by its table. Each file is loaded through the game's loader into a
 * buffer of this file's own, as GE_ROMPROBE loads, with its lookup entry put
 * back afterwards.
 *
 * Its own file because port_lp64_oracle.c is linked into the ROM-free
 * converter tests, which have no file loader. Compiled with the other romconv_
 * sources, and empty outside the 32-bit oracle build (-DGE_LP64_ORACLE=ON).
 */
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include "ob.h"
#include "romdata.h"
#include "port_romconv.h"
#include "port_lp64_oracle.h"

#ifdef GE_LP64_ORACLE

/* port_console.c: the game's log (stderr plus the tilde console ring). */
extern void portConLogf(const char *fmt, ...);

/* romfile_model.c: the in-place conversion the 32-bit game runs. */
extern s32 romfileModelConvert(u8 *data, u32 size, s32 numSwitches, s32 numtextures, const char *name,
        const u8 *compiled);

extern resource_lookup_data_entry resource_lookup_data_array[];

/* The largest stock model file is 43,120 bytes; the loader puts the
 * compressed copy at the buffer's end and inflates to its start. */
#define ORACLE_MODEL_BUFFER (256u * 1024u)

struct OracleModel
{
    const char *name;
    s32 numSwitches;
    s32 numtextures;
};

#undef MODELFILEHEADER
#define MODELFILEHEADER(NAME, ROOTNODE, SKELETON, SWITCHES, NUMSWITCHES, NUMMATRICES, BOUNDINGRADIUS, NUMRECORDS, \
        NUMTEXTURES) { ORACLE_MODEL_PREFIX #NAME "Z", NUMSWITCHES, NUMTEXTURES },

static const struct OracleModel kModels[] = {
#define ORACLE_MODEL_PREFIX "G"
#include <assets/obseg/gun/gunModelFileHeaders.inc.c>
#undef ORACLE_MODEL_PREFIX
#define ORACLE_MODEL_PREFIX "C"
#include <assets/obseg/chr/chrModelFileHeaders.inc.c>
#undef ORACLE_MODEL_PREFIX
#define ORACLE_MODEL_PREFIX "P"
#include <assets/obseg/prop/propModelFileHeaders.inc.c>
#undef ORACLE_MODEL_PREFIX
};

#undef MODELFILEHEADER

#define ORACLE_MODEL_COUNT ((u32) (sizeof(kModels) / sizeof(kModels[0])))

void portModelOracle(const u8 *file, u32 size, s32 numSwitches, s32 numtextures, const char *name)
{
    u8 *inplace = (u8 *) malloc(size);
    u8 *converted = NULL;
    u32 n;
    s32 accepted;

    if (inplace == NULL)
    {
        portLp64OracleResult(PORT_LP64_MODELS, name, 0, "no memory for the in-place copy");
        return;
    }
    memcpy(inplace, file, size);
    accepted = romfileModelConvert(inplace, size, numSwitches, numtextures, name, NULL);
    n = portModelConvertedSize(file, size, numSwitches, numtextures, name);
    if (!accepted || n == 0)
    {
        portLp64OracleResult(PORT_LP64_MODELS, name, 0,
                !accepted ? "the in-place conversion refuses it" : "the converter refuses it");
    }
    else if (n != size)
    {
        portLp64OracleResult(PORT_LP64_MODELS, name, 0, "the converter's image is not the file's size");
    }
    else if ((converted = (u8 *) malloc(n)) == NULL)
    {
        portLp64OracleResult(PORT_LP64_MODELS, name, 0, "no memory for the converter's image");
    }
    else if (!portModelConvert(file, size, converted, n, numSwitches, numtextures, name))
    {
        portLp64OracleResult(PORT_LP64_MODELS, name, 0, "the converter refuses to write it");
    }
    else
    {
        portLp64OracleBytes(PORT_LP64_MODELS, name, inplace, converted, size);
    }
    free(converted);
    free(inplace);
}

void portModelOracleSweep(void)
{
    u8 *buffer = (u8 *) malloc(ORACLE_MODEL_BUFFER);
    u32 i, swept = 0;

    if (buffer == NULL)
    {
        portConLogf("ge: lp64: oracle: models sweep: no memory for its buffer\n");
        return;
    }
    for (i = 0; i < ORACLE_MODEL_COUNT; i++)
    {
        const struct OracleModel *m = &kModels[i];
        s32 index = fileGetIndex((u8 *) m->name);
        resource_lookup_data_entry saved;
        u32 size;

        if (index <= 0 || !romdataFileServed(index))
        {
            portLp64OracleResult(PORT_LP64_MODELS, m->name, 0, "not served from the ROM");
            continue;
        }
        saved = resource_lookup_data_array[index];
        fileIndexLoadToAddr(index, FILELOADMETHOD_DEFAULT, buffer, (s32) ORACLE_MODEL_BUFFER);
        size = resource_lookup_data_array[index].poolRemaining;
        resource_lookup_data_array[index] = saved;
        portModelOracle(buffer, size, m->numSwitches, m->numtextures, m->name);
        swept++;
    }
    free(buffer);
    portConLogf("ge: lp64: oracle: models sweep: the header tables name %u files, %u swept\n", ORACLE_MODEL_COUNT,
            swept);
}

#endif
