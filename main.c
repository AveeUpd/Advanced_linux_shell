#include "my_shell.h"

//cd, pwd, exit, env, setenv, unsetenv, echo
int shell_builtins(char** args, char** env, char* initial_directory, redirect_t* redir, int background){


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
    else if(my_strcmp(args[0], "schedule") == 0){
        return command_schedule(args);
    }
    else if(my_strcmp(args[0], "clear") == 0){
        return command_clear();
    }
    else if(my_strcmp(args[0], "jobs") == 0){
        return command_jobs(args);
    }
    else if(my_strcmp(args[0], "exit") == 0 || my_strcmp(args[0], "quit") == 0){
        exit(EXIT_SUCCESS);
    }
    else{
        //Not a builtin cmd
        return executor(args, env, redir, background);   // built-ins ignore the background flag
    }

    return 0;
}

void shell_loop(char** env){
    char *input = NULL;
    size_t input_size = 0;

    char** args;

    char* initial_directory = getcwd(NULL, 0); // Get the initial directory
    
    setup_sigchld_handler();

    while (1){
        reap_jobs(); // report and reap finished background jobs before prompting
        printf("adv_shell> ");

        errno = 0;
        if(getline(&input, &input_size, stdin) == -1){
            if(errno == EINTR){     // a child changed state (SIGCHLD) while waiting for input
                clearerr(stdin);
                printf("\n");
                continue;           // top of loop reaps it and shows the prompt again
            }
            //EOF, ctrl+d
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

        int background = parse_background(args);
        if(background == -1){   // error already reported
            free_tokens(args);
            continue;
        }

        redirect_t redir;
        if(parse_redirections(args, &redir) == -1){   // error already reported
            free_tokens(args);
            free_redirect(&redir);
            continue;
        }

        // Built-ins run inside the shell process, so redirect the shell's own
        // fd 0/1 temporarily and restore them afterwards. External commands are
        // redirected in the forked child instead (see executor).
        int saved_fds[2];
        int fds_saved = 0;
        int run = 1;

        if(is_builtin(args[0]) && (redir.infile || redir.outfile)){
            if(save_std_fds(saved_fds) == -1){
                perror("dup");
                run = 0;
            }
            else{
                fds_saved = 1;
                if(apply_redirections(&redir) == -1)
                    run = 0;
            }
        }

        if(run){
            if(my_strcmp(args[0], "setenv") == 0){
                env = command_setenv(args, env);
            }
            else if(my_strcmp(args[0], "unsetenv") == 0){
                env = command_unsetenv(args, env);
            }
            else{
                shell_builtins(args, env, initial_directory, &redir, background);
            }
        }

        if(fds_saved)
            restore_std_fds(saved_fds);

        free_tokens(args);
        free_redirect(&redir);
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