#include "my_shell.h"

// ---- Background job table ----
#define MAX_JOBS 64

enum { JOB_RUNNING, JOB_DONE, JOB_EXITED, JOB_SIGNALED };

typedef struct job_s {
    int id;          // job number shown as [id]
    pid_t pid;
    char* command;   // command text, malloc'd
    int state;       // JOB_RUNNING, JOB_DONE (exit 0), JOB_EXITED (exit != 0), JOB_SIGNALED
    int code;        // exit status or terminating signal number
} job_t;

// Exit status of the last command ($? / status): exit code, or 128+signal if it was killed
static int last_status = 0;

int get_last_status() { return last_status; }
void set_last_status(int status) { last_status = status; }

static job_t job_table[MAX_JOBS];
static int job_count = 0;

// The handler only sets this flag (async-signal-safe); the job table is touched
// only from normal code in the main loop.
static volatile sig_atomic_t sigchld_received = 0;

static void sigchld_handler(int sig)
{
    (void)sig;
    sigchld_received = 1;
}

void setup_sigchld_handler()
{
    struct sigaction sa;

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    // no SA_RESTART: a SIGCHLD while the shell waits for input makes getline fail
    // with EINTR so the job can be reaped immediately; SA_NOCLDSTOP: only exits matter
    sa.sa_flags = SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
        perror("sigaction");
}

// Joins args into one space separated string
static char* join_args(char** args)
{
    size_t len = 1;

    for (size_t i = 0; args[i]; i++)
        len += my_strlen(args[i]) + 1;

    char* str = malloc(len);
    if (!str)
        return NULL;

    char* p = str;
    for (size_t i = 0; args[i]; i++) {
        size_t n = my_strlen(args[i]);
        if (i)
            *p++ = ' ';
        memcpy(p, args[i], n);
        p += n;
    }
    *p = '\0';
    return str;
}

static void add_job(pid_t pid, char** args)
{
    int id = 1;

    for (int i = 0; i < job_count; i++) {
        if (job_table[i].id >= id)
            id = job_table[i].id + 1;
    }

    job_t* job = &job_table[job_count++];
    job->id = id;
    job->pid = pid;
    job->command = join_args(args);
    job->state = JOB_RUNNING;
    job->code = 0;

    printf("[%d] %d\n", id, (int)pid);
}

static void remove_job(int index)
{
    free(job_table[index].command);
    for (int i = index + 1; i < job_count; i++)
        job_table[i - 1] = job_table[i];
    job_count--;
}

static void job_status_text(const job_t* job, char* buf, size_t size)
{
    switch (job->state) {
    case JOB_RUNNING:  snprintf(buf, size, "Running"); break;
    case JOB_DONE:     snprintf(buf, size, "Done"); break;
    case JOB_EXITED:   snprintf(buf, size, "Exit %d", job->code); break;
    default:           snprintf(buf, size, "%s", strsignal(job->code)); break;
    }
}

// Non-blocking check of every running job: waitpid(WNOHANG) reaps the child if it
// has finished (no zombie left) and records how it ended.
static void update_jobs()
{
    for (int i = 0; i < job_count; i++) {
        job_t* job = &job_table[i];
        int status;
        pid_t r;

        if (job->state != JOB_RUNNING)
            continue;

        do {
            r = waitpid(job->pid, &status, WNOHANG);
        } while (r == -1 && errno == EINTR);

        if (r == job->pid) {
            if (WIFEXITED(status)) {
                job->code = WEXITSTATUS(status);
                job->state = (job->code == 0) ? JOB_DONE : JOB_EXITED;
            }
            else if (WIFSIGNALED(status)) {
                job->code = WTERMSIG(status);
                job->state = JOB_SIGNALED;
            }
        }
        else if (r == -1) {
            job->state = JOB_DONE; // child no longer exists
        }
    }
}

// Called before each prompt: if SIGCHLD arrived, reap finished jobs, report them and drop them
void reap_jobs()
{
    if (!sigchld_received)
        return;
    sigchld_received = 0; // cleared before checking so a signal arriving meanwhile is not lost

    update_jobs();
    for (int i = 0; i < job_count; ) {
        if (job_table[i].state != JOB_RUNNING) {
            char text[64];
            job_status_text(&job_table[i], text, sizeof(text));
            printf("[%d]  %-20s %s\n", job_table[i].id, text,
                   job_table[i].command ? job_table[i].command : "?");
            remove_job(i);
        }
        else {
            i++;
        }
    }
}

// PID of the job with the given [id], or -1 if there is no such job
pid_t job_pid_by_id(int id)
{
    for (int i = 0; i < job_count; i++) {
        if (job_table[i].id == id)
            return job_table[i].pid;
    }
    return -1;
}

// Built-in 'jobs': print the table; finished jobs are shown once and then removed
void list_jobs()
{
    update_jobs();
    for (int i = 0; i < job_count; ) {
        char text[64];
        job_status_text(&job_table[i], text, sizeof(text));
        printf("[%d]  %-6d %-20s %s\n", job_table[i].id, (int)job_table[i].pid, text,
               job_table[i].command ? job_table[i].command : "?");
        if (job_table[i].state != JOB_RUNNING)
            remove_job(i);
        else
            i++;
    }
}

// Executes a command by forking and running it in a child process.
// A background command is not waited for; it is added to the job table instead.
int executor(char** args, char** env, redirect_t* redir, int background)
{
    pid_t pid;
    int status;

    if (background && job_count == MAX_JOBS) {
        fprintf(stderr, "too many background jobs\n");
        last_status = 1;
        return 1;
    }

    pid = fork();
    if (pid == -1) {
        perror("fork");
        last_status = 1;
        return 1;
    }

    if (pid == 0) // child process
    { 
        if (background) {
            setpgid(0, 0); // own process group: terminal Ctrl+C is not delivered to the job
            int null_fd = open("/dev/null", O_RDONLY); // job must not read the terminal
            if (null_fd != -1) {
                dup2(null_fd, STDIN_FILENO);
                if (null_fd != STDIN_FILENO)
                    close(null_fd);
            }
        }
        // Redirect only in the child: the shell's own fd 0/1 stay untouched
        // (an explicit '<' overrides the /dev/null above)
        if (apply_redirections(redir) == -1)
            _exit(1);
        child_process(args, env);   // returns only if exec failed (already reported)
        _exit(127);
    } 
    else // Parent process
    { 
        if (background) {
            add_job(pid, args);
            last_status = 0; // starting a background job succeeded
            return 1;
        }
        while (waitpid(pid, &status, 0) == -1) {
            if (errno != EINTR) { // EINTR: a background job's SIGCHLD interrupted the wait, so retry
                perror("waitpid");
                last_status = 1;
                return 1;
            }
        }
        if (WIFEXITED(status)) {
            last_status = WEXITSTATUS(status);
        }
        else if (WIFSIGNALED(status)) {
            printf("Process terminated by signal: %d\n", WTERMSIG(status));
            last_status = 128 + WTERMSIG(status);
        }
        // if (WTERMSIG(status) == SIGSEGV) {
        //     perror("segmentation fault");
        // }
    }
    printf("\n");
    return 1;
}

// Attempts to execute the command by searching paths and the current directory
int child_process(char** args, char** env) 
{
    char* path_string = get_path(env);
    int num_paths;
    char** path_list = split_paths(path_string, &num_paths);

    // access() execve()
    for (int i = 0; i < num_paths; i++) {
        char full_path[MAX_INPUT];
        snprintf(full_path, sizeof(full_path), "%s/%s", path_list[i], args[0]);

        if (access(full_path, X_OK) == 0) {
            execve(full_path, args, env);
        }
    }

    for (int i = 0; i < num_paths; i++)
        free(path_list[i]);

    free(path_list);
    free(path_string);
    

    // Try executing the command in the current working directory
    char* cwd = NULL;
    cwd = getcwd(NULL, 0);
    if(cwd == NULL) {
        perror("getcwd");
        return 1;
    }

    char full_cwd_path[MAX_INPUT];
    snprintf(full_cwd_path, sizeof(full_cwd_path), "%s/%s", cwd, args[0]);
    execve(full_cwd_path, args, env);

    perror("execve");
    
    return 1;
}

// Fetches the PATH environment variable
char* get_path(char** env) {
    for (int i = 0; env[i]; i++) {
        if(my_strncmp(env[i], "PATH=", 5) == 0) {
            return my_strdup(env[i] + 5);
        }
    }
    return NULL;
}

// Split the PATH string into individual paths
char** split_paths(char* paths, int* count) {
    char** result = NULL;
    char* token;
    // size_t size_of_path = my_strlen(paths);
    // char paths_copy[size_of_path];

    // my_strncpy(paths_copy, paths, sizeof(paths_copy));
    // paths_copy[sizeof(paths_copy) - 1] = '\0';
    char *paths_copy = my_strdup(paths);

    if (paths_copy == NULL) {
        perror("my_strdup");
        return NULL;
    }

    token = my_strtok(paths_copy, ":");
    *count = 0;
    if (paths_copy == NULL) {
        perror("my_strdup");
        return NULL;
    }
    while (token) {
        result = realloc(result, ((*count + 1) * sizeof(char*)));
        if (!result) {
            perror("realloc");
            return NULL;
        }
        result[*count] = my_strdup(token);
        if (!result[*count]) {
            perror("my_strdup");
            return NULL;
        }

        (*count)++;
        token = my_strtok(NULL, ":");
    }

    free(paths_copy);
    return result;
}

// Opens the redirection targets and dup2()s them onto stdin/stdout.
// Returns 0 on success, -1 on failure (error already printed).
int apply_redirections(redirect_t* redir)
{
    int fd;

    if (redir->infile) {
        fd = open(redir->infile, O_RDONLY);
        if (fd == -1) {
            fprintf(stderr, "%s: %s\n", redir->infile, strerror(errno));
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) == -1) {
            perror("dup2");
            close(fd);
            return -1;
        }
        if (fd != STDIN_FILENO)
            close(fd); // fd 0 now refers to the file; the extra descriptor is not needed
    }

    if (redir->outfile) {
        int flags = O_WRONLY | O_CREAT | (redir->append ? O_APPEND : O_TRUNC);

        fd = open(redir->outfile, flags, 0644); // 0644 & ~umask if the file is created
        if (fd == -1) {
            fprintf(stderr, "%s: %s\n", redir->outfile, strerror(errno));
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) == -1) {
            perror("dup2");
            close(fd);
            return -1;
        }
        if (fd != STDOUT_FILENO)
            close(fd);
    }
    return 0;
}

// Keeps copies of the shell's stdin/stdout so a built-in can run redirected
// inside the shell process and the originals can be put back afterwards.
int save_std_fds(int saved[2])
{
    fflush(stdout); // output already buffered (e.g. the prompt) must go to the original stdout, not the redirect target
    saved[0] = dup(STDIN_FILENO);
    if (saved[0] == -1)
        return -1;
    saved[1] = dup(STDOUT_FILENO);
    if (saved[1] == -1) {
        close(saved[0]);
        return -1;
    }
    return 0;
}

void restore_std_fds(int saved[2])
{
    fflush(stdout); // buffered output must reach the redirect target before fd 1 is switched back
    dup2(saved[0], STDIN_FILENO);
    dup2(saved[1], STDOUT_FILENO);
    close(saved[0]);
    close(saved[1]);
}
