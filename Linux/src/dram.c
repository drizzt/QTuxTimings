#include "dram.h"
#include "util.h"
#include <string.h>
#include <stdio.h>

#define SMN_PATH "/sys/kernel/ryzen_smu_drv/smn"

static uint32_t read_smn(uint32_t address)
{
    FILE *f = fopen(SMN_PATH, "r+b");
    if (!f) return 0;
    /* Write address (little-endian) */
    uint8_t buf[4];
    buf[0] = (uint8_t)(address);
    buf[1] = (uint8_t)(address >> 8);
    buf[2] = (uint8_t)(address >> 16);
    buf[3] = (uint8_t)(address >> 24);
    if (fwrite(buf, 1, 4, f) != 4) { fclose(f); return 0; }
    fflush(f);
    /* Read back value */
    rewind(f);
    if (fread(buf, 1, 4, f) != 4) { fclose(f); return 0; }
    fclose(f);
    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

static float to_nanoseconds(uint32_t cycles, float freq_mhz)
{
    if (freq_mhz <= 0) return 0;
    float ns = (float)cycles * 2000.0f / freq_mhz;
    if (ns > (float)cycles) ns /= 2.0f;
    return ns;
}

/* Common timing extraction from SMN registers — shared between DDR4 and DDR5 */
static void read_common_timings(uint32_t offset, dram_timings_t *d)
{
    uint32_t reg50204 = read_smn(offset | 0x50204);
    uint32_t reg50208 = read_smn(offset | 0x50208);
    uint32_t reg5020C = read_smn(offset | 0x5020C);
    uint32_t reg50210 = read_smn(offset | 0x50210);
    uint32_t reg50214 = read_smn(offset | 0x50214);
    uint32_t reg50218 = read_smn(offset | 0x50218);
    uint32_t reg5021C = read_smn(offset | 0x5021C);
    uint32_t reg50220 = read_smn(offset | 0x50220);
    uint32_t reg50224 = read_smn(offset | 0x50224);
    uint32_t reg50228 = read_smn(offset | 0x50228);
    uint32_t reg50230 = read_smn(offset | 0x50230);
    uint32_t reg50234 = read_smn(offset | 0x50234);
    uint32_t reg50250 = read_smn(offset | 0x50250);
    uint32_t reg50254 = read_smn(offset | 0x50254);
    uint32_t reg50258 = read_smn(offset | 0x50258);
    uint32_t reg502A4 = read_smn(offset | 0x502A4);

    d->tcl     = bit_slice(reg50204, 5, 0);
    d->trcd_rd = bit_slice(reg50204, 21, 16);
    d->trcd_wr = bit_slice(reg50204, 29, 24);
    if (d->trcd_wr == 0) d->trcd_wr = d->trcd_rd;
    d->tras    = bit_slice(reg50204, 14, 8);
    d->trp     = bit_slice(reg50208, 21, 16);
    d->trc     = bit_slice(reg50208, 7, 0);

    d->trrds   = bit_slice(reg5020C, 4, 0);
    d->trrdl   = bit_slice(reg5020C, 12, 8);
    d->tfaw    = bit_slice(reg50210, 7, 0);
    d->rtp     = bit_slice(reg5020C, 28, 24);
    d->wtrs    = bit_slice(reg50214, 12, 8);
    d->wtrl    = bit_slice(reg50214, 22, 16);
    d->tcwl    = bit_slice(reg50214, 5, 0);
    d->twr     = bit_slice(reg50218, 7, 0);
    if (d->twr == 0) d->twr = d->wtrs;

    d->trc_page = bit_slice(reg5021C, 31, 20);
    d->ppd      = bit_slice(reg5021C, 2, 0);

    d->rdrd_scl = bit_slice(reg50220, 29, 24);
    d->rdrd_sc  = bit_slice(reg50220, 19, 16);
    d->rdrd_sd  = bit_slice(reg50220, 11, 8);
    d->rdrd_dd  = bit_slice(reg50220, 3, 0);

    d->wrwr_scl = bit_slice(reg50224, 29, 24);
    d->wrwr_sc  = bit_slice(reg50224, 19, 16);
    d->wrwr_sd  = bit_slice(reg50224, 11, 8);
    d->wrwr_dd  = bit_slice(reg50224, 3, 0);

    d->rdwr = bit_slice(reg50228, 13, 8);
    d->wrrd = bit_slice(reg50228, 3, 0);
    d->refi = bit_slice(reg50230, 15, 0);

    d->mod_pda = bit_slice(reg50234, 29, 24);
    d->mrd_pda = bit_slice(reg50234, 21, 16);
    d->mod     = bit_slice(reg50234, 13, 8);
    d->mrd     = bit_slice(reg50234, 5, 0);

    d->stag    = bit_slice(reg50250, 26, 16);
    d->stag_sb = bit_slice(reg50250, 8, 0);

    d->cke = bit_slice(reg50254, 28, 24);
    d->xp  = bit_slice(reg50254, 5, 0);

    d->phy_wrd = bit_slice(reg50258, 26, 24);
    d->phy_rdl = bit_slice(reg50258, 23, 16);
    d->phy_wrl = bit_slice(reg50258, 15, 8);

    d->wrpre = bit_slice(reg502A4, 10, 8);
    d->rdpre = bit_slice(reg502A4, 2, 0);
    d->rdpost = bit_slice(reg502A4, 6, 4);
    d->wrpost = bit_slice(reg502A4, 14, 12);

    /* ── Controller config, per ZenStates-Core Ddr5Timings.cs ───────── */
    uint32_t reg5012C = read_smn(offset | 0x5012C);
    d->fgr = bit_slice(reg5012C, 18, 16);
    {
        /* Bank Refresh Mode (M_Ordering): the per-bank bit selects the
         * same-bank family, FGR picks the granularity. This decides which of
         * tRFC/tRFC2/tRFCsb the controller actually enforces. */
        uint32_t per_bank = bit_slice(reg5012C, 1, 1);
        if (per_bank == 0)
            snprintf(d->refresh_mode, sizeof(d->refresh_mode), "%s", d->fgr == 0 ? "Normal" : "FGR");
        else
            snprintf(d->refresh_mode, sizeof(d->refresh_mode), "%s", d->fgr != 0 ? "Mixed" : "PbOnly");
    }

    uint32_t reg50284 = read_smn(offset | 0x50284);   /* Nitro / DFI delay modes */
    d->nitro_ctrl      = reg50284 & 0x3;
    d->nitro_tx        = (reg50284 >> 4) & 0x3;
    d->nitro_rx        = (reg50284 >> 8) & 0x3;
    d->nitro_ctrl_mode = (reg50284 >> 16) & 0x1;
    d->nitro_tx_mode   = (reg50284 >> 17) & 0x1;
    d->nitro_rx_mode   = (reg50284 >> 18) & 0x1;

    uint32_t reg5022C = read_smn(offset | 0x5022C);
    d->zqcs_interval = bit_slice(reg5022C, 29, 20);
    d->tzqcs         = bit_slice(reg5022C, 7, 0);

    d->odts_read_interval = bit_slice(reg50230, 29, 20);

    uint32_t reg50244 = read_smn(offset | 0x50244);
    d->pwr_down_dly = bit_slice(reg50244, 19, 8);
    d->csh          = bit_slice(reg50244, 25, 20);

    uint32_t reg50248 = read_smn(offset | 0x50248);
    d->stab    = bit_slice(reg50248, 9, 0);
    d->srx2srx = bit_slice(reg50248, 15, 10);

    uint32_t reg5025C = read_smn(offset | 0x5025C);
    d->lp_dly      = bit_slice(reg5025C, 5, 0);
    d->lp_exit_dly = bit_slice(reg5025C, 13, 8);

    uint32_t reg50198 = read_smn(offset | 0x50198);
    d->rd_brst_gap = bit_slice(reg50198, 7, 3);
    d->wr_brst_gap = bit_slice(reg50198, 13, 9);

    uint32_t reg50100 = read_smn(offset | 0x50100);
    d->burst_length = bit_slice(reg50100, 9, 8);

    d->ddr_max_rate = bit_slice(read_smn(offset | 0x50DF0), 7, 0);
    d->ccdl_wr2     = bit_slice(read_smn(offset | 0x502E0), 5, 0);

    /* Bank Group Scramble: the register holds the per-bank-group scramble map,
     * and the sentinel 0x87654321 on both instances means "identity", i.e.
     * disabled — ZenStates-Core's test, so the two apps agree on the label. */
    uint32_t bgs0 = read_smn(offset | 0x50050);
    uint32_t bgs1 = read_smn(offset | 0x50058);
    d->bgs_raw = bgs0;
    d->bgs_enabled = !(bgs0 == 0x87654321u && bgs1 == 0x87654321u);
    uint32_t bgsa0 = read_smn(offset | 0x500D0);
    uint32_t bgsa1 = read_smn(offset | 0x500D4);
    d->bgs_alt_raw = bgsa0;
    d->bgs_alt_enabled = (bit_slice(bgsa0, 10, 4) > 0) || (bit_slice(bgsa1, 10, 4) > 0);
}

/*
 * Channel B lives at the next UMC instance, SMN base 0x10000 (channel A is 0).
 * AM5 exposes four UMC instances — 0x0, 0x10000, 0x40000 and 0x50000 all answer
 * — and on a two-channel board the outer pair mirrors the inner pair, so we
 * read 0/0x10000 as A/B and just record which of the mirrors are present.
 */
static void read_channel_b(dram_timings_t *d)
{
    dram_timings_t b;
    memset(&b, 0, sizeof(b));
    read_common_timings(0x10000, &b);

    d->phy_rdl_b = b.phy_rdl;
    d->phy_wrl_b = b.phy_wrl;
    d->phy_wrd_b = b.phy_wrd;
    d->rdwr_b    = b.rdwr;
    d->wrrd_b    = b.wrrd;
    d->phy_matched = (b.phy_rdl == d->phy_rdl &&
                      b.phy_wrl == d->phy_wrl &&
                      b.phy_wrd == d->phy_wrd);

    uint32_t mirrors = 0;
    if (read_smn(0x40000u | 0x50200) != 0xFFFFFFFFu) mirrors |= 1;
    if (read_smn(0x50000u | 0x50200) != 0xFFFFFFFFu) mirrors |= 2;
    d->umc_mirrors = mirrors;
}

static void read_ddr5_timings(dram_timings_t *d)
{
    const uint32_t offset = 0; /* UMC0 */

    /* Ratio -> frequency */
    uint32_t ratio_reg = read_smn(offset | 0x50200);
    float ratio = bit_slice(ratio_reg, 15, 0) / 100.0f;
    float mem_freq = ratio * 200.0f;
    d->frequency_hint_mhz = mem_freq;

    /* GDM, Cmd2T, PowerDown */
    d->gdm_enabled = bit_slice(ratio_reg, 18, 18) == 1;
    uint32_t cmd2t_bit = bit_slice(ratio_reg, 17, 17);
    snprintf(d->cmd2t, sizeof(d->cmd2t), "%s", cmd2t_bit ? "2T" : "1T");
    uint32_t refresh_reg = read_smn(offset | 0x5012C);
    d->power_down_enabled = bit_slice(refresh_reg, 28, 28) == 1;

    read_common_timings(offset, d);
    read_channel_b(d);

    /* RFC - DDR5: choose first non-default from 4 registers */
    uint32_t trfc_regs[4];
    trfc_regs[0] = read_smn(offset | 0x50260);
    trfc_regs[1] = read_smn(offset | 0x50264);
    trfc_regs[2] = read_smn(offset | 0x50268);
    trfc_regs[3] = read_smn(offset | 0x5026C);
    uint32_t trfc_reg = 0;
    for (int i = 0; i < 4; i++) {
        if (trfc_regs[i] != 0x00C00138) { trfc_reg = trfc_regs[i]; break; }
    }
    if (trfc_reg) {
        d->rfc  = bit_slice(trfc_reg, 15, 0);
        d->rfc2 = bit_slice(trfc_reg, 31, 16);
    }

    /* RFCsb */
    uint32_t rfcsb_regs[4];
    rfcsb_regs[0] = bit_slice(read_smn(offset | 0x502C0), 10, 0);
    rfcsb_regs[1] = bit_slice(read_smn(offset | 0x502C4), 10, 0);
    rfcsb_regs[2] = bit_slice(read_smn(offset | 0x502C8), 10, 0);
    rfcsb_regs[3] = bit_slice(read_smn(offset | 0x502CC), 10, 0);
    for (int i = 0; i < 4; i++) {
        if (rfcsb_regs[i] != 0) { d->rfcsb = rfcsb_regs[i]; break; }
    }

    /* Nanosecond conversions */
    d->trefi_ns  = to_nanoseconds(d->refi, mem_freq);
    d->trfc_ns   = to_nanoseconds(d->rfc, mem_freq);
    d->trfc2_ns  = to_nanoseconds(d->rfc2, mem_freq);
    d->trfcsb_ns = to_nanoseconds(d->rfcsb, mem_freq);
}

static void read_ddr4_timings(dram_timings_t *d)
{
    const uint32_t offset = 0; /* UMC0 */

    /*
     * DDR4 frequency ratio / flags (aligned with ZenStates-Core):
     *   - ratio: bits 0..7, scaled by /3.0, then *200 -> effective MT/s
     *   - Cmd2T: bit 10
     *   - GDM:   bit 11
     */
    uint32_t ratio_reg = read_smn(offset | 0x50200);
    float ratio = (float)bit_slice(ratio_reg, 7, 0) / 3.0f;
    float mem_freq = ratio * 200.0f;
    d->frequency_hint_mhz = mem_freq;

    uint32_t cmd2t_bit = bit_slice(ratio_reg, 10, 10);
    snprintf(d->cmd2t, sizeof(d->cmd2t), "%s", cmd2t_bit ? "2T" : "1T");
    d->gdm_enabled = bit_slice(ratio_reg, 11, 11) == 1;
    uint32_t refresh_reg = read_smn(offset | 0x5012C);
    d->power_down_enabled = bit_slice(refresh_reg, 28, 28) == 1;

    read_common_timings(offset, d);

    /* RFC - DDR4: first non-default from 2 registers */
    uint32_t trfc0 = read_smn(offset | 0x50260);
    uint32_t trfc1 = read_smn(offset | 0x50264);
    uint32_t trfc_reg = (trfc0 != trfc1) ? ((trfc0 != 0x21060138) ? trfc0 : trfc1) : trfc0;
    if (trfc_reg) {
        d->rfc  = bit_slice(trfc_reg, 10, 0);
        d->rfc2 = bit_slice(trfc_reg, 21, 11);
    }

    /* Nanosecond conversions (DDR4 path) */
    d->trefi_ns  = to_nanoseconds(d->refi, mem_freq);
    d->trfc_ns   = to_nanoseconds(d->rfc, mem_freq);
    d->trfc2_ns  = to_nanoseconds(d->rfc2, mem_freq);
}

void dram_read_timings(int codename_index, dram_timings_t *out)
{
    memset(out, 0, sizeof(*out));
    switch (codename_index) {
    case 20: case 21: /* Raphael (Ryzen 7000), Phoenix (Ryzen 7040/8040) — Zen4 DDR5 */
    case 22: case 24: /* Strix Point / Hawk Point — Zen5/Zen4c DDR5 */
    case 23:          /* Granite Ridge (Ryzen 9000) — Zen5 DDR5 */
        read_ddr5_timings(out);
        break;
    case 2:  /* Renoir (APU, DDR4/LPDDR4) */
    case 3:  /* Picasso (APU, DDR4) */
    case 7:  /* Raven Ridge (APU, DDR4) */
    case 8:  /* Raven Ridge 2 (APU, DDR4) */
    case 14: /* Cezanne (APU, DDR4) */
    case 16: /* Dali (APU, DDR4) */
    case 17: /* Lucienne (APU, DDR4/LPDDR4) */
    case 4: case 9: case 10: case 12: case 18: case 19:
        /* Matisse, Summit, Pinnacle, Vermeer, Naples, Chagall (DDR4) */
        read_ddr4_timings(out);
        break;
    default:
        break;
    }
}
