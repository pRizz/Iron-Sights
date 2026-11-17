/**
 * The converter oracle (LP64-004; scripts/port/native_64bit_lp64_004.md,
 * decision 8): in the 32-bit headless build configured with
 * -DGE_LP64_ORACLE=ON, each converter also runs beside the game's in-place
 * conversion it replaces, and these functions compare the two and log the
 * verdict. At 32 bits a converter writes the run-time layout through the C
 * types, so its output must equal the in-place result exactly.
 *
 * At exit the oracle logs one line a class:
 * ge: lp64: oracle: CLASS: N files, M differ
 * which scripts/port/lp64_oracle.py sums over a boot and the 20 missions.
 *
 * Without GE_LP64_ORACLE (every shipped build, and every 64-bit build) these
 * are macros that generate no code, and port_lp64_oracle.c is not compiled.
 */
#ifndef PORT_LP64_ORACLE_H
#define PORT_LP64_ORACLE_H

#include <PR/ultratypes.h>
#include "port_lp64.h"

#ifdef GE_LP64_ORACLE
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Compares SIZE bytes of the in-place result with the converter's and logs
 * ge: lp64: oracle: CLASS FILE identical (N bytes)
 * or, at the first difference,
 * ge: lp64: oracle: CLASS FILE DIFFERS at +0xOFF (in-place XX, converter YY)
 * counting the file for CLASS's exit line.
 */
void portLp64OracleBytes(enum PortLp64Asset asset, const char *file, const void *inplace, const void *converted,
        u32 size);

/**
 * For a comparison that is not of bytes (two graphs of records, say): the
 * caller compared, and passes IDENTICAL and a DETAIL saying what was
 * compared or where it differs. Logs
 * ge: lp64: oracle: CLASS FILE identical (DETAIL)
 * or ge: lp64: oracle: CLASS FILE DIFFERS (DETAIL)
 * and counts the file as portLp64OracleBytes does.
 */
void portLp64OracleResult(enum PortLp64Asset asset, const char *file, int identical, const char *detail);

#ifdef __cplusplus
}
#endif
#else
#define portLp64OracleBytes(asset, file, inplace, converted, size) ((void) 0)
#define portLp64OracleResult(asset, file, identical, detail) ((void) 0)
#endif

#endif
