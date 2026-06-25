#include "ui.h"
#include "backend.h"
#include "bench.h"
#include "pi_bench.h"

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <locale.h>
#include <thread>

#define APP_VERSION "v1.0.5"

/* ── CSS theme (GitHub dark) ────────────────────────────────────────── */

static const char *css_data =
    "QMainWindow, QDialog { background-color: #0D1117; }\n"
    "QLabel[cssClass=\"header-title\"] { color: #E6EDF3; font-size: 15px; font-weight: bold; }\n"
    "QLabel[cssClass=\"header-muted\"] { color: #8B949E; font-size: 11px; }\n"
    "QLabel[cssClass=\"footer-muted\"] { color: #8B949E; font-size: 11px; }\n"
    "QLabel[cssClass=\"section-title\"] { color: #C9D1D9; font-size: 12px; font-weight: bold; }\n"
    "QLabel[cssClass=\"label\"] { color: #8B949E; font-size: 11px; }\n"
    "QLabel[cssClass=\"value-highlight\"] { color: #3FB950; font-size: 11px; }\n"
    "QFrame[cssClass=\"section-box\"] { background-color: #161B22; border-radius: 6px; }\n"
    "QTabWidget::pane { background: transparent; border-top: 1px solid #30363D; }\n"
    "QTabBar::tab { color: #8B949E; background: transparent; padding: 6px 16px; font-size: 12px; }\n"
    "QTabBar::tab:selected { color: #E6EDF3; border-bottom: 2px solid #3FB950; }\n"
    "QComboBox { background-color: #161B22; color: #E6EDF3; font-size: 12px; border: 1px solid #30363D; padding: 2px 6px; }\n"
    "QComboBox QAbstractItemView { background-color: #161B22; color: #E6EDF3; selection-background-color: #30363D; }\n"
    "QPushButton { background-color: #21262D; color: #E6EDF3; border: 1px solid #30363D; border-radius: 6px; padding: 4px 12px; font-size: 11px; }\n"
    "QPushButton:hover { background-color: #30363D; }\n"
    "QPushButton:disabled { color: #555B62; }\n"
    "QPlainTextEdit { background-color: #0D1117; color: #E6EDF3; border: none; }\n";

/* ── Helpers ────────────────────────────────────────────────────────── */

static QLabel *make_label(const char *text, const char *css_class)
{
    QLabel *l = new QLabel(QString::fromUtf8(text));
    l->setProperty("cssClass", css_class);
    l->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return l;
}

static QLabel *make_value(const char *text)
{
    return make_label(text, "value-highlight");
}

/* Adds a "label : value" row to a grid. Returns the label widget (col 0). */
static QLabel *grid_row(QGridLayout *grid, int row, const char *label_text, QLabel **out_val)
{
    QLabel *lbl = make_label(label_text, "label");
    *out_val = make_value("—");
    grid->addWidget(lbl, row, 0);
    grid->addWidget(*out_val, row, 1);
    grid->setColumnStretch(1, 1);
    return lbl;
}

static void set_label_text(QLabel *label, const char *text)
{
    label->setText(QString::fromUtf8(text));
}

static void set_label_fmt(QLabel *label, const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    label->setText(QString::fromUtf8(buf));
}

/* A boxed section: rounded dark card with a vertical layout. */
static QFrame *make_section_box(QVBoxLayout **out_layout)
{
    QFrame *frame = new QFrame;
    frame->setProperty("cssClass", "section-box");
    QVBoxLayout *v = new QVBoxLayout(frame);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(4);
    *out_layout = v;
    return frame;
}

static QGridLayout *new_grid(int row_spacing, int col_spacing)
{
    QGridLayout *g = new QGridLayout();
    g->setVerticalSpacing(row_spacing);
    g->setHorizontalSpacing(col_spacing);
    g->setContentsMargins(0, 0, 0, 0);
    return g;
}

/* ── Build RAM tab ──────────────────────────────────────────────────── */

QWidget *MainWindow::build_ram_tab()
{
    QWidget *page = new QWidget;
    QVBoxLayout *vbox = new QVBoxLayout(page);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(8);

    /* ── Top row: DIMM | DIMM info | Voltages ── */
    QHBoxLayout *top = new QHBoxLayout();
    top->setSpacing(4);

    /* DIMM speeds */
    {
        QVBoxLayout *box;
        QFrame *dimm_box = make_section_box(&box);
        box->addWidget(make_label("DIMM", "section-title"));

        QGridLayout *g = new_grid(4, 8);
        int r = 0;
        grid_row(g, r++, "Speed:", &lbl_speed);
        grid_row(g, r++, "MCLK:", &lbl_mclk);
        grid_row(g, r++, "FCLK:", &lbl_fclk);
        grid_row(g, r++, "UCLK:", &lbl_uclk);
        grid_row(g, r++, "BCLK:", &lbl_bclk);
        box->addLayout(g);

        /* GDM / PowerDown / Temp — labels row then values row */
        QGridLayout *g2 = new_grid(2, 12);
        g2->addWidget(make_label("GDM", "label"),       0, 0);
        g2->addWidget(make_label("PowerDown", "label"), 0, 1);
        g2->addWidget(make_label("Temp", "label"),      0, 2);
        lbl_gdm       = make_value("—");
        lbl_powerdown = make_value("—");
        lbl_spd_temp  = make_value("—");
        g2->addWidget(lbl_gdm,       1, 0);
        g2->addWidget(lbl_powerdown, 1, 1);
        g2->addWidget(lbl_spd_temp,  1, 2);
        box->addLayout(g2);
        box->addStretch(1);
        top->addWidget(dimm_box, 1);
    }

    /* DIMM info */
    {
        QVBoxLayout *box;
        QFrame *info_box = make_section_box(&box);
        box->addWidget(make_label("DIMM", "section-title"));

        QGridLayout *g = new_grid(2, 8);
        int r = 0;
        grid_row(g, r++, "Capacity:", &lbl_capacity);
        grid_row(g, r++, "Manufacturer:", &lbl_manufacturer);
        grid_row(g, r++, "Part Number:", &lbl_part_number);
        grid_row(g, r++, "Serial:", &lbl_serial_number);
        grid_row(g, r++, "Rank:", &lbl_rank);
        grid_row(g, r++, "Cmd2T:", &lbl_cmd2t);
        box->addLayout(g);
        box->addStretch(1);
        top->addWidget(info_box, 1);
    }

    /* Voltages */
    {
        QVBoxLayout *box;
        QFrame *volt_box = make_section_box(&box);
        box->addWidget(make_label("Voltages", "section-title"));

        QGridLayout *g = new_grid(2, 8);
        int r = 0;
        grid_row(g, r++, "VSOC", &lbl_vsoc);
        grid_row(g, r++, "CLDO VDDP", &lbl_vddp);
        grid_row(g, r++, "VDDG CCD", &lbl_vddg_ccd);
        grid_row(g, r++, "VDDG IOD", &lbl_vddg_iod);
        grid_row(g, r++, "VDD MISC", &lbl_vdd_misc);
        grid_row(g, r++, "MEM VDD", &lbl_mem_vdd);
        grid_row(g, r++, "MEM VDDQ", &lbl_mem_vddq);
        grid_row(g, r++, "CPU VDDIO", &lbl_cpu_vddio);
        grid_row(g, r++, "MEM VPP", &lbl_mem_vpp);
        grid_row(g, r++, "VCORE", &lbl_vcore);
        grid_row(g, r++, "PPT", &lbl_ppt);
        box->addLayout(g);
        box->addStretch(1);
        top->addWidget(volt_box, 1);
    }
    vbox->addLayout(top);

    /* ── Timing columns: Primary | Secondary | Tertiary ── */
    QHBoxLayout *mid = new QHBoxLayout();
    mid->setSpacing(4);

    /* Primary */
    {
        QVBoxLayout *box;
        QFrame *prim_box = make_section_box(&box);
        box->addWidget(make_label("Primary Timings", "section-title"));
        QGridLayout *g = new_grid(2, 8);
        int r = 0;
        grid_row(g, r++, "tCL", &lbl_tcl);
        grid_row(g, r++, "tRCDRD", &lbl_trcd_rd);
        grid_row(g, r++, "tRCDWR", &lbl_trcd_wr);
        grid_row(g, r++, "tRP", &lbl_trp);
        grid_row(g, r++, "tRAS", &lbl_tras);
        grid_row(g, r++, "tRC", &lbl_trc);
        grid_row(g, r++, "tRRDS", &lbl_trrds);
        grid_row(g, r++, "tRRDL", &lbl_trrdl);
        grid_row(g, r++, "tFAW", &lbl_tfaw);
        grid_row(g, r++, "tWR", &lbl_twr);
        grid_row(g, r++, "tCWL", &lbl_tcwl);
        grid_row(g, r++, "tRFC (ns)", &lbl_trfc_ns);
        grid_row(g, r++, "tRFC", &lbl_rfc);
        grid_row(g, r++, "tRFC2", &lbl_rfc2);
        grid_row(g, r++, "tRFCsb", &lbl_rfcsb);
        box->addLayout(g);
        box->addStretch(1);
        mid->addWidget(prim_box, 1);
    }

    /* Secondary */
    {
        QVBoxLayout *box;
        QFrame *sec_box = make_section_box(&box);
        box->addWidget(make_label("Secondary Timings", "section-title"));
        QGridLayout *g = new_grid(2, 8);
        int r = 0;
        grid_row(g, r++, "tRTP", &lbl_rtp);
        grid_row(g, r++, "tWTRS", &lbl_wtrs);
        grid_row(g, r++, "tWTRL", &lbl_wtrl);
        grid_row(g, r++, "tRDWR", &lbl_rdwr);
        grid_row(g, r++, "tWRRD", &lbl_wrrd);
        grid_row(g, r++, "tRDRDSC", &lbl_rdrd_sc);
        grid_row(g, r++, "tRDRDSD", &lbl_rdrd_sd);
        grid_row(g, r++, "tRDRDDD", &lbl_rdrd_dd);
        grid_row(g, r++, "tWRWRSC", &lbl_wrwr_sc);
        grid_row(g, r++, "tWRWRSD", &lbl_wrwr_sd);
        grid_row(g, r++, "tWRWRDD", &lbl_wrwr_dd);
        grid_row(g, r++, "tREFI", &lbl_refi);
        grid_row(g, r++, "tREFI (ns)", &lbl_trefi_ns);
        grid_row(g, r++, "tWRPRE", &lbl_wrpre);
        grid_row(g, r++, "tRDPRE", &lbl_rdpre);
        box->addLayout(g);
        box->addStretch(1);
        mid->addWidget(sec_box, 1);
    }

    /* Tertiary */
    {
        QVBoxLayout *box;
        QFrame *tert_box = make_section_box(&box);
        box->addWidget(make_label("Tertiary Timings", "section-title"));
        QGridLayout *g = new_grid(2, 8);
        int r = 0;
        grid_row(g, r++, "tRDRDSCL", &lbl_rdrd_scl);
        grid_row(g, r++, "tWRWRSCL", &lbl_wrwr_scl);
        grid_row(g, r++, "tCKE", &lbl_cke);
        grid_row(g, r++, "tXP", &lbl_xp);
        grid_row(g, r++, "tTRCPAGE", &lbl_trc_page);
        grid_row(g, r++, "tMOD", &lbl_mod);
        grid_row(g, r++, "tMODPDA", &lbl_mod_pda);
        grid_row(g, r++, "tMRD", &lbl_mrd);
        grid_row(g, r++, "tMRDPDA", &lbl_mrd_pda);
        grid_row(g, r++, "tSTAG", &lbl_stag);
        grid_row(g, r++, "tSTAGsb", &lbl_stag_sb);
        grid_row(g, r++, "tPHYWRL", &lbl_phy_wrl);
        grid_row(g, r++, "tPHYRDL", &lbl_phy_rdl);
        grid_row(g, r++, "tPHYWRD", &lbl_phy_wrd);
        box->addLayout(g);
        box->addStretch(1);
        mid->addWidget(tert_box, 1);
    }
    vbox->addLayout(mid, 1);

    /* Footer */
    QHBoxLayout *footer = new QHBoxLayout();
    QLabel *footer_text = make_label("DRAM timings & MCLK/UCLK: SMN. Voltages & FCLK: PM table.", "footer-muted");
    lbl_footer_type = make_label("DDR5", "value-highlight");
    footer->addWidget(footer_text, 1);
    footer->addWidget(lbl_footer_type);
    vbox->addLayout(footer);

    return page;
}

/* ── Build CPU tab ──────────────────────────────────────────────────── */

QWidget *MainWindow::build_cpu_tab()
{
    QWidget *page = new QWidget;
    QVBoxLayout *vbox = new QVBoxLayout(page);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(8);

    QHBoxLayout *top = new QHBoxLayout();
    top->setSpacing(4);

    /* Left: VID & per-core voltages */
    {
        QVBoxLayout *box;
        QFrame *volt_box = make_section_box(&box);
        box->addWidget(make_label("VID & Core Voltages", "section-title"));
        QGridLayout *g = new_grid(2, 8);
        int r = 0;
        grid_row(g, r++, "VID:", &lbl_vid);
        for (int i = 0; i < MAX_CORES; i++) {
            char lbl[8];
            snprintf(lbl, sizeof(lbl), "C%d:", i);
            lbl_core_volt_lbl[i] = grid_row(g, r++, lbl, &lbl_core_volt[i]);
            lbl_core_volt[i]->setVisible(false);
            lbl_core_volt_lbl[i]->setVisible(false);
        }
        box->addLayout(g);
        box->addStretch(1);
        top->addWidget(volt_box, 1);
    }

    /* Right: Temperatures & Fans */
    {
        QVBoxLayout *box;
        QFrame *temp_box = make_section_box(&box);
        box->addWidget(make_label("Temp & Fans", "section-title"));

        QGridLayout *g = new_grid(2, 8);
        grid_row(g, 0, "Fans:", &lbl_fans);
        lbl_fans->setWordWrap(true);
        box->addLayout(g);

        box->addWidget(make_label("Temps:", "label"));
        lbl_tdie = make_value("—");
        lbl_tdie->setWordWrap(true);
        box->addWidget(lbl_tdie);

        box->addWidget(make_label("Core temps / load / freq:", "label"));
        lbl_core_temps = make_value("—");
        lbl_core_temps->setWordWrap(true);
        box->addWidget(lbl_core_temps);
        box->addStretch(1);
        top->addWidget(temp_box, 1);
    }
    vbox->addLayout(top);
    vbox->addStretch(1);

    return page;
}

/* ── Benchmark tab ──────────────────────────────────────────────────── */

QWidget *MainWindow::build_bench_tab()
{
    QWidget *page = new QWidget;
    QVBoxLayout *vbox = new QVBoxLayout(page);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(8);

    /* Run button row */
    QHBoxLayout *btn_row = new QHBoxLayout();
    btn_row->setSpacing(8);
    btn_bench_run = new QPushButton("Run Benchmark");
    connect(btn_bench_run, &QPushButton::clicked, this, &MainWindow::on_bench_run);
    lbl_bench_status = make_label("Ready", "header-muted");
    btn_row->addWidget(btn_bench_run);
    btn_row->addWidget(lbl_bench_status);
    btn_row->addStretch(1);
    vbox->addLayout(btn_row);

    /* Two-column row: Latency | Bandwidth */
    QHBoxLayout *cols = new QHBoxLayout();
    cols->setSpacing(4);

    /* Latency section */
    {
        QVBoxLayout *box;
        QFrame *lat_box = make_section_box(&box);
        box->addWidget(make_label("Cache & DRAM Latency", "section-title"));
        QGridLayout *g = new_grid(4, 8);
        int r = 0;
        grid_row(g, r++, "L1 Cache:", &lbl_bench_lat_l1);
        grid_row(g, r++, "L2 Cache:", &lbl_bench_lat_l2);
        grid_row(g, r++, "L3 Cache:", &lbl_bench_lat_l3);
        grid_row(g, r++, "DRAM:", &lbl_bench_lat_dram);
        box->addLayout(g);
        box->addStretch(1);
        cols->addWidget(lat_box, 1);
    }

    /* Bandwidth section */
    {
        QVBoxLayout *box;
        QFrame *bw_box = make_section_box(&box);
        box->addWidget(make_label("DRAM Bandwidth", "section-title"));
        QGridLayout *g = new_grid(4, 8);
        int r = 0;
        grid_row(g, r++, "Read:", &lbl_bench_bw_read);
        grid_row(g, r++, "Write:", &lbl_bench_bw_write);
        grid_row(g, r++, "Copy:", &lbl_bench_bw_copy);
        box->addLayout(g);
        box->addStretch(1);
        cols->addWidget(bw_box, 1);
    }
    vbox->addLayout(cols);

    /* ── Pi benchmark section ─────────────────────────────────────────── */
    {
        QVBoxLayout *box;
        QFrame *pi_box = make_section_box(&box);
        box->addWidget(make_label("Pi Computation", "section-title"));

        /* Controls: [digit dropdown] [Run Pi button] [status] */
        QHBoxLayout *pi_ctrl = new QHBoxLayout();
        pi_ctrl->setSpacing(8);

        combo_pi_digits = new QComboBox;
        combo_pi_digits->addItems({"1 M digits", "10 M digits", "100 M digits", "200 M digits"});
        combo_pi_digits->setCurrentIndex(0); /* default: 1 M */

        btn_pi_run = new QPushButton("Run Pi");
        connect(btn_pi_run, &QPushButton::clicked, this, &MainWindow::on_pi_run);

        lbl_pi_status = make_label("Ready", "header-muted");

        pi_ctrl->addWidget(combo_pi_digits);
        pi_ctrl->addWidget(btn_pi_run);
        pi_ctrl->addWidget(lbl_pi_status);
        pi_ctrl->addStretch(1);
        box->addLayout(pi_ctrl);

        /* Results grid */
        QGridLayout *pi_grid = new_grid(4, 8);
        grid_row(pi_grid, 0, "Time:", &lbl_pi_time);
        box->addLayout(pi_grid);
        box->addStretch(1);
        vbox->addWidget(pi_box);
    }

    vbox->addStretch(1);
    return page;
}

/* ── Benchmark / Pi actions ─────────────────────────────────────────── */

void MainWindow::on_bench_run()
{
    btn_bench_run->setEnabled(false);
    set_label_text(lbl_bench_status, "Running…");

    if (bench_thread.joinable())
        bench_thread.join();
    bench_thread = std::thread([this]() {
        bench_results_t r;
        memset(&r, 0, sizeof(r));
        bench_run(&r);
        QMetaObject::invokeMethod(this, [this, r]() {
            set_label_fmt(lbl_bench_lat_l1,   "%.1f ns",  r.lat_l1_ns);
            set_label_fmt(lbl_bench_lat_l2,   "%.1f ns",  r.lat_l2_ns);
            set_label_fmt(lbl_bench_lat_l3,   "%.1f ns",  r.lat_l3_ns);
            set_label_fmt(lbl_bench_lat_dram, "%.1f ns",  r.lat_dram_ns);
            set_label_fmt(lbl_bench_bw_read,  "%.0f MB/s", r.bw_read_mbs);
            set_label_fmt(lbl_bench_bw_write, "%.0f MB/s", r.bw_write_mbs);
            set_label_fmt(lbl_bench_bw_copy,  "%.0f MB/s", r.bw_copy_mbs);
            set_label_text(lbl_bench_status, "Done");
            btn_bench_run->setEnabled(true);
        }, Qt::QueuedConnection);
    });
}

void MainWindow::on_pi_run()
{
    btn_pi_run->setEnabled(false);
    combo_pi_digits->setEnabled(false);
    set_label_text(lbl_pi_status, "Running…");

    static const int digit_counts[] = { 1000000, 10000000, 100000000, 200000000 };
    int sel = combo_pi_digits->currentIndex();
    if (sel < 0 || sel >= (int)(sizeof(digit_counts) / sizeof(digit_counts[0])))
        sel = 0;
    int n_digits = digit_counts[sel];

    if (pi_thread.joinable())
        pi_thread.join();
    pi_thread = std::thread([this, n_digits]() {
        pi_results_t r;
        memset(&r, 0, sizeof(r));
        pi_bench_run(n_digits, &r);
        QMetaObject::invokeMethod(this, [this, r]() {
            if (r.time_sec < 1.0)
                set_label_fmt(lbl_pi_time, "%.1f ms", r.time_sec * 1000.0);
            else
                set_label_fmt(lbl_pi_time, "%.3f s", r.time_sec);
            set_label_text(lbl_pi_status, "Done");
            btn_pi_run->setEnabled(true);
            combo_pi_digits->setEnabled(true);
        }, Qt::QueuedConnection);
    });
}

/* ── Debug dump dialog ──────────────────────────────────────────────── */

void MainWindow::on_debug_dump()
{
    char *text = backend_read_debug_dump();
    if (!text) {
        text = strdup("(failed to read debug data)");
        if (!text) return;
    }

    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle("Debug Dump");
    dlg->resize(700, 600);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(0, 0, 0, 0);

    QPlainTextEdit *tv = new QPlainTextEdit;
    tv->setReadOnly(true);
    QFont mono("monospace");
    mono.setStyleHint(QFont::Monospace);
    tv->setFont(mono);
    tv->setPlainText(QString::fromUtf8(text));
    free(text);

    layout->addWidget(tv);
    dlg->show();
}

/* ── Module dropdown selection changed ──────────────────────────────── */

void MainWindow::on_module_changed(int index)
{
    if (index >= 0)
        selected_module = index;
}

/* ── Refresh data → UI ──────────────────────────────────────────────── */

void MainWindow::refresh()
{
    setlocale(LC_NUMERIC, "C");

    backend_read_summary(&summary);
    const system_summary_t *s = &summary;
    const smu_metrics_t *m = &s->metrics;
    const dram_timings_t *d = &s->dram;

    /* Header */
    set_label_text(lbl_cpu_name, s->cpu.processor_name[0] ? s->cpu.processor_name : s->cpu.name);
    set_label_fmt(lbl_codename, "%s  ·  SMU %s  ·  %s",
                  s->cpu.codename, s->cpu.smu_version, s->cpu.pm_table_version);
    set_label_text(lbl_board_info, s->board.display_line);

    /* Module dropdown — populate once */
    if (s->module_count > 0 && !modules_populated) {
        combo_modules->blockSignals(true);
        combo_modules->clear();
        for (int i = 0; i < s->module_count; i++)
            combo_modules->addItem(QString::fromUtf8(s->modules[i].slot_display));
        combo_modules->setCurrentIndex(0);
        combo_modules->blockSignals(false);
        selected_module = 0;
        modules_populated = true;
    }

    int mi = selected_module;

    /* DIMM speeds */
    if (s->memory.frequency > 0.0f)
        set_label_fmt(lbl_speed, "%.0f MT/s", s->memory.frequency);
    else
        set_label_text(lbl_speed, "—");
    set_label_fmt(lbl_mclk, "%.0f MHz", m->mclk_mhz);
    set_label_fmt(lbl_fclk, "%.0f MHz", m->fclk_mhz);
    set_label_fmt(lbl_uclk, "%.0f MHz", m->uclk_mhz);
    set_label_fmt(lbl_bclk, "%.1f MHz", m->bclk_mhz);
    set_label_text(lbl_gdm, d->gdm_enabled ? "True" : "False");
    set_label_text(lbl_powerdown, d->power_down_enabled ? "True" : "False");

    /* SPD temp */
    if (mi >= 0 && mi < m->spd_temps_count)
        set_label_fmt(lbl_spd_temp, "%.1f °C", m->spd_temps_c[mi]);
    else
        set_label_text(lbl_spd_temp, "—");

    /* DIMM info */
    if (mi >= 0 && mi < s->module_count) {
        const memory_module_t *mod = &s->modules[mi];
        set_label_text(lbl_capacity, mod->capacity_display);
        set_label_text(lbl_manufacturer, mod->manufacturer[0] ? mod->manufacturer : "—");
        set_label_text(lbl_part_number, mod->part_number[0] ? mod->part_number : "—");
        set_label_text(lbl_serial_number, mod->serial_number[0] ? mod->serial_number : "—");
        const char *rank_str = mod->rank == RANK_QR ? "QR" : mod->rank == RANK_DR ? "DR" : "SR";
        set_label_text(lbl_rank, rank_str);
    }
    set_label_text(lbl_cmd2t, d->cmd2t[0] ? d->cmd2t : "—");

/* Helper: show voltage or "—" if unavailable (0) */
#define SET_VOLT(lbl, val) \
    ((val) > 0 ? set_label_fmt((lbl), "%.4fV", (double)(val)) : set_label_text((lbl), "—"))

    /* Voltages */
    SET_VOLT(lbl_vsoc,     m->vsoc);
    SET_VOLT(lbl_vddp,     m->vddp);
    SET_VOLT(lbl_vddg_ccd, m->vddg_ccd);
    SET_VOLT(lbl_vddg_iod, m->vddg_iod);
    SET_VOLT(lbl_vdd_misc, m->vdd_misc);
    SET_VOLT(lbl_mem_vdd,  m->mem_vdd);
    SET_VOLT(lbl_mem_vddq, m->mem_vddq);
    SET_VOLT(lbl_cpu_vddio, m->cpu_vddio);
    SET_VOLT(lbl_mem_vpp,  m->mem_vpp);
    SET_VOLT(lbl_vcore,    m->vcore);
    set_label_fmt(lbl_ppt, "%.1fW", m->ppt_w);

    /* Primary timings */
    set_label_fmt(lbl_tcl, "%u", d->tcl);
    set_label_fmt(lbl_trcd_rd, "%u", d->trcd_rd);
    set_label_fmt(lbl_trcd_wr, "%u", d->trcd_wr);
    set_label_fmt(lbl_trp, "%u", d->trp);
    set_label_fmt(lbl_tras, "%u", d->tras);
    set_label_fmt(lbl_trc, "%u", d->trc);
    set_label_fmt(lbl_trrds, "%u", d->trrds);
    set_label_fmt(lbl_trrdl, "%u", d->trrdl);
    set_label_fmt(lbl_tfaw, "%u", d->tfaw);
    set_label_fmt(lbl_twr, "%u", d->twr);
    set_label_fmt(lbl_tcwl, "%u", d->tcwl);
    set_label_fmt(lbl_trfc_ns, "%.0f", d->trfc_ns);
    set_label_fmt(lbl_rfc, "%u", d->rfc);
    set_label_fmt(lbl_rfc2, "%u", d->rfc2);
    set_label_fmt(lbl_rfcsb, "%u", d->rfcsb);

    /* Secondary timings */
    set_label_fmt(lbl_rtp, "%u", d->rtp);
    set_label_fmt(lbl_wtrs, "%u", d->wtrs);
    set_label_fmt(lbl_wtrl, "%u", d->wtrl);
    set_label_fmt(lbl_rdwr, "%u", d->rdwr);
    set_label_fmt(lbl_wrrd, "%u", d->wrrd);
    set_label_fmt(lbl_rdrd_sc, "%u", d->rdrd_sc);
    set_label_fmt(lbl_rdrd_sd, "%u", d->rdrd_sd);
    set_label_fmt(lbl_rdrd_dd, "%u", d->rdrd_dd);
    set_label_fmt(lbl_wrwr_sc, "%u", d->wrwr_sc);
    set_label_fmt(lbl_wrwr_sd, "%u", d->wrwr_sd);
    set_label_fmt(lbl_wrwr_dd, "%u", d->wrwr_dd);
    set_label_fmt(lbl_refi, "%u", d->refi);
    set_label_fmt(lbl_trefi_ns, "%.0f", d->trefi_ns);
    set_label_fmt(lbl_wrpre, "%u", d->wrpre);
    set_label_fmt(lbl_rdpre, "%u", d->rdpre);

    /* Tertiary timings */
    set_label_fmt(lbl_rdrd_scl, "%u", d->rdrd_scl);
    set_label_fmt(lbl_wrwr_scl, "%u", d->wrwr_scl);
    set_label_fmt(lbl_cke, "%u", d->cke);
    set_label_fmt(lbl_xp, "%u", d->xp);
    set_label_fmt(lbl_trc_page, "%u", d->trc_page);
    set_label_fmt(lbl_mod, "%u", d->mod);
    set_label_fmt(lbl_mod_pda, "%u", d->mod_pda);
    set_label_fmt(lbl_mrd, "%u", d->mrd);
    set_label_fmt(lbl_mrd_pda, "%u", d->mrd_pda);
    set_label_fmt(lbl_stag, "%u", d->stag);
    set_label_fmt(lbl_stag_sb, "%u", d->stag_sb);
    set_label_fmt(lbl_phy_wrl, "%u", d->phy_wrl);
    set_label_fmt(lbl_phy_rdl, "%u", d->phy_rdl);
    set_label_fmt(lbl_phy_wrd, "%u", d->phy_wrd);

    /* Footer mem type */
    const char *mem_str = s->memory.type == MEM_DDR5 ? "DDR5" :
                          s->memory.type == MEM_DDR4 ? "DDR4" : "—";
    set_label_text(lbl_footer_type, mem_str);

    /* CPU tab — VID & per-core voltages */
    SET_VOLT(lbl_vid, m->vid);
    {
        int count = m->core_voltages_count < MAX_CORES ? m->core_voltages_count : MAX_CORES;
        for (int i = 0; i < MAX_CORES; i++) {
            bool vis = (i < count);
            lbl_core_volt[i]->setVisible(vis);
            lbl_core_volt_lbl[i]->setVisible(vis);
            if (vis) SET_VOLT(lbl_core_volt[i], m->core_voltages[i]);
        }
    }

    /* CPU tab — Temperatures (single blob row) */
    {
        char tbuf[256];
        int  toff = 0;
        if (m->has_tdie)        toff += snprintf(tbuf+toff, sizeof(tbuf)-toff, "Tdie: %.1f\xC2\xB0""C  ", m->tdie_c);
        if (m->has_tctl)        toff += snprintf(tbuf+toff, sizeof(tbuf)-toff, "Tctl: %.1f\xC2\xB0""C  ", m->tctl_c);
        if (m->has_tccd1)       toff += snprintf(tbuf+toff, sizeof(tbuf)-toff, "Tccd1: %.1f\xC2\xB0""C  ", m->tccd1_c);
        if (m->has_tccd2)       toff += snprintf(tbuf+toff, sizeof(tbuf)-toff, "Tccd2: %.1f\xC2\xB0""C  ", m->tccd2_c);
        if (m->has_iod_hotspot) toff += snprintf(tbuf+toff, sizeof(tbuf)-toff, "IOD: %.1f\xC2\xB0""C", m->iod_hotspot_c);
        if (toff == 0) snprintf(tbuf, sizeof(tbuf), "—");
        set_label_text(lbl_tdie, tbuf);
    }

    {
        int count = m->core_temps_count;
        if (m->core_usage_count > count) count = m->core_usage_count;
        if (m->core_freq_count  > count) count = m->core_freq_count;
        if (count > MAX_CORES) count = MAX_CORES;
        if (count == 0) {
            set_label_text(lbl_core_temps, "—");
        } else {
            char buf[2048];
            int  off = 0;
            for (int i = 0; i < count; i++) {
                float temp  = (i < m->core_temps_count) ? m->core_temps_c[i]  : 0;
                float usage = (i < m->core_usage_count) ? m->core_usage_pct[i]: 0;
                float freq  = (i < m->core_freq_count)  ? m->core_freq_mhz[i] : 0;
                off += snprintf(buf+off, sizeof(buf)-off,
                                "C%d: %.1f\xC2\xB0""C  %.0f%%  %.0fMHz\n",
                                i, temp, usage, freq);
            }
            /* trim trailing newline */
            if (off > 0 && buf[off-1] == '\n') buf[off-1] = '\0';
            set_label_text(lbl_core_temps, buf);
        }
    }

    /* Fans */
    {
        char buf[1024];
        int off = 0;
        for (int i = 0; i < s->fan_count; i++)
            off += snprintf(buf + off, sizeof(buf) - off, "%s: %d RPM  ", s->fans[i].label, s->fans[i].rpm);
        if (off == 0) snprintf(buf, sizeof(buf), "—");
        set_label_text(lbl_fans, buf);
    }

#undef SET_VOLT
}

/* ── Construction ───────────────────────────────────────────────────── */

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    /* Qt resets LC_NUMERIC on QApplication construction; force C locale
     * for dot decimal separators (backend output is parsed as floats). */
    setlocale(LC_NUMERIC, "C");

    qApp->setStyleSheet(QString::fromUtf8(css_data));

    setWindowTitle("QTuxTimings");
    setWindowIcon(QIcon::fromTheme("qtuxtimings"));
    resize(420, 820);
    setMinimumSize(380, 720);

    QWidget *central = new QWidget;
    setCentralWidget(central);
    QVBoxLayout *main_box = new QVBoxLayout(central);
    main_box->setContentsMargins(10, 10, 10, 14);
    main_box->setSpacing(8);

    /* Header */
    QVBoxLayout *header = new QVBoxLayout();
    header->setSpacing(2);

    QHBoxLayout *header_top = new QHBoxLayout();
    header_top->setSpacing(8);
    lbl_cpu_name = make_label("QTuxTimings", "header-title");
    header_top->addWidget(lbl_cpu_name, 1);

    combo_modules = new QComboBox;
    combo_modules->addItem("(detecting...)");
    combo_modules->setMinimumWidth(180);
    connect(combo_modules, &QComboBox::currentIndexChanged, this, &MainWindow::on_module_changed);
    header_top->addWidget(combo_modules);

    QPushButton *btn_debug = new QPushButton("Debug");
    connect(btn_debug, &QPushButton::clicked, this, &MainWindow::on_debug_dump);
    header_top->addWidget(btn_debug);
    header->addLayout(header_top);

    lbl_codename = make_label("", "header-muted");
    header->addWidget(lbl_codename);

    /* Board info row: board string left, version right */
    QHBoxLayout *board_row = new QHBoxLayout();
    lbl_board_info = make_label("", "footer-muted");
    QLabel *lbl_version = make_label(APP_VERSION, "footer-muted");
    lbl_version->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    board_row->addWidget(lbl_board_info, 1);
    board_row->addWidget(lbl_version);
    header->addLayout(board_row);
    main_box->addLayout(header);

    /* Notebook (tabs) */
    QTabWidget *tabs = new QTabWidget;
    tabs->addTab(build_ram_tab(), "RAM");
    tabs->addTab(build_cpu_tab(), "CPU");
    tabs->addTab(build_bench_tab(), "Benchmark");
    main_box->addWidget(tabs, 1);

    /* Initial data load — populates the header/board labels. */
    refresh();

    /* Grow the window once to fit the header/board text now that the labels
     * carry real content (Qt sizes to the placeholder text otherwise). Grow
     * both dimensions so larger fonts or HiDPI scaling can't clip the bottom,
     * and clamp to the available screen area so a long DMI/board string can't
     * push the window past the monitor. */
    if (QLayout *lay = layout())
        lay->activate();
    const QSize hint = sizeHint();
    QSize target(qMax(width(), hint.width()), qMax(height(), hint.height()));
    if (QScreen *scr = screen())
        target = target.boundedTo(scr->availableGeometry().size());
    resize(target);

    /* 1-second refresh timer */
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::refresh);
    timer->start(1000);
}

MainWindow::~MainWindow()
{
    /* Join any in-flight worker thread before the widgets it writes back to are
     * destroyed, so a benchmark/Pi run still running at window close cannot post
     * results to a freed MainWindow. */
    if (bench_thread.joinable())
        bench_thread.join();
    if (pi_thread.joinable())
        pi_thread.join();
}
