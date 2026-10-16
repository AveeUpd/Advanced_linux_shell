#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>


#define MAX_INPUT 1024

// Redirection info extracted from a command line
typedef struct redirect_s {
    char* infile;    // target of '<'  (NULL if none)
    char* outfile;   // target of '>' or '>>' (NULL if none)
    int append;      // 1 for '>>', 0 for '>'
} redirect_t;

char** parse_input(char* input);
void free_tokens(char** tokens);
int parse_redirections(char** args, redirect_t* redir);
int parse_background(char** args);
void free_redirect(redirect_t* redir);

int shell_builtins(char** args, char** env, char* initial_directory, redirect_t* redir, int background);

//Built in func imp
int command_cd(char** args, char* initial_directory, char** env);
int command_pwd();
int command_echo(char** args, char** env);
int command_env(char** env);
int command_which(char** args, char** env);
int is_builtin(const char* name);
int command_help();
int command_history(char** args);
int command_clear();
int command_jobs(char** args);
void add_history(const char* line);

char** command_setenv(char** args, char** env);
char** command_unsetenv(char** args, char** env);

// Executor
int executor(char** args, char** env, redirect_t* redir, int background);

// Background jobs
void setup_sigchld_handler();
void reap_jobs();
void list_jobs();
int save_std_fds(int saved[2]);
int apply_redirections(redirect_t* redir);
void restore_std_fds(int saved[2]);
int child_process(char** args, char** env);

// Path functions
char* get_path(char** env);
char** split_paths(char* paths, int* count);

//helpers
int my_strcmp(const char* str1, const char* str2);
int my_strlen(const char* str);
int my_strncmp(const char* str1, const char* str2, size_t n);
char* my_getenv(const char* name, char** env);
char* my_strdup(const char* str);
char* my_strcpy(char* dest, const char* src);
char* my_strchr(const char* str, char c);
char* my_strtok(char* input_string, const char* delimiter);
char* my_strncpy(char* dest, const char* src, size_t n);