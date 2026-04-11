## PM table indices summary

This file documents the PM table indices used in `Linux/src/pm_table.c`. Indices are zero-based entries in the SMU PM table float array.

---

### Granite Ridge — PM table `0x00620205` (Ryzen 9000, e.g. 9950X, 613 entries)

Named entries (indices match standard Granite Ridge):

| Index | Field |
| ----- | ----- |
| 58    | `vdd_misc` (VMISC rail) |
| 71    | `fclk_mhz` (Infinity Fabric clock) |
| 75    | `uclk_mhz` (Memory controller clock) |
| 79    | `mclk_mhz` (DRAM clock) |
| 83    | `vsoc` (SoC voltage) |
| 259   | `vddg_iod` (VDDG IOD rail) |
| 261   | `vddg_ccd` (VDDG CCD rail) |
| 269   | `vddp` (VDDP rail) |
| 271   | `vcore` (Core voltage) |

Additional fields:

| Index / Range | Field |
| ------------- | ----- |
| 3             | PPT (W) |
| 11            | IOD hotspot temp (°C) |
| 220           | Socket power (W) |
| 275           | `vid` (Aggregate VID) |
| 301–316       | `core_voltages[0–15]` (Per-core voltages, C0–C15) |
| 333–348       | `core_temps_c[0–15]` (Per-core temps, °C) |
| 349–364       | `core_clocks_ghz[0–15]` (Per-core clocks, GHz) |

---

### Granite Ridge — PM table `0x00620105` (Ryzen 9000 X3D, e.g. 9850X3D, 457 entries)

Same named entries as `0x00620205`. Per-core indices differ:

| Index / Range | Field |
| ------------- | ----- |
| 3             | PPT (W) |
| 11            | IOD hotspot temp (°C) |
| 220           | Socket power (W) |
| 275           | `vid` (Aggregate VID) |
| 301–308       | `core_voltages[0–7]` (Per-core voltages, C0–C7) |
| 317–324       | `core_temps_c[0–7]` (Per-core temps, °C) |
| 325–332       | `core_clocks_ghz[0–7]` (Per-core clocks, GHz) |

---

### Raphael — PM table `0x00540104` (7000-series, e.g. 7800X3D)

| Index / Range | Field |
| ------------- | ----- |
| 3             | PPT |
| 11            | IOD hotspot temp (°C) |
| 29            | Socket power |
| 70            | `fclk_mhz` (Infinity Fabric clock) |
| 74            | `mclk_mhz` (DRAM clock) |
| 78            | `uclk_mhz` (Memory controller clock) |
| 82            | `vsoc` (SoC voltage) |
| 259           | `vddg_iod` (VDDG IOD rail) |
| 261           | `vddg_ccd` (VDDG CCD rail) |
| 269           | `vddp` (VDDP rail) |
| 271           | `vcore` (Core voltage) |
| 275           | `vid` (Aggregate VID) |
| 301–308       | `core_voltages[0–7]` (Per-core voltages, C0–C7) |
| 309–316       | `core_temps_c[0–7]` (Per-core temps, C0–C7, °C) |
| 317–324       | `core_clocks_ghz[0–7]` (Per-core clocks, GHz; 8 cores) |

### Raphael — PM table `0x00540004` (7000-series, e.g. 7950X/7900X)

Same as Raphael `0x00540104` **except**:

| Index / Range | Field |
| ------------- | ----- |
| 74            | `uclk_mhz` (Memory controller clock) |
| 78            | `mclk_mhz` (DRAM clock) |
| 309–324       | `core_voltages[0–15]` (Per-core voltages, C0–C15) |
| 325–340       | `core_temps_c[0–15]` (Per-core temps, C0–C15, °C) |
| 341–356       | `core_clocks_ghz[0–15]` (Per-core clocks, GHz; 16 cores) |

---

### Vermeer — PM table `0x00380804` (5900X/5950X 16-core, older BIOS)

| Index / Range | Field |
| ------------- | ----- |
| 1             | PPT |
| 10            | `vid` (Aggregate VID) |
| 11            | IOD hotspot temp (°C) |
| 29            | Socket power |
| 40            | `vcore` (Core voltage) |
| 44            | `vsoc` (SoC voltage) |
| 48            | `fclk_mhz` (Infinity Fabric clock) |
| 50            | `uclk_mhz` (Memory controller clock) |
| 51            | `mclk_mhz` (DRAM clock) |
| 137           | `vddp` (VDDP rail) |
| 138           | `vddg_iod` (VDDG IOD rail) |
| 139           | `vddg_ccd` (VDDG CCD rail) |
| 185–200       | `core_voltages[0–15]` (Per-core voltages, 16 cores) |
| 201–216       | `core_temps_c[0–15]` (Per-core temps, 16 cores, °C) |

### Vermeer — PM table `0x00380805` (5900X/5950X 16-core, newer BIOS)

Same as 0x380804 except:

| Index / Range | Field |
| ------------- | ----- |
| 39            | `vcore` (Core voltage) |
| 188–203       | `core_voltages[0–15]` (Per-core voltages, 16 cores) |
| 204–219       | `core_temps_c[0–15]` (Per-core temps, 16 cores, °C) |

### Vermeer — PM table `0x00380904` (5600X 6-core, older BIOS)

Same named fields as 0x380804 except `vcore` = 40.

| Index / Range | Field |
| ------------- | ----- |
| 177–182       | `core_voltages[0–5]` (Per-core voltages C0–C5) |
| 183–188       | `core_temps_c[0–5]` (Per-core temps C0–C5, °C) |

### Vermeer — PM table `0x00380905` (5600X 6-core, newer BIOS)

Same named fields as 0x380904 except `vcore` = 39.

| Index / Range | Field |
| ------------- | ----- |
| 180–185       | `core_voltages[0–5]` (Per-core voltages C0–C5) |
| 186–191       | `core_temps_c[0–5]` (Per-core temps C0–C5, °C) |

---

### Cezanne — PM table `0x00400005` (5700G APU)

| Index / Range | Field |
| ------------- | ----- |
| 5             | PPT |
| 28            | `vid` (Aggregate VID) |
| 29            | IOD hotspot temp (°C) |
| 38            | Socket power |
| 98            | `vcore` (Core voltage) |
| 102           | `vsoc` (SoC voltage) |
| 208–215       | `core_voltages[0–7]` (Per-core voltages C0–C7) |
| 216–223       | `core_temps_c[0–7]` (Per-core temps C0–C7, °C) |
| 409           | `fclk_mhz` (Infinity Fabric clock) |
| 410           | `uclk_mhz` (Memory controller clock) |
| 411           | `mclk_mhz` (DRAM clock) |
| 565           | `vddp` (VDDP rail) |

---

### Matisse — PM table `0x00240903` (3700X/3800X 8-core)

| Index / Range | Field |
| ------------- | ----- |
| 1             | PPT |
| 10            | `vid` (Aggregate VID) |
| 11            | IOD hotspot temp (°C) |
| 29            | Socket power |
| 39            | `vcore` (Core voltage) |
| 44            | `vsoc` (SoC voltage) |
| 48            | `fclk_mhz` (Infinity Fabric clock) |
| 50            | `uclk_mhz` (Memory controller clock) |
| 51            | `mclk_mhz` (DRAM clock) |
| 125           | `vddp` (VDDP rail) |
| 126           | `vddg_iod` (VDDG IOD rail) |
| 155–162       | `core_voltages[0–7]` (Per-core voltages C0–C7) |
| 163–170       | `core_temps_c[0–7]` (Per-core temps C0–C7, °C) |

### Matisse — PM table `0x00240803` (3950X 16-core)

Same as 0x240903 except `vcore` = 40.

| Index / Range | Field |
| ------------- | ----- |
| 163–178       | `core_voltages[0–15]` (Per-core voltages, 16 cores) |
| 179–194       | `core_temps_c[0–15]` (Per-core temps, 16 cores, °C) |

---

### Renoir — PM table `0x00370003` (4800U APU)

| Index / Range | Field |
| ------------- | ----- |
| 5             | PPT |
| 28            | `vid` (Aggregate VID) |
| 29            | IOD hotspot temp (°C) |
| 38            | Socket power |
| 97            | `vcore` (Core voltage) |
| 101           | `vsoc` (SoC voltage) |
| 200–207       | `core_voltages[0–7]` (Per-core voltages C0–C7) |
| 208–215       | `core_temps_c[0–7]` (Per-core temps C0–C7, °C) |
| 371           | `fclk_mhz` (Infinity Fabric clock) |
| 372           | `uclk_mhz` (Memory controller clock) |
| 373           | `mclk_mhz` (DRAM clock) |
| 527           | `vddp` (VDDP rail) |

### Renoir — PM table `0x00370005` (Renoir v2 APU)

Same fields as 0x370003 with shifted indices:

| Index / Range | Field |
| ------------- | ----- |
| 207–214       | `core_voltages[0–7]` (Per-core voltages C0–C7) |
| 215–222       | `core_temps_c[0–7]` (Per-core temps C0–C7, °C) |
| 378           | `fclk_mhz` (Infinity Fabric clock) |
| 379           | `uclk_mhz` (Memory controller clock) |
| 380           | `mclk_mhz` (DRAM clock) |
| 534           | `vddp` (VDDP rail) |

---

### Hawk Point — PM table `0x4C0009` (Ryzen 8700G, 8-core Phoenix APU)

| Index / Range | Field |
| ------------- | ----- |
| 3             | PPT (W) |
| 28            | `vid` (Aggregate VID) |
| 38            | Socket power (W) |
| 89            | `fclk_mhz` (Infinity Fabric clock) |
| 93            | `uclk_mhz` (Memory controller clock) |
| 97            | `mclk_mhz` (DRAM clock) |
| 101           | `vsoc` (SoC voltage) |
| 477           | `vddp` (VDDP rail) |

Note: Per-core voltage, temperature, and clock indices are not mapped (`core_voltage_start = -1`, etc.).

---

### Raven Ridge — PM table `0x001E0004` (2500U APU)

| Index / Range | Field |
| ------------- | ----- |
| 5             | PPT |
| 38            | Socket power |
| 57            | `vid` (Aggregate VID) |
| 60            | `vddp` (VDDP rail) |
| 61            | IOD hotspot temp / `vcore` (same index used for both) |
| 65            | `vsoc` (SoC voltage) |
| 104–107       | `core_voltages[0–3]` (Per-core voltages, 4 cores) |
| 108–111       | `core_temps_c[0–3]` (Per-core temps, 4 cores, °C) |
| 166           | `fclk_mhz` (Infinity Fabric clock) |
| 167           | `uclk_mhz` (Memory controller clock) |
| 168           | `mclk_mhz` (DRAM clock) |
