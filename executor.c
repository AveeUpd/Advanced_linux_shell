#include "my_shell.h"

// Executes a command by forking and running it in a child process
int executor(char** args, char** env, redirect_t* redir)
{
    pid_t pid;
    int status;

    pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) // child process
    { 
        // Redirect only in the child: the shell's own fd 0/1 stay untouched
        if (apply_redirections(redir) == -1)
            _exit(1);
        child_process(args, env);   // returns only if exec failed (already reported)
        _exit(127);
    } 
    else // Parent process
    { 
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            return 1;
        }
        if (WIFSIGNALED(status)) {
            printf("Process terminated by signal: %d\n", WTERMSIG(status));
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
