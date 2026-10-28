#include "my_shell.h"

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

// Prints PID, PPID, state, process group and command of one process.
// Linux reads /proc/<pid>/stat; macOS has no /proc, so it uses sysctl instead.
static int print_procinfo(pid_t pid)
{
#ifdef __linux__
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);

    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "procinfo: no such process: %d\n", (int)pid);
        return 1;
    }
    char line[1024];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        fprintf(stderr, "procinfo: cannot read %s\n", path);
        return 1;
    }
    fclose(f);

    // Format: pid (comm) state ppid pgrp ... ; comm may contain spaces/parens,
    // so parse from the last ')'
    char* open = strchr(line, '(');
    char* close = strrchr(line, ')');
    if (!open || !close || close < open) {
        fprintf(stderr, "procinfo: unexpected format in %s\n", path);
        return 1;
    }
    *close = '\0';
    char state;
    int ppid, pgrp;
    if (sscanf(close + 2, "%c %d %d", &state, &ppid, &pgrp) != 3) {
        fprintf(stderr, "procinfo: unexpected format in %s\n", path);
        return 1;
    }
    const char* name;
    switch (state) {
    case 'R': name = "running"; break;
    case 'S': name = "sleeping"; break;
    case 'D': name = "disk wait"; break;
    case 'Z': name = "zombie"; break;
    case 'T': case 't': name = "stopped"; break;
    default:  name = "other"; break;
    }
    printf("PID:     %d\nPPID:    %d\nState:   %c (%s)\nPGID:    %d\nCommand: %s\n",
           (int)pid, ppid, state, name, pgrp, open + 1);
    return 0;
#elif defined(__APPLE__)
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, (int)pid };
    struct kinfo_proc kp;
    size_t len = sizeof(kp);

    if (sysctl(mib, 4, &kp, &len, NULL, 0) == -1 || len == 0) {
        fprintf(stderr, "procinfo: no such process: %d\n", (int)pid);
        return 1;
    }
    const char* name;
    switch (kp.kp_proc.p_stat) {
    case SRUN:   name = "running"; break;
    case SSLEEP: name = "sleeping"; break;
    case SIDL:   name = "idle (being created)"; break;
    case SSTOP:  name = "stopped"; break;
    case SZOMB:  name = "zombie"; break;
    default:     name = "other"; break;
    }
    printf("PID:     %d\nPPID:    %d\nState:   %s\nPGID:    %d\nCommand: %s\n",
           (int)kp.kp_proc.p_pid, (int)kp.kp_eproc.e_ppid, name,
           (int)kp.kp_eproc.e_pgid, kp.kp_proc.p_comm);
    return 0;
#else
    (void)pid;
    fprintf(stderr, "procinfo: not supported on this platform\n");
    return 1;
#endif
}

// procinfo [pid | %jobid]   (no argument = the shell itself)
int command_procinfo(char** args)
{
    pid_t pid = getpid();

    if (args[1] && args[2]) {
        fprintf(stderr, "procinfo: usage: procinfo [pid|%%jobid]\n");
        return 1;
    }
    if (args[1]) {
        char* end;
        int is_job = (args[1][0] == '%');
        long n = strtol(args[1] + is_job, &end, 10);

        if (end == args[1] + is_job || *end != '\0' || n <= 0) {
            fprintf(stderr, "procinfo: invalid pid or job: %s\n", args[1]);
            return 1;
        }
        if (is_job) {
            pid = job_pid_by_id((int)n);
            if (pid == -1) {
                fprintf(stderr, "procinfo: no such job: %s\n", args[1]);
                return 1;
            }
        }
        else {
            pid = (pid_t)n;
        }
    }
    return print_procinfo(pid);
}
