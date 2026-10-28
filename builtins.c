#include "my_shell.h"

int command_cd(char** args, char* initial_directory, char** env){
    (void)initial_directory;
    char* home = my_getenv("HOME", env);
    char* path = NULL;       // allocated only for "~/..." expansion
    const char* target = NULL;
    int ret = 0;

    if(args[1] != NULL && args[2] != NULL){
        fprintf(stderr, "cd: too many arguments\n");
        return 1;
    }

    if(args[1] == NULL || my_strcmp(args[1], "~") == 0){   // cd  /  cd ~
        if(home == NULL || home[0] == '\0'){
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
        target = home;
    }
    else if(args[1][0] == '~' && args[1][1] == '/'){        // cd ~/dir
        if(home == NULL || home[0] == '\0'){
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
        path = malloc(my_strlen(home) + my_strlen(args[1]));
        if(!path){
            perror("malloc");
            return 1;
        }
        sprintf(path, "%s%s", home, args[1] + 1);
        target = path;
    }
    else{                                                   // cd .. / cd dir
        target = args[1];
    }

    if(chdir(target) != 0){
        fprintf(stderr, "cd: %s: %s\n", target, strerror(errno));
        ret = 1;
    }

    free(path);
    return ret;
}

int command_pwd(){
    char* cwd = NULL;

    //uses dynamic allocation
    cwd = getcwd(NULL, 0);
    if(cwd != NULL){
        printf("%s\n", cwd);
        free(cwd); // Free the allocated memory
    }
    else{
        perror("pwd");
    }
    return 0;
}

int command_echo(char** args, char** env){
    int new_line = 1; //Default echo ends with new line
    size_t i = 1;

    if(args[1] != NULL &&  my_strcmp(args[1], "-n") == 0){
        new_line = 0; //No new line
        i++; //Skip the -n argument
    }

    for(; args[i]; i++){
        if(my_strcmp(args[i], "$?") == 0){
            printf("%d", get_last_status());
        }
        else if(args[i][0] == '$'){
            char* value = my_getenv(args[i] + 1, env);
            if(value){
                printf("%s", value);
            }
            else{
                printf(" "); 
            }
        }
        else{
            printf("%s", args[i]);
        }

        if(args[i + 1] != NULL){
            new_line = 0;
            printf(" ");
        }

        if(new_line){
            printf("\n");
        }

        new_line = 1;
    }
    return 0;
}

int command_env(char** env){
    size_t i = 0;
    while(env[i]){
        printf("%s\n", env[i]);
        i++;
    }
    return 0;
}

char* find_command_in_path(char* command, char** env);

static const char* built_in_commands[] = {"cd", "pwd", "echo", "env", "setenv", "unsetenv", "which", "help", "history", "clear", "jobs", "status", "procinfo", "schedule", "exit", "quit", NULL};

// 1 if name is a shell built-in (these run inside the shell process)
int is_builtin(const char* name){
    for(size_t i = 0; built_in_commands[i]; i++){
        if(my_strcmp(name, built_in_commands[i]) == 0){
            return 1;
        }
    }
    return 0;
}

int command_which(char** args, char** env){
    if(args[1] == NULL){
        printf("which: expected argument to \"which [command]\"\n");
        return 1;
    }

    //Check builtins
    if(is_builtin(args[1])){
        printf("%s: shell built-in command\n", args[1]);
        return 0;
    }

    //Check external commands
    char* full_path = find_command_in_path(args[1], env);

    if (full_path != NULL) {
        printf("%s\n", full_path);
        free(full_path);
        return 0;
    } else {
        printf("which: %s command not found\n", args[1]);
        return 1;
    }
}

char* find_command_in_path(char* command, char** env){
    char* path_env = NULL; //store path val
    char* path = NULL; //dupe of path
    char* token = NULL; //tokenize dirs from path
    char full_path[1024]; //buffer to construct full path

    //locate the path
    for(size_t i = 0; env[i]; i++){
        if (my_strncmp(env[i], "PATH=", 5) == 0) {
            path_env = env[i] + 5; // Skip PATH= prefix
            break;
        }
    }

    if(path_env == NULL){
        return NULL;
    }

    //Duplicate the path to avoid modifying the original
    path = my_strdup(path_env);
    if(path == NULL) {
        perror("my_strdup");
        return NULL;
    }

    // Split PATH into individual dirs using ':' delimiter
    token = my_strtok(path, ":");
    while (token != NULL) {
        // Constract full path, check for '/'
        size_t len = my_strlen(token);
        if (token[len - 1] != '/') {
            snprintf(full_path, sizeof(full_path), "%s%s%s", token, "/", command);
        } else {
            snprintf(full_path, sizeof(full_path), "%s%s", token, command);
        }

        // Check if the command exists as executable
        if (access(full_path, X_OK) == 0) {
            free(path);
            return my_strdup(full_path); // found commands path
        }

        token = my_strtok(NULL, ":"); // move to the next dir
    }


    printf("Path: %s\n", path);

    free(path);
    return NULL;
}

//helper func to count env vars
int count_env_vars(char** env){
    int count = 0;
    while(env[count]){
        count++; 
    }
    return count;
}

//func to set env var
char** command_setenv(char** args, char** env){
    if(args[1] == NULL){
        printf("Usage: setenv VAR=value\n setenv <variable>\n");
        return NULL;
    }
    
    int env_count = count_env_vars(env);
    char** new_env = malloc((env_count + 2) * sizeof(char *));
    if(!new_env){
        perror("malloc");
        return env;
    }

    for (size_t i = 0; i < env_count; i++){
        new_env[i] = my_strdup(env[i]);
        if(!new_env[i]){
            perror("strdup");
            for (size_t j = 0; j < i; j++)
            {
                free(new_env[j]);
            }
            free(new_env);
            return env;
        }
    }

    char* new_var = NULL;
    if(args[2] == NULL){
        new_var = my_strdup(args[1]);
    }
    else{
        new_var = malloc(my_strlen(args[1]) + my_strlen(args[2]) + 2);
        if(new_var)
            sprintf(new_var, "%s=%s", args[1], args[2]);
    }

    if(!new_var){
        perror("malloc");
        for (size_t i = 0; i < env_count; i++)
        {
            free(new_env[i]);
        }
        free(new_env);
        return env;
    }

    new_env[env_count] = new_var;
    new_env[env_count + 1] = NULL;

    // for(size_t i = 0; env[i]; i++){
    //     free(env[i]);
    // }
    // free(env);
    
    // for(size_t i = env_count; env[i]; i++){
    //     printf("Env: %s", new_env[i]);
    //     printf("\n");
    // }
    return new_env; 
}

char** command_unsetenv(char** args, char** env){
    if (!args[1]) {
        printf("Usage: unsetenv <variable>\n");
        return env;
    }

    int env_count = count_env_vars(env);
    char** new_env = malloc(env_count * sizeof(char*));
    if(!new_env) {
        perror("malloc");
        return env;
    }

    int j = 0, found = 0;
    for (int i = 0; i < env_count; i++) { // var=123
        if (my_strncmp(env[i], args[1], my_strlen(args[1])) == 0 && env[i][my_strlen(args[1])] == '=') {
            found = 1;
            free(env[i]); // Free the matching variable
        } else {
            new_env[j++] = env[i];
        }
    }

    if (!found) {
        printf("Variable %s not found in environment\n", args[1]);
        free(new_env);
        return env;
    }

    new_env[j] = NULL;
    // free(env);
    return new_env;

}


//help, history, clear
#define HISTORY_MAX 1000

static char* history_list[HISTORY_MAX];
static int history_count = 0;       // entries currently stored
static int history_total = 0;       // commands entered so far (for numbering)

void add_history(const char* line){
    char* copy = my_strdup(line);
    if(!copy){
        perror("malloc");
        return;
    }

    //strip trailing newline / carriage return left by getline
    int len = my_strlen(copy);
    while(len > 0 && (copy[len - 1] == '\n' || copy[len - 1] == '\r')){
        copy[--len] = '\0';
    }

    if(history_count == HISTORY_MAX){      // full: drop oldest, shift the rest
        free(history_list[0]);
        for(int i = 1; i < HISTORY_MAX; i++){
            history_list[i - 1] = history_list[i];
        }
        history_count--;
    }
    history_list[history_count++] = copy;
    history_total++;
}

int command_history(char** args){
    if(args[1] != NULL){
        fprintf(stderr, "history: takes no arguments\n");
        return 1;
    }

    int first = history_total - history_count + 1;   // number of oldest stored entry
    for(int i = 0; i < history_count; i++){
        printf("%5d  %s\n", first + i, history_list[i]);
    }
    return 0;
}

int command_help(){
    printf("Built-in commands:\n");
    printf("  cd [dir]          change directory (no arg or ~ = HOME, .. = parent, ~/dir supported)\n");
    printf("  pwd               print current working directory\n");
    printf("  echo [-n] [args]  print arguments ($VAR expands an environment variable)\n");
    printf("  env               print the environment\n");
    printf("  setenv VAR value  set an environment variable\n");
    printf("  unsetenv VAR      remove an environment variable\n");
    printf("  which cmd         show whether cmd is a built-in or its path\n");
    printf("  history           list previously entered commands\n");
    printf("  clear             clear the terminal screen\n");
    printf("  jobs              list background jobs (ID, PID, status, command)\n");
    printf("  status            print the exit status of the last command (also: echo $?)\n");
    printf("  procinfo [pid|%%N] show PID, PPID, state, process group of a process (default: the shell)\n");
    printf("  schedule          CPU scheduling simulator (FCFS, SJF, Priority, Round Robin)\n");
    printf("  help              show this message\n");
    printf("  exit | quit       leave the shell\n");
    printf("\nShell features:\n");
    printf("  - External commands are searched in PATH, then the current directory\n");
    printf("  - Each external command runs in a child process (fork/execve/waitpid)\n");
    printf("  - A trailing & runs an external command in the background\n");
    return 0;
}

int command_clear(){
    //ANSI escape: move cursor home, erase screen, erase scrollback
    printf("\033[H\033[2J\033[3J");
    fflush(stdout);
    return 0;
}

int command_jobs(char** args){
    if(args[1] != NULL){
        fprintf(stderr, "jobs: takes no arguments\n");
        return 1;
    }
    list_jobs();
    return 0;
}

int command_status(char** args){
    if(args[1] != NULL){
        fprintf(stderr, "status: takes no arguments\n");
        return 1;
    }
    printf("%d\n", get_last_status());
    return 0;
}
