#ifndef AOD_H
#define AOD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*
 * aod.h — read the AMD AOD (Overclocking Data) table and decode the
 * signal-integrity fields the BIOS trained: the drive strengths and the
 * on-die termination resistances.
 *
 * ZenTimings on Windows reads the same table (HKLM\HARDWARE\DESCRIPTION\System\
 * BIOS\AOD in the registry) and decodes it with ZenStates-Core's
 * AodDictionaries/AodData/EncodedValueDictionaries. Here we read the same
 * SystemMemory OperationRegion through the bundled aod_voltages kernel module
 * (/sys/kernel/aod_voltages/full_raw), which locates AODE/AODT by scanning the
 * ACPI SSDTs, and apply the same offset algorithm.
 *
 * The layout is self-locating: the record is anchored on the u16 that holds
 * MCLK, and every field is a fixed byte offset from it. The offsets past the
 * command-gap block depend on the microcode patch level (see Aod.cs
 * GetAodDataDictionary, GraniteRidge/Turin branch), so we pick the variant the
 * same way ZenStates-Core does.
 */

#define AOD_BLOB_MAX 16384

typedef struct {
    bool valid;             /* set once the blob was found, anchored and parsed */

    /* Raw codes as stored in the AOD table (they are u32 fields) */
    int cad_bus_ds;         /* Proc CA drive strength                (CadBusDrvStren) */
    int proc_odt;           /* CPU on-die termination, pull-up       (ProcOdt)        */
    int proc_odt_pd;        /* CPU on-die termination, pull-down     (ProcOdtPullDown)*/
    int proc_dq_ds;         /* Proc DQ drive strength, pull-up       (ProcDataDrvStren)*/
    int proc_dq_ds_pd;      /* Proc DQ drive strength, pull-down     (ProcDqDsPullDown)*/
    int dram_dq_ds;         /* DRAM DQ drive strength, pull-up       (DramDataDrvStren)*/
    int dram_dq_ds_pd;      /* DRAM DQ drive strength, pull-down     (DramDqDsPullDown)*/
    int proc_cs_ds;         /* Proc CS drive strength */
    int proc_ck_ds;         /* Proc CK drive strength */
    int rtt_nom_wr;         /* DRAM RTT_NOM_WR (MR34) */
    int rtt_nom_rd;         /* DRAM RTT_NOM_RD (MR35) */
    int rtt_wr;             /* DRAM RTT_WR     (MR33) */
    int rtt_park;           /* DRAM RTT_PARK   (MR35) */
    int rtt_park_dqs;       /* DRAM DQS_RTT_PARK (MR33) */

    /* Diagnostics: which layout we used, and where the record lives */
    int  anchor;            /* byte offset of the MemClk u16 inside the AOD blob */
    int  mclk;              /* MCLK the anchor was matched against (MHz)         */
    uint32_t patch_level;   /* microcode patch level, for the offset variant     */
    bool new_layout;        /* patch level > 0xB404022 (the 1Ah_B404023 layout)  */
    bool rmp_layout;        /* which of the two offset sets was used (see Aod.cs) */
    char source[64];        /* where the blob came from                           */
} dram_resistances_t;

/* mclk_mhz is the UMC memory clock (DDR5-5200 -> 2600), i.e. the same value
 * dram_timings_t.frequency_hint_mhz carries. */
void aod_read_resistances(int mclk_mhz, dram_resistances_t *out);

/* Code -> ZenTimings' vocabulary. Each returns a static string; unknown codes
 * render as "?". */
const char *aod_rtt_name(int code);         /* RTT_NOM_WR/RD, RTT_WR, RTT_PARK, DQS_RTT_PARK */
const char *aod_proc_odt_name(int code);    /* ProcOdt, Proc*DqDs (the RZQ/(n/2) ladder)     */
const char *aod_dram_dq_ds_name(int code);  /* DRAM DQ drive strength (34/40/48)             */
const char *aod_cad_bus_name(int code);     /* CAD bus / CS / CK drive strength (30..120)    */

#ifdef __cplusplus
}
#endif

#endif /* AOD_H */
