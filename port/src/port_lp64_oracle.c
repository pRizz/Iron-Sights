/**
 * The converter oracle (port_lp64_oracle.h): the verdict lines for each
 * comparison of a converter's output with the game's in-place result, and the
 * per-class totals at exit.
 *
 * Compiled only in the 32-bit headless oracle build (-DGE_LP64_ORACLE=ON;
 * CMakeLists.txt), never in a shipped or a 64-bit build. It needs no hook in
 * shared code: a constructor registers the exit report, and the converters'
 * call sites (each inside #ifdef GE_LP64_ORACLE) feed the counts.
 *
 * GE_LP64_ORACLE_SWEEP=1 asks for the sweeps that also convert files a run
 * does not load; lp64_oracle.py sets it for its boot run. The models class has
 * one (portModelOracleSweep, romconv_model_oracle.c), which romdata.c runs
 * at boot when portLp64OracleSweepRequested says so. GE_ROMPROBE=1, which the
 * same run sets, is the game's own: it loads every served text bank, briefing,
 * setup and stan file through the normal load path at boot (romdata.c).
 */
#include <stdio.h>
#include <stdlib.h>
#include "port_lp64_oracle.h"

#ifdef GE_LP64_ORACLE

/* port_console.c: the game's log (stderr plus the tilde console ring). */
extern void portConLogf(const char *fmt, ...);

static unsigned s_Files[PORT_LP64_ASSET_COUNT];
static unsigned s_Differ[PORT_LP64_ASSET_COUNT];
static int s_SweepRequested;

static void oracleCount(enum PortLp64Asset asset, int identical)
{
    if ((unsigned) asset >= (unsigned) PORT_LP64_ASSET_COUNT)
    {
        return;
    }
    s_Files[asset]++;
    if (!identical)
    {
        s_Differ[asset]++;
    }
}

void portLp64OracleBytes(enum PortLp64Asset asset, const char *file, const void *inplace, const void *converted,
        u32 size)
{
    const u8 *a = (const u8 *) inplace;
    const u8 *b = (const u8 *) converted;
    u32 i;

    for (i = 0; i < size && a[i] == b[i]; i++)
    {
    }
    if (i == size)
    {
        portConLogf("ge: lp64: oracle: %s %s identical (%u bytes)\n", portLp64AssetName(asset), file, size);
        oracleCount(asset, 1);
        return;
    }
    portConLogf("ge: lp64: oracle: %s %s DIFFERS at +0x%X (in-place %02X, converter %02X)\n",
            portLp64AssetName(asset), file, i, a[i], b[i]);
    oracleCount(asset, 0);
}

void portLp64OracleResult(enum PortLp64Asset asset, const char *file, int identical, const char *detail)
{
    portConLogf("ge: lp64: oracle: %s %s %s (%s)\n", portLp64AssetName(asset), file,
            identical ? "identical" : "DIFFERS", detail != NULL ? detail : "");
    oracleCount(asset, identical);
}

int portLp64OracleSweepRequested(void)
{
    return s_SweepRequested;
}

static void oracleReport(void)
{
    int asset;

    for (asset = 0; asset < PORT_LP64_ASSET_COUNT; asset++)
    {
        portConLogf("ge: lp64: oracle: %s: %u files, %u differ\n", portLp64AssetName((enum PortLp64Asset) asset),
                s_Files[asset], s_Differ[asset]);
    }
}

__attribute__((constructor)) static void oracleStart(void)
{
    const char *sweep = getenv("GE_LP64_ORACLE_SWEEP");

    s_SweepRequested = sweep != NULL && sweep[0] != '\0' && sweep[0] != '0';
    atexit(oracleReport);
}

#endif
