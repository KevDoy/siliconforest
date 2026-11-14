// SPDX-License-Identifier: MIT
// /usr/bin/open: start an application bundle on the SiliconForest desktop, as macOS's open(1)
// does for the forms people type:
//   open <path to .app>             open -a <name or path> [--args <arguments...>]
//   open -n ...  (accepted: every open starts a new instance here)
// The bundle's executable (CFBundleExecutable, read by CFBundle) is started in a session of its
// own with the desktop's environment (the compositor's socket: XDG_RUNTIME_DIR and
// WAYLAND_DISPLAY, taken from the caller or the session's defaults), detached from the terminal:
// open returns at once with status 0, or says why the bundle could not be started. What the
// application writes goes to ~/Library/Logs/<name>.log.
// Not macOS's: documents and URLs (there is no LaunchServices database here), -b, -e, -t, -W, -g.
#include <CoreFoundation/CoreFoundation.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern char **environ;

static int is_dir(const char *p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }

// <name> or <name>.app in the application directories, or a path as it is
static int find_bundle(const char *name, char *out, size_t size)
{
    static const char *dirs[] = { "/Applications", "/Applications/Utilities", "/System/Applications", "/System/Applications/Utilities", NULL };
    const char *home = getenv("HOME");
    if (strchr(name, '/')) { snprintf(out, size, "%s", name); return is_dir(out); }
    const char *suffix = strlen(name) > 4 && strcmp(name + strlen(name) - 4, ".app") == 0 ? "" : ".app";
    for (int i = 0; dirs[i]; i++) { snprintf(out, size, "%s/%s%s", dirs[i], name, suffix); if (is_dir(out)) return 1; }
    if (home) { snprintf(out, size, "%s/Applications/%s%s", home, name, suffix); if (is_dir(out)) return 1; }
    return 0;
}

int main(int argc, char **argv)
{
    const char *target = NULL; char **args = NULL; int nargs = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--args") == 0) { args = argv + i + 1; nargs = argc - i - 1; break; }
        else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) target = argv[++i];
        else if (strcmp(argv[i], "-n") == 0) continue;
        else if (argv[i][0] == '-') { fprintf(stderr, "open: option %s is not supported here\n", argv[i]); return 1; }
        else if (target == NULL) target = argv[i];
        else { fprintf(stderr, "open: documents are not supported here (one application bundle per call)\n"); return 1; }
    }
    if (target == NULL) { fprintf(stderr, "usage: open <application bundle> | open -a <application> [--args ...]\n"); return 1; }
    char bundle[PATH_MAX], real[PATH_MAX], exe[PATH_MAX];
    if (!find_bundle(target, bundle, sizeof(bundle)) || realpath(bundle, real) == NULL) { fprintf(stderr, "open: no application bundle %s\n", target); return 1; }
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)real, (CFIndex)strlen(real), true);
    CFBundleRef b = url ? CFBundleCreate(NULL, url) : NULL;
    CFURLRef exeURL = b ? CFBundleCopyExecutableURL(b) : NULL;
    if (exeURL == NULL || !CFURLGetFileSystemRepresentation(exeURL, true, (UInt8 *)exe, sizeof(exe)) || access(exe, X_OK) != 0) {
        fprintf(stderr, "open: %s has no executable (CFBundleExecutable)\n", real); return 1;
    }
    // the desktop's environment where the caller has none
    setenv("XDG_RUNTIME_DIR", "/private/var/run/gpu-desktop", 0);
    setenv("WAYLAND_DISPLAY", "wayland-0", 0);
    if (getenv("HOME") == NULL) setenv("HOME", "/var/root", 0);
    char sock[PATH_MAX]; snprintf(sock, sizeof(sock), "%s/%s", getenv("XDG_RUNTIME_DIR"), getenv("WAYLAND_DISPLAY"));
    if (access(sock, F_OK) != 0) { fprintf(stderr, "open: no desktop to open it on (%s: %s)\n", sock, strerror(errno)); return 1; }
    // its output: ~/Library/Logs/<executable name>.log
    char log[PATH_MAX]; const char *leaf = strrchr(exe, '/'); leaf = leaf ? leaf + 1 : exe;
    snprintf(log, sizeof(log), "%s/Library", getenv("HOME")); mkdir(log, 0755);
    snprintf(log, sizeof(log), "%s/Library/Logs", getenv("HOME")); mkdir(log, 0755);
    snprintf(log, sizeof(log), "%s/Library/Logs/%s.log", getenv("HOME"), leaf);

    char **child = calloc((size_t)nargs + 2, sizeof(char *));
    child[0] = exe; for (int i = 0; i < nargs; i++) child[i + 1] = args[i];
    posix_spawn_file_actions_t fa; posix_spawnattr_t at;
    posix_spawn_file_actions_init(&fa); posix_spawnattr_init(&at);
    posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&fa, 1, log, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    posix_spawn_file_actions_adddup2(&fa, 1, 2);
    posix_spawnattr_setflags(&at, POSIX_SPAWN_SETSID);          // its own session: it outlives the terminal it was typed in
    pid_t pid = 0;
    int rc = posix_spawn(&pid, exe, &fa, &at, child, environ);
    if (rc != 0) { fprintf(stderr, "open: %s: %s\n", exe, strerror(rc)); return 1; }
    return 0;
}
