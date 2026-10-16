#include "my_shell.h"

//cd, pwd, exit, env, setenv, unsetenv, echo
int shell_builtins(char** args, char** env, char* initial_directory){


    if(my_strcmp(args[0], "cd") == 0){
        return command_cd(args, initial_directory, env);
    }
    else if(my_strcmp(args[0], "pwd") == 0){
        return command_pwd();
    }
    else if(my_strcmp(args[0], "echo") == 0){
        return command_echo(args, env);
    }
    else if(my_strcmp(args[0], "env") == 0){
        return command_env(env);
    }
    else if(my_strcmp(args[0], "which") == 0){
        return command_which(args, env);
    }
    else if(my_strcmp(args[0], "help") == 0){
        return command_help();
    }
    else if(my_strcmp(args[0], "history") == 0){
        return command_history(args);
    }
    else if(my_strcmp(args[0], "clear") == 0){
        return command_clear();
    }
    else if(my_strcmp(args[0], "exit") == 0 || my_strcmp(args[0], "quit") == 0){
        exit(EXIT_SUCCESS);
    }
    else{
        //Not a builtin cmd
        return executor(args, env);
    }

    return 0;
}

void shell_loop(char** env){
    char *input = NULL;
    size_t input_size = 0;

    char** args;

    char* initial_directory = getcwd(NULL, 0); // Get the initial directory
    
    while (1){
        printf("adv_shell> ");

        if(getline(&input, &input_size, stdin) == -1){ //EOF, ctrl+d
            
            perror("getline");
            break;
        }

        args = parse_input(input);
        // for(size_t i = 0; args[i]; i++){
        //     printf("Args: %s", args[i]);
        //     printf("\n");
        // }
        // //printf("Input: %s", input);

        if(!args[0]){
            free_tokens(args);
            continue;
        }

        add_history(input);

        if(my_strcmp(args[0], "setenv") == 0){
            env = command_setenv(args, env);
        }
        else if(my_strcmp(args[0], "unsetenv") == 0){
            env = command_unsetenv(args, env);
        }
        else{
            shell_builtins(args, env, initial_directory);
        }

        free_tokens(args);
    }

    free(input);
    free(initial_directory);
}

int main(int argc, char *argv, char** env){
    (void)argc;
    (void)argv;

    shell_loop(env);

    return 0;
}