#ifndef APOB_H
#define APOB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * apob.h — locate and read the AMD APOB (AGESA PSP Output Block).
 *
 * APOB is a POST-time structure the PSP writes into low physical memory; it
 * carries the trained per-channel memory configuration, including the CA/CK/CS
 * ODT groups, the PHY/DRAM Vref set and the programmed rails. ZenTimings reads
 * it on Windows through the same field map this module mirrors.
 *
 * Locating it: the driver's RSMU message that returns the base blocks on some
 * kernels, and strict-devmem caps /dev/mem mmap at 16 MiB, so we scan with
 * pread. The block sits just under the top of the highest System RAM region
 * below 4 GiB, so /proc/iomem bounds the search to a few megabytes instead of
 * the whole 4 GiB.
 */

#define APOB_MAX_SIZE (8u * 1024u * 1024u)

typedef struct {
    bool     found;
    uint64_t address;          /* physical address of the header           */
    uint32_t version;
    uint32_t table_size;
    uint32_t header_size;
    uint8_t *data;             /* the whole block, malloc'd once           */
    size_t   scanned_from;     /* top of low RAM the scan started at       */
    char     note[96];         /* how it was found / why it failed         */
} apob_block_t;

/* Locate the block and load it. Idempotent: the address is cached for the
 * process, and revalidated against the signature/size on every call. */
bool apob_load(apob_block_t *out);

/* Little-endian reads out of the loaded block; safe, return 0 when out of range. */
uint32_t apob_u32(const apob_block_t *b, size_t off);
uint16_t apob_u16(const apob_block_t *b, size_t off);
uint8_t  apob_u8(const apob_block_t *b, size_t off);

/* Top of the highest System RAM range below 4 GiB, from /proc/iomem.
 * Returns 0 if unavailable (caller falls back to scanning the whole 4 GiB). */
uint64_t apob_low_ram_top(void);

/*
 * The trained memory-configuration fields a tuner cares about. Values are raw
 * APOB codes; the renderer turns them into ohms / percent / millivolts, the
 * same split ZenStates-Core uses between ApobData and EncodedValues. -1 means
 * "not present in this APOB version".
 */
typedef struct {
    bool valid;
    int ca_odt_a, ca_odt_b;     /* Group ODT, MR32/MR33 Group A / B  */
    int ck_odt_a, ck_odt_b;
    int cs_odt_a, cs_odt_b;
    int phy_vref;               /* PHY read reference, % of VDDQ      */
    int dram_vref_dq;           /* MR10 code, % of VDDQ               */
    int dram_vref_ca;           /* MR11 code, % of VCC              */
    int dram_vref_cs;           /* MR12 code                        */
    int vddio_mv;               /* programmed VDDIO_MEM, millivolts   */
    int vddq_mv, vdd_mv, vpp_mv;
    size_t vddio_at;            /* offset the VDDIO value was read at */
    char source[64];
} apob_mem_config_t;

/* Code -> ZenTimings' vocabulary (GroupOdtImpedanceDict / MR10 VrefDQ). */
const char *apob_group_odt_name(int code);
const char *apob_vref_dq_name(int code);    /* "% of VDDQ", MR10 0.5 % steps */

bool apob_read_mem_config(apob_mem_config_t *out);

#ifdef __cplusplus
}
#endif

#endif /* APOB_H */
