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

#define APOB_ENTRY_HDR   48u    /* u32 group, type, instance, size + 32 B hash */
#define APOB_ENTRY_OFF   12u    /* the size field, and the C# bounds test       */
#define APOB_CFG_LIST    0x30u  /* first slot of the header's offset list       */
#define APOB_HDR_HASH    32u    /* trailing hash, so the list ends at −32       */
#define APOB_MAX_ENTRIES 4096u

/* Group IDs (Apob.cs:49-57) and the types this reader consumes. */
#define APOB_GROUP_MEM         1u
#define APOB_GROUP_GEN         7u
#define APOB_GROUP_SMBIOS      8u
#define APOB_TYPE_MEM_SYS_CFG 25u   /* MEM/25: the trained per-channel records */
#define APOB_TYPE_MEM_RMP     28u   /* MEM/28: the active memory profile       */
#define APOB_TYPE_GEN_CFG      3u   /* GEN/3: timing blocks + extended record  */
#define APOB_TYPE_SMBIOS_MEM   8u   /* SMBIOS/8: per-DIMM speed and voltage    */

/* Zen 5 desktop: the extended ODT record hangs 0x41F past the channel's first
 * timing block; the block spans Tecsc 0x9C + 2, and record 0 is the base the
 * extended record is measured from. */
#define APOB_ZS5_EXT_OFF     0x41Fu
#define APOB_ZS5_TIMING_SPAN 0x9Eu
#define APOB_ZS5_BLK0          0x0u

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

/* One entry of the table: a 48-byte header, data at off+APOB_ENTRY_HDR.
 * group/type are the low 16 bits of the stored words; encrypted marks the
 * entries whose upper half is 0xFFFF (data unreadable). */
typedef struct {
    size_t   off;            /* table-relative offset of the entry header  */
    uint32_t raw_group, raw_type;
    uint32_t group, type;    /* low 16 bits                                */
    uint32_t instance;       /* socket                                     */
    uint32_t size;           /* entry size, header included                */
    bool     encrypted;
} apob_entry_t;

typedef struct {
    apob_entry_t v[APOB_MAX_ENTRIES];
    size_t       count;
} apob_entry_list_t;

/* Walk the entry chains: the one at header_size, then one per non-zero slot of
 * the config list, deduplicated and sorted by offset. Returns the count. */
size_t apob_walk_entries(const apob_block_t *b, apob_entry_list_t *el);

/* The first entry of a group/type in table order, skipping encrypted ones. */
const apob_entry_t *apob_find_entry(const apob_entry_list_t *el,
                                    uint32_t group, uint32_t type);

/* Offset of the first channel's timing block in a GEN/3 entry, table-relative,
 * and of the extended ODT record inside it. False when the entry has none. */
bool apob_timing_block(const apob_block_t *b, size_t entry_off, size_t entry_end,
                       size_t *rel_out);

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
    char source[96];            /* as wide as apob_block_t.note, no truncation */
    /* Provenance, for the dump: where each record was found, and how many. */
    int  channels;              /* records in MEM/25 (one per channel)  */
    int  entry_count;           /* entries the walk produced            */
    int  rmp_memclk, rmp_cas;   /* MEM/28 profile MEMCLK MHz and CAS    */
    size_t main_at, ext_at;     /* MEM/25 ch0 and GEN/3+0x41F records   */
} apob_mem_config_t;

/* Code -> ZenTimings' vocabulary (GroupOdtImpedanceDict / MR10 VrefDQ). */
const char *apob_group_odt_name(int code);
const char *apob_vref_dq_name(int code);    /* MR10/11/12: 97.5 − 0.5·code % VDDQ */
const char *apob_phy_vref_name(int code);   /* PHY receiver: code · 100/128 % VDDQ */

bool apob_read_mem_config(apob_mem_config_t *out);

#ifdef __cplusplus
}
#endif

#endif /* APOB_H */
