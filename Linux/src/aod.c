/*
 * aod.c — read the AMD AOD table through the aod_voltages kernel module and
 * decode the trained drive strengths and termination resistances.
 *
 * Port of ZenStates-Core's Aod.cs / AodDictionaries.cs / AodData.cs /
 * EncodedValueDictionaries.cs (irusanov), GraniteRidge + Turin branch, so the
 * numbers match what ZenTimings shows on Windows for the same boot.
 */

#include "aod.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AOD_SYSFS "/sys/kernel/aod_voltages/full_raw"

/* The u32 fields are laid out from the anchor (the MemClk record) exactly as
 * Aod.cs GetBaseDictionaryByFrequency()/GetAodDataDictionary() place them. */
#define AOD_BASE_SMTEN        (-4)
#define AOD_BASE_MEMCLK       (0)
#define AOD_BASE_TCL          (4)
#define AOD_BASE_CAD_BUS_DS   (104)   /* CadBusDrvStren, and the lastOffset */

/* patch level > 0xB404022 selects the 1Ah_B404023 layout */
#define AOD_PATCH_B404022     0xB404022u

static uint32_t read_patch_level(void)
{
    /* /proc/cpuinfo: "microcode\t: 0xb404031" */
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            char *colon = strchr(line, ':');
            if (!colon) continue;
            if (strncmp(line, "microcode", 9) != 0) continue;
            unsigned v = 0;
            if (sscanf(colon + 1, " %x", &v) == 1) { fclose(f); return v; }
        }
        fclose(f);
    }
    /* Newer kernels also expose it per-CPU. */
    FILE *g = fopen("/sys/devices/system/cpu/cpu0/microcode/patch_level", "r");
    if (g) {
        unsigned v = 0;
        if (fscanf(g, "%x", &v) == 1) { fclose(g); return v; }
        fclose(g);
    }
    return 0;
}

static int load_blob(uint8_t *buf, size_t cap, char *src_name, size_t src_sz)
{
    FILE *f = fopen(AOD_SYSFS, "rb");
    if (!f) return 0;
    size_t total = 0;
    /* The sysfs bin attribute hands out the region in chunks; keep reading. */
    while (total < cap) {
        size_t n = fread(buf + total, 1, cap - total, f);
        if (n == 0) break;
        total += n;
    }
    fclose(f);
    if (total < 512) return 0;
    if (src_name && src_sz) snprintf(src_name, src_sz, "aod_voltages full_raw, %zu B", total);
    return (int)total;
}

/* ZenStates-Core uses FindLastSequence: the LAST match wins (the table carries
 * per-frequency records and the active one is the trailing one). */
static int find_anchor(const uint8_t *blob, int len, int mclk)
{
    if (mclk <= 0 || mclk > 0xFFFF) return -1;
    const int lo = mclk & 0xFF, hi = (mclk >> 8) & 0xFF;
    int found = -1;
    for (int i = 0; i + 1 < len; i++) {
        if (blob[i] == lo && blob[i + 1] == hi) found = i;
    }
    return found;
}

static int field(const uint8_t *blob, int len, int at)
{
    if (at < 0 || at + 4 > len) return -1;
    return (int)((uint32_t)blob[at] | ((uint32_t)blob[at + 1] << 8) |
                 ((uint32_t)blob[at + 2] << 16) | ((uint32_t)blob[at + 3] << 24));
}

/* Which offset set the BIOS uses. ZenStates-Core picks it from the presence of
 * the "Set RMP Profile" WMI method; ASUS AM5 boards expose it, and on this box
 * the RMP set is the one that decodes (ProcODT 48 Ω, CS/CK/CA 30 Ω, DRAM DQ 34,
 * Proc DQ 34.3 — the non-RMP set yields a 160 Ω ProcODT and an illegal 14 for
 * CS/CK). QTUX_AOD_RMP=0 A/B-tests the other set. */
static bool want_rmp_layout(void)
{
    const char *e = getenv("QTUX_AOD_RMP");
    if (e && e[0] == '0') return false;
    return true;
}

void aod_read_resistances(int mclk_mhz, dram_resistances_t *out)
{
    static uint8_t blob[AOD_BLOB_MAX];
    memset(out, 0, sizeof(*out));
    out->anchor = -1;
    out->mclk = mclk_mhz;
    out->patch_level = read_patch_level();
    out->new_layout = out->patch_level > AOD_PATCH_B404022;

    int len = load_blob(blob, sizeof(blob), out->source, sizeof(out->source));
    if (len <= 0) {
        snprintf(out->source, sizeof(out->source), "aod_voltages module not loaded");
        return;
    }

    int anchor = find_anchor(blob, len, mclk_mhz);
    if (anchor < 0) {
        snprintf(out->source, sizeof(out->source), "AOD record for MCLK %d not found", mclk_mhz);
        return;
    }
    out->anchor = anchor;

    const int last = anchor + AOD_BASE_CAD_BUS_DS;   /* == CadBusDrvStren offset */
    out->cad_bus_ds = field(blob, len, anchor + AOD_BASE_CAD_BUS_DS);
    const bool rmp = want_rmp_layout();
    out->rmp_layout = rmp;

    if (out->new_layout) {
        out->rtt_nom_wr   = field(blob, len, last + 4);
        out->rtt_nom_rd   = field(blob, len, last + 8);
        out->rtt_wr       = field(blob, len, last + 12);
        out->rtt_park     = field(blob, len, last + 16);
        out->rtt_park_dqs = field(blob, len, last + 20);
        if (rmp) {
            out->proc_odt       = field(blob, len, last + 144);
            out->proc_odt_pd    = field(blob, len, last + 148);
            out->dram_dq_ds     = field(blob, len, last + 152);
            out->dram_dq_ds_pd  = field(blob, len, last + 156);
            out->proc_cs_ds     = field(blob, len, last + 160);
            out->proc_ck_ds     = field(blob, len, last + 164);
            out->proc_dq_ds     = field(blob, len, last + 168);
            out->proc_dq_ds_pd  = field(blob, len, last + 172);
        } else {
            out->proc_odt       = field(blob, len, last + 132);
            out->proc_odt_pd    = field(blob, len, last + 136);
            out->dram_dq_ds     = field(blob, len, last + 140);
            out->dram_dq_ds_pd  = field(blob, len, last + 140);  /* same u32 */
            out->proc_cs_ds     = field(blob, len, last + 144);
            out->proc_ck_ds     = field(blob, len, last + 148);
            out->proc_dq_ds     = field(blob, len, last + 152);
            out->proc_dq_ds_pd  = field(blob, len, last + 156);
        }
    } else {
        out->proc_dq_ds     = field(blob, len, last + 4);
        out->rtt_nom_wr     = field(blob, len, last + 8);
        out->rtt_nom_rd     = field(blob, len, last + 12);
        out->rtt_wr         = field(blob, len, last + 16);
        out->rtt_park       = field(blob, len, last + 20);
        out->rtt_park_dqs   = field(blob, len, last + 24);
        if (rmp) {
            out->proc_odt      = field(blob, len, last + 136);
            out->proc_odt_pd   = field(blob, len, last + 140);
            out->dram_dq_ds    = field(blob, len, last + 144);
            out->dram_dq_ds_pd = out->dram_dq_ds;
            out->proc_cs_ds    = out->cad_bus_ds;
            out->proc_ck_ds    = out->cad_bus_ds;
            out->proc_dq_ds_pd = out->proc_dq_ds;
        } else {
            out->proc_odt      = field(blob, len, last + 148);
            out->proc_odt_pd   = field(blob, len, last + 152);
            out->dram_dq_ds    = field(blob, len, last + 156);
            out->dram_dq_ds_pd = out->dram_dq_ds;
            out->proc_cs_ds    = out->cad_bus_ds;
            out->proc_ck_ds    = out->cad_bus_ds;
            out->proc_dq_ds_pd = out->proc_dq_ds;
        }
    }

    out->valid = true;
}

/* ── Code -> impedance, the EncodedValueDictionaries tables ─────────────── */

const char *aod_rtt_name(int code)
{
    /* RttDict: 0 = Off, 1..7 = RZQ/1..RZQ/7 over the 240 ohm RZQ reference. */
    static char buf[32];
    if (code <= 0 || code > 7) return code < 0 ? "—" : "?";
    const double ohm = 240.0 / (double)code;
    snprintf(buf, sizeof(buf), "RZQ/%d (%.0f Ω)", code, ohm);
    return buf;
}

const char *aod_proc_odt_name(int code)
{
    /* ProcOdtDict / ProcDataDrvStrenDict: the Zen PHY ladder is RZQ/(n/2),
     * i.e. 240/(n/2) = 480/n ohms. Codes step by half-rungs. */
    switch (code) {
    case 0:  return "Hi-Z";
    case 1:  return "480.0 Ω";
    case 2:  return "240.0 Ω";
    case 3:  return "160.0 Ω";
    case 4:  return "120.0 Ω";
    case 5:  return "96.0 Ω";
    case 6:  return "80.0 Ω";
    case 7:  return "68.6 Ω";
    case 12: return "60.0 Ω";
    case 13: return "53.3 Ω";
    case 14: return "48.0 Ω";
    case 15: return "43.6 Ω";
    case 28: return "40.0 Ω";
    case 29: return "36.9 Ω";
    case 30: return "34.3 Ω";
    case 31: return "32.0 Ω";
    case 60: return "30.0 Ω";
    case 61: return "28.2 Ω";
    case 62: return "26.7 Ω";
    case 63: return "25.3 Ω";
    case -1: return "—";
    default: return "?";
    }
}

const char *aod_dram_dq_ds_name(int code)
{
    /* DramDataDrvStrenDict */
    switch (code) {
    case 0: return "34.0 Ω";
    case 1: return "40.0 Ω";
    case 2: return "48.0 Ω";
    case -1: return "—";
    default: return "?";
    }
}

const char *aod_cad_bus_name(int code)
{
    /* CadBusDrvStrenDict / ProcImpedanceDict (CA, CS, CK) */
    switch (code) {
    case 0:   return "Hi-Z";
    case 30:  return "30.0 Ω";
    case 40:  return "40.0 Ω";
    case 60:  return "60.0 Ω";
    case 120: return "120.0 Ω";
    case -1:  return "—";
    default:  return "?";
    }
}
