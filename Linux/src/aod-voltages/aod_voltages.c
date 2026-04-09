// SPDX-License-Identifier: GPL-2.0
/*
 * aod_voltages.c — AMD AOD (Overclocking Data) memory voltage reader
 *
 * Locates the AMD AOD SystemMemory OperationRegion by parsing ACPI SSDT tables,
 * maps it with memremap, then exposes voltage candidates via sysfs at:
 *   /sys/kernel/aod_voltages/scan       — all float-range values in DSPD
 *   /sys/kernel/aod_voltages/mem_vddio  — MemVddio (VDD)
 *   /sys/kernel/aod_voltages/mem_vddq   — MemVddq
 *   /sys/kernel/aod_voltages/mem_vpp    — MemVpp
 *   /sys/kernel/aod_voltages/full_raw   — entire AOD region (binary; read in chunks)
 *
 * Offsets for the named voltages are set via module parameters after
 * identifying them from the scan output:
 *   modprobe aod_voltages off_vddio=N off_vddq=N off_vpp=N
 *
 * Scan lists u32 values that look like millivolts. Tune the mV window with:
 *   modprobe aod_voltages scan_mv_min=400 scan_mv_max=3500
 * (defaults: 500–3000; widen max for boards with high DRAM voltage mode.)
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/acpi.h>
#include <linux/io.h>
#include <linux/memremap.h>
#include <linux/string.h>
#include <linux/fs.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("TuxTimings");
MODULE_DESCRIPTION("AMD AOD memory voltage reader");
MODULE_VERSION("0.4");

/*
 * Some vendors/AGESA revisions ship the AOD SSDT under different OEM Table IDs.
 * ZenStates-Core recognizes at least:
 *   - "AOD     "
 *   - "AMD AOD"
 *   - "CB-01   " (Lenovo)
 *
 * We do not hard-filter by OEM Table ID anymore; we scan SSDTs for the AOD
 * OperationRegion signatures directly (AODE/AODT).
 */

/*
 * Layout of the AODE OperationRegion (from SSDT9 Field definition):
 *   OUTB  1568 bits = 196 bytes  (offset    0)  — SMI output buffer
 *   AQVS    32 bits =   4 bytes  (offset  196)
 *   SCMI    32 bits =   4 bytes  (offset  200)
 *   SCMD    32 bits =   4 bytes  (offset  204)
 *   DSPD 68128 bits = 8516 bytes (offset  208)  — XMP/timing profiles
 *   RESV    96 bits =  12 bytes  (offset 8724)
 *   RMPD  1120 bits = 140 bytes  (offset 8736)
 *   WCNS  4096 bits = 512 bytes  (offset 8876)  — OC settings/voltages
 *   ...
 *
 * ZenStates-Core Granite Ridge AOD offsets are absolute from AODE start:
 *   MemVddio = 9084  (WCNS + 208)
 *   MemVddq  = 9088  (WCNS + 212)
 *   MemVpp   = 9092  (WCNS + 216)
 */
#define AOD_REGION_SIZE  0x24BB
#define WCNS_OFFSET      8876
#define WCNS_SIZE        512
/* Scan the full region to catch data in any field */
#define SCAN_START       4      /* skip first 4 bytes (status/version) */
#define SCAN_END         AOD_REGION_SIZE

/*
 * Voltages are stored as unsigned 32-bit integers in millivolts.
 * e.g. 1550 mV = 0x0000060E, 1800 mV = 0x00000708
 * Default filter range for scan; override at runtime with scan_mv_min / scan_mv_max.
 */
#define MV_SCAN_DEFAULT_MIN  500U
#define MV_SCAN_DEFAULT_MAX  3000U

/*
 * AML byte patterns for:
 *   OpRegion (AODE, SystemMemory, ...)
 *   OpRegion (AODT, SystemMemory, ...)
 *
 *   5B 80        — DefOpRegion opcode
 *   41 4F 44 45  — NameSeg 'AODE' (or 'AODT')
 *   00           — RegionSpace = SystemMemory
 */
static const u8 aode_pattern[] = { 0x5B, 0x80, 0x41, 0x4F, 0x44, 0x45, 0x00 };
static const u8 aodt_pattern[] = { 0x5B, 0x80, 0x41, 0x4F, 0x44, 0x54, 0x00 };

static void *aod_base;          /* remapped AOD region          */
static struct kobject *aod_kobj;

/* Scan window (millivolts): which u32 values count as voltage candidates in `scan`. */
static uint scan_mv_min = MV_SCAN_DEFAULT_MIN;
static uint scan_mv_max = MV_SCAN_DEFAULT_MAX;

module_param(scan_mv_min, uint, 0644);
MODULE_PARM_DESC(scan_mv_min,
    "Minimum mV for sysfs scan (default 500; lower e.g. 300 to experiment)");
module_param(scan_mv_max, uint, 0644);
MODULE_PARM_DESC(scan_mv_max,
    "Maximum mV for sysfs scan (default 3000; raise for exotic OC / BIOS ranges)");

/* Module parameters: byte offsets of each voltage in the AODE region.
 * Defaults are the Granite Ridge ZenStates-Core values (AGESA > 0xB404022).
 * Override if scan shows different offsets on your board. */
static int off_vddio     = 9084;
static int off_vddq      = 9088;
static int off_vpp       = 9092;
static int off_cpu_vddio = 9096;

module_param(off_vddio, int, 0644);
MODULE_PARM_DESC(off_vddio,     "Byte offset of MemVddio (VDD) in AOD region");
module_param(off_vddq, int, 0644);
MODULE_PARM_DESC(off_vddq,      "Byte offset of MemVddq in AOD region");
module_param(off_vpp, int, 0644);
MODULE_PARM_DESC(off_vpp,       "Byte offset of MemVpp in AOD region");
module_param(off_cpu_vddio, int, 0644);
MODULE_PARM_DESC(off_cpu_vddio, "Byte offset of CPU VDDIO in AOD region (ApuVddio)");

/* Read a u32 millivolt value from the AOD region at byte offset. */
static u32 read_mv(int offset)
{
    u32 v;

    if (!aod_base || offset < 0 || offset + 4 > AOD_REGION_SIZE)
        return 0;
    memcpy(&v, (u8 *)aod_base + offset, 4);
    return v;
}

/* sysfs: scan — list all millivolt-range integers in the AOD region with their offsets */
static ssize_t scan_show(struct kobject *kobj,
                         struct kobj_attribute *attr, char *buf)
{
    ssize_t len = 0;
    int     i;

    if (!aod_base)
        return scnprintf(buf, PAGE_SIZE, "error: AOD region not mapped\n");

    len += scnprintf(buf + len, PAGE_SIZE - len,
                     "offset  hex     field  value\n"
                     "------  ------  -----  -------\n");

    for (i = SCAN_START; i < SCAN_END - 4; i += 4) {
        u32 mv = read_mv(i);

        if (mv < scan_mv_min || mv > scan_mv_max)
            continue;

        const char *field =
            (i <  196) ? "OUTB" :
            (i <  208) ? "CTRL" :
            (i < 8724) ? "DSPD" :
            (i < 8736) ? "RESV" :
            (i < 8876) ? "RMPD" :
            (i < 9388) ? "WCNS" : "TAIL";
        len += scnprintf(buf + len, PAGE_SIZE - len,
                         "%6d  0x%04X  %-4s  %u mV (%u.%03u V)\n",
                         i, i, field, mv, mv / 1000, mv % 1000);

        if (len >= PAGE_SIZE - 64)
            break;
    }

    if (len <= 32) /* only header printed */
        len += scnprintf(buf + len, PAGE_SIZE - len,
                         "(no voltage-range values found)\n");
    return len;
}

static ssize_t show_named(struct kobject *kobj,
                           struct kobj_attribute *attr,
                           char *buf, int offset)
{
    u32 mv;

    if (!aod_base)
        return scnprintf(buf, PAGE_SIZE, "error: not mapped\n");
    if (offset < 0 || offset + 4 > AOD_REGION_SIZE)
        return scnprintf(buf, PAGE_SIZE,
                         "unset — reload with: modprobe aod_voltages off_vddio=N ...\n");

    mv = read_mv(offset);
    return scnprintf(buf, PAGE_SIZE, "%u mV (%u.%03u V)\n",
                     mv, mv / 1000, mv % 1000);
}

static ssize_t mem_vddio_show(struct kobject *k, struct kobj_attribute *a, char *b)
{ return show_named(k, a, b, off_vddio); }

static ssize_t mem_vddq_show(struct kobject *k, struct kobj_attribute *a, char *b)
{ return show_named(k, a, b, off_vddq); }

static ssize_t mem_vpp_show(struct kobject *k, struct kobj_attribute *a, char *b)
{ return show_named(k, a, b, off_vpp); }

static ssize_t cpu_vddio_show(struct kobject *k, struct kobj_attribute *a, char *b)
{ return show_named(k, a, b, off_cpu_vddio); }

/*
 * full_raw — binary read of entire mapped AOD region (supports partial reads / seek).
 * Example:  hexdump -C /sys/kernel/aod_voltages/full_raw
 */
static ssize_t full_raw_read(struct file *filp, struct kobject *kobj,
                             const struct bin_attribute *attr, char *buf,
                             loff_t off, size_t count)
{
    if (!aod_base)
        return -ENODEV;
    if (off >= AOD_REGION_SIZE)
        return 0;
    if (count > AOD_REGION_SIZE - off)
        count = AOD_REGION_SIZE - off;
    memcpy(buf, (u8 *)aod_base + off, count);
    return count;
}

static struct bin_attribute bin_attr_full_raw = {
    .attr = { .name = "full_raw", .mode = 0444 },
    .size = AOD_REGION_SIZE,
    .read = full_raw_read,
};

static struct kobj_attribute scan_attr      = __ATTR_RO(scan);
static struct kobj_attribute vddio_attr     = __ATTR_RO(mem_vddio);
static struct kobj_attribute vddq_attr      = __ATTR_RO(mem_vddq);
static struct kobj_attribute vpp_attr       = __ATTR_RO(mem_vpp);
static struct kobj_attribute cpu_vddio_attr = __ATTR_RO(cpu_vddio);

static struct attribute *aod_attrs[] = {
    &scan_attr.attr,
    &vddio_attr.attr,
    &vddq_attr.attr,
    &vpp_attr.attr,
    &cpu_vddio_attr.attr,
    NULL,
};
static struct attribute_group aod_attr_group = { .attrs = aod_attrs };

/*
 * Scan SSDT tables for an AOD OperationRegion:
 *   - AODE (common)
 *   - AODT (seen on some platforms, matching ZenStates-Core behavior)
 *
 * We look specifically for SystemMemory OperationRegions and only handle
 * constant addresses (DWordConst/QWordConst).
 */
static phys_addr_t find_aod_phys(void)
{
    struct acpi_table_header *hdr;
    acpi_status status;
    u32 idx;

    for (idx = 1; ; idx++) {
        u8  *aml;
        u32  aml_len, i;

        status = acpi_get_table("SSDT", idx, &hdr);
        if (ACPI_FAILURE(status))
            break;

        aml     = (u8 *)hdr + sizeof(*hdr);
        aml_len = hdr->length - (u32)sizeof(*hdr);

        for (i = 0; i + sizeof(aode_pattern) + 10 < aml_len; i++) {
            phys_addr_t addr = 0;
            u8 enc;

            if (memcmp(aml + i, aode_pattern, sizeof(aode_pattern)) != 0 &&
                memcmp(aml + i, aodt_pattern, sizeof(aodt_pattern)) != 0)
                continue;

            enc = aml[i + sizeof(aode_pattern)];

            if (enc == 0x0C) {
                /* DWordConst: 4-byte LE address */
                u32 tmp32;
                memcpy(&tmp32, aml + i + sizeof(aode_pattern) + 1, 4);
                addr = tmp32;
            } else if (enc == 0x0E) {
                /* QWordConst: 8-byte LE address */
                memcpy(&addr, aml + i + sizeof(aode_pattern) + 1, 8);
            } else {
                continue;
            }

            pr_info("aod_voltages: AOD region phys=0x%llx size=0x%x\n",
                    (unsigned long long)addr, AOD_REGION_SIZE);
            acpi_put_table(hdr);
            return addr;
        }

        acpi_put_table(hdr);
    }

    return 0;
}

static int __init aod_voltages_init(void)
{
    phys_addr_t phys = find_aod_phys();

    if (!phys) {
        pr_err("aod_voltages: AOD ACPI region (AODE/AODT) not found\n");
        return -ENODEV;
    }

    aod_base = memremap(phys, AOD_REGION_SIZE, MEMREMAP_WB);
    if (!aod_base) {
        pr_err("aod_voltages: memremap(0x%llx) failed\n",
               (unsigned long long)phys);
        return -ENOMEM;
    }

    aod_kobj = kobject_create_and_add("aod_voltages", kernel_kobj);
    if (!aod_kobj) {
        memunmap(aod_base);
        return -ENOMEM;
    }

    if (sysfs_create_group(aod_kobj, &aod_attr_group) != 0) {
        kobject_put(aod_kobj);
        memunmap(aod_base);
        return -ENOMEM;
    }

    if (sysfs_create_bin_file(aod_kobj, &bin_attr_full_raw) != 0) {
        sysfs_remove_group(aod_kobj, &aod_attr_group);
        kobject_put(aod_kobj);
        memunmap(aod_base);
        return -ENOMEM;
    }

    pr_info("aod_voltages: ready — offsets vddio=%d vddq=%d vpp=%d scan_mv=%u-%u\n",
            off_vddio, off_vddq, off_vpp, scan_mv_min, scan_mv_max);

    return 0;
}

static void __exit aod_voltages_exit(void)
{
    if (aod_kobj) {
        sysfs_remove_bin_file(aod_kobj, &bin_attr_full_raw);
        sysfs_remove_group(aod_kobj, &aod_attr_group);
        kobject_put(aod_kobj);
    }
    if (aod_base)
        memunmap(aod_base);

    pr_info("aod_voltages: unloaded\n");
}

module_init(aod_voltages_init);
module_exit(aod_voltages_exit);
