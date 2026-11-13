/*
 * apob.c — locate and read the AMD APOB block from physical memory.
 *
 * The scan is anchored on /proc/iomem: APOB is written just under the top of
 * the highest System RAM range below 4 GiB, so the common case is a handful of
 * 2 MiB reads instead of the full 4 GiB (measured: 2m45s for a blind 4 GiB
 * pread scan on this box, versus milliseconds for the anchored scan).
 */

#include "apob.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define APOB_SIG 0x424F5041u   /* "APOB" */
#define CHUNK    (2u * 1024u * 1024u)
#define SCAN_CAP (256u * 1024u * 1024u)   /* anchored pass: 256 MiB below the top */

static uint8_t *s_blob;        /* the loaded block, kept for the process */
static uint64_t s_addr;
static uint32_t s_size;

static bool header_sane(uint32_t sig, uint32_t version, uint32_t size, uint32_t hdr)
{
    return sig == APOB_SIG && version >= 1 && version <= 64 &&
           size >= 256 && size <= APOB_MAX_SIZE && hdr >= 16 && hdr <= size;
}

uint64_t apob_low_ram_top(void)
{
    FILE *f = fopen("/proc/iomem", "r");
    if (!f) return 0;
    char line[256];
    uint64_t top = 0;
    while (fgets(line, sizeof(line), f)) {
        unsigned long long lo = 0, hi = 0;
        char *colon = strchr(line, ':');
        if (!colon) continue;
        if (sscanf(line, " %llx - %llx", &lo, &hi) != 2) continue;
        if (strncmp(colon + 1, " System RAM", 11) != 0) continue;
        uint64_t end = hi + 1;
        if (end <= 0x100000000ull && end > top) top = end;
    }
    fclose(f);
    return top;
}

static bool read_at(int fd, void *dst, size_t n, uint64_t addr)
{
    return pread(fd, dst, n, (off_t)addr) == (ssize_t)n;
}

/* Try to use a candidate offset found inside a scanned chunk. */
static bool try_candidate(int fd, uint64_t addr, apob_block_t *out)
{
    uint32_t head[4];
    if (!read_at(fd, head, sizeof(head), addr)) return false;
    if (!header_sane(head[0], head[1], head[2], head[3])) return false;

    uint8_t *blob = malloc(head[2]);
    if (!blob) return false;
    if (!read_at(fd, blob, head[2], addr)) { free(blob); return false; }

    free(s_blob);
    s_blob = blob;
    s_size = head[2];
    s_addr = addr;

    out->found = true;
    out->address = addr;
    out->version = head[1];
    out->table_size = head[2];
    out->header_size = head[3];
    out->data = blob;
    snprintf(out->note, sizeof(out->note), "signature scan @0x%llX", (unsigned long long)addr);
    return true;
}

static bool scan_down(int fd, uint64_t from, uint64_t span, apob_block_t *out)
{
    static uint8_t buf[CHUNK];
    uint64_t floor = (from > span) ? (from - span) : 0;

    for (uint64_t base = from - CHUNK; ; base -= CHUNK) {
        uint64_t start = (base < floor) ? floor : base;
        size_t n = (size_t)((base + CHUNK) - start);
        if (!read_at(fd, buf, n, start)) {
            if (start == floor) break;
            continue;
        }
        /* Highest match in the chunk wins — the active record trails. */
        uint64_t hit = 0;
        bool have_hit = false;
        for (size_t o = 0; o + 16 <= n; o += 4) {
            uint32_t sig;
            memcpy(&sig, buf + o, 4);
            if (sig != APOB_SIG) continue;
            uint32_t ver, sz, hdr;
            memcpy(&ver, buf + o + 4, 4);
            memcpy(&sz,  buf + o + 8, 4);
            memcpy(&hdr, buf + o + 12, 4);
            if (!header_sane(sig, ver, sz, hdr)) continue;
            hit = start + o;
            have_hit = true;
        }
        if (have_hit && try_candidate(fd, hit, out)) return true;
        if (start == floor) break;
        if (base < CHUNK) break;
    }
    return false;
}

bool apob_load(apob_block_t *out)
{
    memset(out, 0, sizeof(*out));

    /* Cached from an earlier refresh: revalidate cheaply. */
    if (s_blob) {
        uint32_t sig, ver, sz, hdr;
        memcpy(&sig, s_blob, 4);
        memcpy(&ver, s_blob + 4, 4);
        memcpy(&sz,  s_blob + 8, 4);
        memcpy(&hdr, s_blob + 12, 4);
        if (header_sane(sig, ver, sz, hdr)) {
            out->found = true;
            out->address = s_addr;
            out->version = ver;
            out->table_size = sz;
            out->header_size = hdr;
            out->data = s_blob;
            snprintf(out->note, sizeof(out->note), "cached");
            return true;
        }
        free(s_blob);
        s_blob = NULL;
    }

    int fd = open("/dev/mem", O_RDONLY);
    if (fd < 0) {
        snprintf(out->note, sizeof(out->note), "/dev/mem unavailable (need root)");
        return false;
    }

    uint64_t top = apob_low_ram_top();
    out->scanned_from = top;
    if (top >= 0x1000 && scan_down(fd, top, SCAN_CAP, out)) { close(fd); return true; }

    /* Nothing under the low-RAM top: fall back to the whole 4 GiB. */
    if (scan_down(fd, 0x100000000ull, 0x100000000ull, out)) { close(fd); return true; }

    snprintf(out->note, sizeof(out->note), "no APOB signature below 4 GiB");
    close(fd);
    return false;
}

uint32_t apob_u32(const apob_block_t *b, size_t off)
{
    if (!b->found || !b->data || off + 4 > b->table_size) return 0;
    uint32_t v;
    memcpy(&v, b->data + off, 4);
    return v;
}

uint16_t apob_u16(const apob_block_t *b, size_t off)
{
    if (!b->found || !b->data || off + 2 > b->table_size) return 0;
    uint16_t v;
    memcpy(&v, b->data + off, 2);
    return v;
}

const char *apob_group_odt_name(int code)
{
    /* GroupOdtImpedanceDict: the CA/CK/CS command-bus terminators. Note the
     * 480 ohm rung that the DQ ladder does not have, and no 48 ohm rung. */
    switch (code) {
    case 0:  return "Off";
    case 1:  return "480.0 Ω";
    case 2:  return "240.0 Ω";
    case 3:  return "120.0 Ω";
    case 4:  return "80.0 Ω";
    case 5:  return "60.0 Ω";
    case 6:  return "48.0 Ω";
    case 7:  return "40.0 Ω";
    case -1: return "—";
    default: return "?";
    }
}

uint8_t apob_u8(const apob_block_t *b, size_t off)
{
    if (!b->found || !b->data || off >= b->table_size) return 0;
    return b->data[off];
}

/*
 * The entry walk (ZenStates-Core ApobCoreMap.EnumerateEntries / WalkChain,
 * :207-252, and Apob.GetConfigOffsets, :453-474).
 *
 * Each entry is a 48-byte header — u32 group, u32 type, u32 instance, u32
 * size covering the header, then a 32-byte HMAC — and the table is a linked
 * list: the next entry is at off += size, with no alignment padding. The
 * chain starts at header_size; the config list of u32 entry offsets at 0x30
 * (up to header_size − 32, where the trailing hash begins) seeds extra
 * chains, because some groups are not reachable from the first one. A chain
 * stops at the first entry whose size is unusable, and at a repeat; group and
 * type are the low 16 bits, the high half 0xFFFF marking an entry whose data
 * is obfuscated. The list is sorted by offset, so "first match" is table
 * order.
 */
static void walk_chain(const apob_block_t *b, size_t off, apob_entry_list_t *el)
{
    for (unsigned n = 0; n < APOB_MAX_ENTRIES; n++) {
        if (off == 0 || off + 16 > b->table_size) break;

        bool seen = false;
        for (size_t i = 0; i < el->count; i++)
            if (el->v[i].off == off) { seen = true; break; }
        if (seen) break;

        uint32_t size = apob_u32(b, off + 12);
        if (size < 16 || off + size > b->table_size) break;

        if (el->count == APOB_MAX_ENTRIES) break;
        apob_entry_t *e = &el->v[el->count++];
        e->raw_group   = apob_u32(b, off + 0);
        e->raw_type    = apob_u32(b, off + 4);
        e->group       = e->raw_group & 0xFFFFu;
        e->type        = e->raw_type & 0xFFFFu;
        e->instance    = apob_u32(b, off + 8);
        e->size        = size;
        e->off         = off;
        e->encrypted   = (e->raw_group >> 16) == 0xFFFFu || (e->raw_type >> 16) == 0xFFFFu;
        off += size;
    }
}

static int by_offset(const void *a, const void *b)
{
    const apob_entry_t *x = a, *y = b;
    return (x->off > y->off) - (x->off < y->off);
}

size_t apob_walk_entries(const apob_block_t *b, apob_entry_list_t *el)
{
    el->count = 0;
    if (!b->found || !b->data || b->header_size < APOB_CFG_LIST + 4) return 0;

    walk_chain(b, b->header_size, el);

    /* Config list: 0x30 .. header_size−32, the 32-byte hash sits at the end. */
    size_t region = (b->header_size > APOB_CFG_LIST + APOB_HDR_HASH)
                  ? b->header_size - APOB_CFG_LIST - APOB_HDR_HASH : 0;
    for (size_t o = 0; o + 4 <= region; o += 4) {
        uint32_t target = apob_u32(b, APOB_CFG_LIST + o);
        if (target && (size_t)target + APOB_ENTRY_OFF + 4 < b->table_size)
            walk_chain(b, target, el);
    }

    qsort(el->v, el->count, sizeof(el->v[0]), by_offset);
    return el->count;
}

const apob_entry_t *apob_find_entry(const apob_entry_list_t *el, uint32_t group, uint32_t type)
{
    for (size_t i = 0; i < el->count; i++)
        if (!el->v[i].encrypted && el->v[i].group == group && el->v[i].type == type)
            return &el->v[i];
    return NULL;
}

/*
 * Zen 5's GEN/3 entry holds a timing block per channel, u16 fields: data rate
 * at +0, MEMCLK at +2, half-MEMCLK at +4, CL at +8. The block is found the way
 * ApobChannelTimings.Read finds it (:342-450): 2-byte granularity, rate = 2 ×
 * MEMCLK, MEMCLK 800..7000, half-MEMCLK consistent, CL 10..100. This returns
 * the first block, i.e. channel 0, P-state 0 — the block the extended ODT
 * record hangs off.
 */
bool apob_timing_block(const apob_block_t *b, size_t entry_off, size_t entry_end, size_t *rel_out)
{
    size_t data = entry_off + APOB_ENTRY_HDR;
    for (size_t o = data; o + APOB_ZS5_TIMING_SPAN <= entry_end; o += 2) {
        uint32_t rate = apob_u16(b, o + 0x00);
        uint32_t clk  = apob_u16(b, o + 0x02);
        uint32_t half = apob_u16(b, o + 0x04);
        uint32_t cl   = apob_u16(b, o + 0x08);
        if (clk < 800 || clk > 7000) continue;
        if (rate != 2u * clk) continue;
        if (half * 2u != clk - (clk & 1u)) continue;
        if (cl < 10 || cl > 100) continue;
        *rel_out = o;                       /* table-relative, channel 0 / P-state 0 */
        return true;
    }
    return false;
}

/*
 * Encodings are the ones ZenStates-Core renders through EncodedValues /
 * EncodedValueDictionaries:
 *   GroupOdtImpedanceDict  code 0..7 → Off/480/240/120/80/60/48/40 Ω
 *   PhyVref                % of VDDQ = code · 100 / 128
 *   DramVref (MR10/11/12)  % of VDDQ = 97.5 − 0.5 · code, codes ≤ 0x7D
 *                            (LPDDR5 is 10 + 0.5 · code, codes ≤ 0x7F)
 */
const char *apob_vref_dq_name(int code)
{
    /* DDR5 MR10 VrefDQ: 97.5 % of VDDQ down to 58.5 % in 0.5 % steps. */
    static char buf[32];
    if (code < 0) return "—";
    if (code > 125) return "?";
    snprintf(buf, sizeof(buf), "%.1f %% VDDQ", 97.5 - 0.5 * (double)code);
    return buf;
}

const char *apob_phy_vref_name(int code)
{
    /* The PHY receiver threshold: VDDQ · code / 128, a 128-step ladder — a
     * different encoder from the DRAM MR10/11/12 Vrefs. */
    static char buf[32];
    if (code < 0) return "—";
    snprintf(buf, sizeof(buf), "%.1f %% VDDQ", (double)code * 100.0 / 128.0);
    return buf;
}

/* ── Zen 5 desktop (Granite Ridge) record map ─────────────────────────────
 * From ZenStates-Core ApobLayout.cs: Zen5MainOffsets (:319) and
 * Zen5ExtendedOffsets (:354), both read as 0x30-byte records (Zen5MainLayout
 * / Zen5ExtendedLayout, :452-453). Main = MEM group type 25, one record per
 * channel; extended = GEN group type 3, at +0x41F from the channel's timing
 * block (Zen5ChannelTimingLayout, :474). Every field is one unsigned byte.
 * -1 = the desktop layout does not place that field.
 */
#define REC_GDM          0x01   /* decoded by no one in ZenStates-Core   */
#define REC_RTT_NOM_RD   0x02
#define REC_CK_ODT_A     0x08
#define REC_CS_ODT_A     0x09
#define REC_CA_ODT_A     0x0A
#define REC_CK_ODT_B     0x0B
#define REC_CS_ODT_B     0x0C
#define REC_CA_ODT_B     0x0D
#define REC_PHY_VREF     0x14
#define REC_DRAM_VREF_DQ 0x15
#define REC_DRAM_VREF_CA 0x16
#define REC_DRAM_VREF_CS 0x17
#define REC_SIZE         0x30

/* The record is "used" when the RTT block (RttNomRd..RttParkDqs) is not all
 * zero — the same test Apob.ParseDataBlocks stops the scan on. */
static bool record_used(const apob_block_t *b, size_t rec)
{
    if (rec + REC_SIZE > b->table_size) return false;
    for (size_t o = REC_RTT_NOM_RD; o < REC_RTT_NOM_RD + 5; o++)
        if (apob_u8(b, rec + o)) return true;
    return false;
}

static int rec_field(const apob_block_t *b, size_t rec, int off)
{
    if (off < 0 || rec == (size_t)-1) return -1;
    return apob_u8(b, rec + (size_t)off);
}

/* main wins, extended fills the holes — ZenTimings' ApobDataView merge. */
static int pick(const apob_block_t *b, size_t main, size_t ext, int off)
{
    int v = rec_field(b, main, off);
    if (v < 0) v = rec_field(b, ext, off);
    return v;
}

bool apob_read_mem_config(apob_mem_config_t *out)
{
    apob_block_t ab;
    apob_entry_list_t el;
    const apob_entry_t *e;

    memset(out, 0, sizeof(*out));
    out->ca_odt_a = out->ca_odt_b = -1;
    out->ck_odt_a = out->ck_odt_b = -1;
    out->cs_odt_a = out->cs_odt_b = -1;
    out->phy_vref = out->dram_vref_dq = out->dram_vref_ca = out->dram_vref_cs = -1;
    out->vddio_mv = out->vddq_mv = out->vdd_mv = out->vpp_mv = -1;
    out->vddio_at = (size_t)-1;

    if (!apob_load(&ab)) {
        snprintf(out->source, sizeof(out->source), "%s", ab.note);
        return false;
    }
    apob_walk_entries(&ab, &el);
    out->entry_count = (int)el.count;

    /* The trained record set: MEM/25, channel 0. */
    size_t main_rec = (size_t)-1;
    size_t ext_rec  = (size_t)-1;
    e = apob_find_entry(&el, APOB_GROUP_MEM, APOB_TYPE_MEM_SYS_CFG);
    if (e && e->size >= APOB_ENTRY_HDR + REC_SIZE) {
        size_t data = e->off + APOB_ENTRY_HDR;
        size_t size = e->size - APOB_ENTRY_HDR;
        if (size % REC_SIZE == 0 && record_used(&ab, data)) {
            main_rec = data;
            out->channels = (int)(size / REC_SIZE);
            out->main_at = data;
        }
    }
    /* The extended copy, from GEN/3's first channel timing block. */
    size_t blk = (size_t)-1;
    e = apob_find_entry(&el, APOB_GROUP_GEN, APOB_TYPE_GEN_CFG);
    if (e && apob_timing_block(&ab, e->off, e->off + e->size, &blk)) {
        size_t cand = blk + APOB_ZS5_EXT_OFF;
        if (record_used(&ab, cand)) { ext_rec = cand; out->ext_at = cand; }
    }

    if (main_rec != (size_t)-1 || ext_rec != (size_t)-1) {
        out->ck_odt_a = pick(&ab, main_rec, ext_rec, REC_CK_ODT_A);
        out->cs_odt_a = pick(&ab, main_rec, ext_rec, REC_CS_ODT_A);
        out->ca_odt_a = pick(&ab, main_rec, ext_rec, REC_CA_ODT_A);
        out->ck_odt_b = pick(&ab, main_rec, ext_rec, REC_CK_ODT_B);
        out->cs_odt_b = pick(&ab, main_rec, ext_rec, REC_CS_ODT_B);
        out->ca_odt_b = pick(&ab, main_rec, ext_rec, REC_CA_ODT_B);
        out->phy_vref      = pick(&ab, main_rec, ext_rec, REC_PHY_VREF);
        out->dram_vref_dq  = pick(&ab, main_rec, ext_rec, REC_DRAM_VREF_DQ);
        out->dram_vref_ca  = pick(&ab, main_rec, ext_rec, REC_DRAM_VREF_CA);
        out->dram_vref_cs  = pick(&ab, main_rec, ext_rec, REC_DRAM_VREF_CS);
        out->valid = true;
    }

    /* The programmed rails. MEM/28 (the RMP / active memory profile) is a
     * u16-per-field record: flags 0/2/4, MEMCLK 6, VDD 8, CAS 10 — all mV /
     * MHz / clocks. ZenStates-Core names the +8 field VddMv; it is the
     * profile's DRAM rail, the number the BIOS VDDIO_MEM knob tracks. */
    e = apob_find_entry(&el, APOB_GROUP_MEM, APOB_TYPE_MEM_RMP);
    if (e && e->size >= APOB_ENTRY_HDR + 12) {
        size_t data = e->off + APOB_ENTRY_HDR;
        uint16_t mv = apob_u16(&ab, data + 8);
        if (mv > 500 && mv < 3000) {
            out->vddio_mv = mv;
            out->vddio_at = data + 8;
            out->rmp_memclk = apob_u16(&ab, data + 6);
            out->rmp_cas    = apob_u16(&ab, data + 10);
            out->valid = true;
        }
    }

    /* SMBIOS/8: the per-DIMM record the BIOS builds type 17 from. Present
     * bit 0x80, speed u16 at +4 (MEMCLK on DDR5), voltage u16 mV at +6. */
    e = apob_find_entry(&el, APOB_GROUP_SMBIOS, APOB_TYPE_SMBIOS_MEM);
    if (e && e->size >= APOB_ENTRY_HDR + 4) {
        size_t data = e->off + APOB_ENTRY_HDR;
        size_t n = apob_u8(&ab, data + 1);
        if (4 + n * 8 <= e->size - APOB_ENTRY_HDR) {
            for (size_t i = 0; i < n; i++) {
                size_t rec = data + 4 + i * 8;
                if (!(apob_u8(&ab, rec) & 0x80)) continue;
                uint16_t mv = apob_u16(&ab, rec + 6);
                if (mv > 500 && mv < 3000) { out->vdd_mv = mv; break; }
            }
        }
    }

    if (out->valid)
        snprintf(out->source, sizeof(out->source), "APOB v%u ch0/%d %u entries",
                 ab.version, out->channels > 0 ? out->channels : 0, (unsigned)el.count);
    else
        snprintf(out->source, sizeof(out->source), "APOB v%u @0x%llX: no MEM/25 record",
                 ab.version, (unsigned long long)ab.address);
    return out->valid;
}
