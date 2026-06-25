#ifndef UI_H
#define UI_H

#include "types.h"

#include <QMainWindow>

#include <thread>

class QLabel;
class QPushButton;
class QComboBox;
class QWidget;

/* Main window. Holds all live-updated widgets and the refresh timer. */
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    /* Tab builders */
    QWidget *build_ram_tab();
    QWidget *build_cpu_tab();
    QWidget *build_bench_tab();

    /* Actions */
    void refresh();
    void on_module_changed(int index);
    void on_bench_run();
    void on_pi_run();
    void on_debug_dump();

    /* Header */
    QLabel    *lbl_cpu_name   = nullptr;
    QLabel    *lbl_codename   = nullptr;
    QLabel    *lbl_board_info = nullptr;
    QComboBox *combo_modules  = nullptr;

    /* RAM tab — DIMM section */
    QLabel *lbl_speed = nullptr, *lbl_mclk = nullptr, *lbl_fclk = nullptr;
    QLabel *lbl_uclk = nullptr, *lbl_bclk = nullptr;
    QLabel *lbl_gdm = nullptr, *lbl_powerdown = nullptr, *lbl_spd_temp = nullptr;

    /* RAM tab — DIMM info */
    QLabel *lbl_capacity = nullptr, *lbl_manufacturer = nullptr, *lbl_part_number = nullptr;
    QLabel *lbl_serial_number = nullptr, *lbl_rank = nullptr, *lbl_cmd2t = nullptr;

    /* RAM tab — Voltages */
    QLabel *lbl_vsoc = nullptr, *lbl_vddp = nullptr, *lbl_vddg_ccd = nullptr, *lbl_vddg_iod = nullptr;
    QLabel *lbl_vdd_misc = nullptr, *lbl_mem_vdd = nullptr, *lbl_mem_vddq = nullptr;
    QLabel *lbl_cpu_vddio = nullptr, *lbl_mem_vpp = nullptr, *lbl_vcore = nullptr, *lbl_ppt = nullptr;

    /* RAM tab — Primary timings */
    QLabel *lbl_tcl = nullptr, *lbl_trcd_rd = nullptr, *lbl_trcd_wr = nullptr;
    QLabel *lbl_trp = nullptr, *lbl_tras = nullptr, *lbl_trc = nullptr;
    QLabel *lbl_trrds = nullptr, *lbl_trrdl = nullptr, *lbl_tfaw = nullptr, *lbl_twr = nullptr, *lbl_tcwl = nullptr;
    QLabel *lbl_trfc_ns = nullptr, *lbl_rfc = nullptr, *lbl_rfc2 = nullptr, *lbl_rfcsb = nullptr;

    /* RAM tab — Secondary timings */
    QLabel *lbl_rtp = nullptr, *lbl_wtrs = nullptr, *lbl_wtrl = nullptr, *lbl_rdwr = nullptr, *lbl_wrrd = nullptr;
    QLabel *lbl_rdrd_sc = nullptr, *lbl_rdrd_sd = nullptr, *lbl_rdrd_dd = nullptr;
    QLabel *lbl_wrwr_sc = nullptr, *lbl_wrwr_sd = nullptr, *lbl_wrwr_dd = nullptr;
    QLabel *lbl_refi = nullptr, *lbl_trefi_ns = nullptr, *lbl_wrpre = nullptr, *lbl_rdpre = nullptr;

    /* RAM tab — Tertiary timings */
    QLabel *lbl_rdrd_scl = nullptr, *lbl_wrwr_scl = nullptr, *lbl_cke = nullptr, *lbl_xp = nullptr;
    QLabel *lbl_trc_page = nullptr, *lbl_mod = nullptr, *lbl_mod_pda = nullptr, *lbl_mrd = nullptr, *lbl_mrd_pda = nullptr;
    QLabel *lbl_stag = nullptr, *lbl_stag_sb = nullptr, *lbl_phy_wrl = nullptr, *lbl_phy_rdl = nullptr, *lbl_phy_wrd = nullptr;

    /* RAM tab — Footer */
    QLabel *lbl_footer_type = nullptr;

    /* CPU tab — VID & per-core voltages */
    QLabel *lbl_vid = nullptr;
    QLabel *lbl_core_volt[MAX_CORES] = {};
    QLabel *lbl_core_volt_lbl[MAX_CORES] = {}; /* row label widgets for show/hide */

    /* CPU tab — Temperatures (lbl_tdie = single-line temps blob) */
    QLabel *lbl_tdie = nullptr;
    QLabel *lbl_core_temps = nullptr;

    /* CPU tab — Fans */
    QLabel *lbl_fans = nullptr;

    /* Benchmark tab — RAM */
    QPushButton *btn_bench_run = nullptr;
    QLabel *lbl_bench_status = nullptr;
    QLabel *lbl_bench_lat_l1 = nullptr;
    QLabel *lbl_bench_lat_l2 = nullptr;
    QLabel *lbl_bench_lat_l3 = nullptr;
    QLabel *lbl_bench_lat_dram = nullptr;
    QLabel *lbl_bench_bw_read = nullptr;
    QLabel *lbl_bench_bw_write = nullptr;
    QLabel *lbl_bench_bw_copy = nullptr;

    /* Benchmark tab — Pi */
    QPushButton *btn_pi_run = nullptr;
    QComboBox   *combo_pi_digits = nullptr;
    QLabel *lbl_pi_status = nullptr;
    QLabel *lbl_pi_time = nullptr;

    /* Worker threads for the benchmark/Pi runs. Kept as members (not detached)
     * so the destructor can join them before the widgets they post results to
     * are torn down, avoiding a use-after-free when the window is closed mid-run. */
    std::thread bench_thread;
    std::thread pi_thread;

    /* Data */
    system_summary_t summary = {};
    int selected_module = 0;
    bool modules_populated = false;
};

#endif
