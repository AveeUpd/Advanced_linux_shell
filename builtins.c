#include "my_shell.h"

int command_cd(char** args, char* initial_directory){
    (void)initial_directory;
    if(args[1] == NULL){
        printf("cd: expected argument to \"cd [dir]\"\n");
    }
    else if(chdir(args[1]) == 0){
        // Change directory successful
    }
    else{
        perror("cd");
    }
    return 0;
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
        if(args[i][0] == '$'){
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

int command_which(char** args, char** env){
    if(args[1] == NULL){
        printf("which: expected argument to \"which [command]\"\n");
        return 1;
    }

    //List of builtins
    const char* built_in_commands[] = {"cd", "pwd", "echo", "env", "setenv", "unsetenv", "which", "exit", "quit", NULL};
    for(size_t i = 0; built_in_commands[i]; i++){
        if(my_strcmp(args[1], built_in_commands[i]) == 0){
            printf("%s: shell built-in command\n", args[1]);
            return 0;
        }
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
