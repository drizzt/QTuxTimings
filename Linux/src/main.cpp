#include "ui.h"
#include "backend.h"

#include <QApplication>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <locale.h>
#include <signal.h>

static void on_signal(int sig)
{
    (void)sig;
    backend_cleanup();
    _exit(0);
}

/* ── Restore environment variables from --env-VAR=VALUE args ────────── */

static int restore_env(int *argc, char ***argv)
{
    int new_argc = 0;
    char **new_argv = (char **)malloc(*argc * sizeof(char *));
    if (!new_argv) return -1;

    for (int i = 0; i < *argc; i++) {
        if (strncmp((*argv)[i], "--env-", 6) == 0) {
            const char *eq = strchr((*argv)[i] + 6, '=');
            if (eq && eq > (*argv)[i] + 6) {
                int namelen = (int)(eq - ((*argv)[i] + 6));
                char name[256];
                if (namelen < (int)sizeof(name)) {
                    memcpy(name, (*argv)[i] + 6, namelen);
                    name[namelen] = '\0';
                    setenv(name, eq + 1, 1);
                }
            }
        } else {
            new_argv[new_argc++] = (*argv)[i];
        }
    }

    *argc = new_argc;
    *argv = new_argv;
    return 0;
}

/* ── Elevate to root via pkexec if not already root ─────────────────── */

static void elevate_if_necessary(int argc, char **argv)
{
    if (geteuid() == 0) return;

    /* Build argv for pkexec: pkexec <self> --env-VAR=VALUE ... [original args] */
    const char *env_vars[] = {
        "DISPLAY", "WAYLAND_DISPLAY", "XDG_RUNTIME_DIR", "XAUTHORITY",
        "DBUS_SESSION_BUS_ADDRESS", "XDG_CONFIG_HOME", "HOME", NULL
    };

    /* Count env args */
    int env_count = 0;
    for (int i = 0; env_vars[i]; i++) {
        if (getenv(env_vars[i])) env_count++;
    }

    /* +3: pkexec, self, NULL; +argc for original args;
     * +1 extra for the conditional --env-QT_QPA_PLATFORM=wayland entry */
    int total = 2 + env_count + 1 + (argc - 1) + 1;
    char **new_argv = (char **)malloc(total * sizeof(char *));
    if (!new_argv) return;

    int n = 0;
    new_argv[n++] = (char *)"pkexec";

    /* Get self path */
    char self[4096];
    ssize_t len = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (len <= 0) { free(new_argv); return; }
    self[len] = '\0';

    /* Prefer installed path for polkit policy match */
    const char *installed = "/opt/QTuxTimings/bin/qtuxtimings";
    if (access(installed, X_OK) == 0)
        new_argv[n++] = (char *)installed;
    else
        new_argv[n++] = self;

    /* Add env forwarding args. One buffer per env_vars entry (the trailing
     * NULL slot doubles as room for the conditional QT_QPA_PLATFORM hint). */
    static const int env_buf_count = sizeof(env_vars) / sizeof(env_vars[0]);
    static char env_bufs[env_buf_count][512];
    int ei = 0;
    for (int i = 0; env_vars[i] && ei < env_buf_count; i++) {
        const char *val = getenv(env_vars[i]);
        if (val) {
            snprintf(env_bufs[ei], sizeof(env_bufs[ei]), "--env-%s=%s", env_vars[i], val);
            new_argv[n++] = env_bufs[ei];
            ei++;
        }
    }

    /* If on Wayland, forward that hint so Qt selects the Wayland platform */
    if (getenv("WAYLAND_DISPLAY") && ei < env_buf_count) {
        snprintf(env_bufs[ei], sizeof(env_bufs[ei]), "--env-QT_QPA_PLATFORM=wayland");
        new_argv[n++] = env_bufs[ei++];
    }

    /* Copy original args (skip argv[0]) */
    for (int i = 1; i < argc; i++)
        new_argv[n++] = argv[i];
    new_argv[n] = NULL;

    execvp("pkexec", new_argv);
    /* If execvp returns, pkexec failed */
    perror("pkexec");
    free(new_argv);
    exit(1);
}

/* ── Main ───────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    signal(SIGTERM, on_signal);
    signal(SIGINT,  on_signal);

    restore_env(&argc, &argv);
    elevate_if_necessary(argc, argv);

    if (!backend_is_supported()) {
        fprintf(stderr, "QTuxTimings: ryzen_smu driver not found at /sys/kernel/ryzen_smu_drv/\n"
                        "Please install the ryzen_smu kernel module.\n");
        return 1;
    }

    QApplication app(argc, argv);

    /* Force C locale for numeric formatting (dots, not commas).
     * Qt resets the locale during QApplication construction, so set it after. */
    setlocale(LC_NUMERIC, "C");

    int status;
    {
        MainWindow w;
        w.show();
        status = app.exec();
    } /* ~MainWindow joins worker threads before backend_cleanup() unloads
       * the kernel modules they may still be using. */
    backend_cleanup();
    return status;
}
