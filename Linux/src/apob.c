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
 * Field offsets come from ZenStates-Core's ApobLayout for this platform
 * (ApobData reads each field through Layout.Offsets, and the encodings render
 * through EncodedValueDictionaries). Until the offset table is filled in per
 * field, everything reports -1 so the UI shows an em dash rather than a
 * plausible-looking zero.
 */
const char *apob_vref_dq_name(int code)
{
    /* MR10 VrefDQ: 8-bit code, 35.0 % to 97.5 % of VDDQ in 0.5 % steps. */
    static char buf[32];
    if (code < 0) return "—";
    if (code > 125) return "?";
    snprintf(buf, sizeof(buf), "%.1f %% VDDQ", 35.0 + 0.5 * (double)code);
    return buf;
}

bool apob_read_mem_config(apob_mem_config_t *out)
{
    apob_block_t ab;
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
    snprintf(out->source, sizeof(out->source), "APOB v%u @0x%llX",
             ab.version, (unsigned long long)ab.address);

    /* Offsets are populated by apob_layout.h once the platform map is wired. */
    out->valid = false;
    return false;
}
