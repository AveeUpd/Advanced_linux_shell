#include "my_shell.h"

char** parse_input(char* input){
    size_t buffer_size = MAX_INPUT;
    char** tokens = malloc(buffer_size * sizeof(char*));
    char* token = NULL;
    size_t position = 0;
    size_t token_length = 0;

    if(!tokens){
        perror("malloc");
        exit(1);
    }

    for(size_t i = 0; input[i]; i++){
        
        while(input[i] == ' ' || input[i] == '\t' || input[i] == '\n' || input[i] == '\r'){
            i++;
        }

        if(!input[i]) //End of string
            break;

        token = &input[i];

        while(input[i] && input[i] != ' ' && input[i] != '\t' && input[i] != '\n' && input[i] != '\r'){
            token_length++;
            i++;
        }

        tokens[position] = malloc((token_length + 1) * sizeof(char));

        if(!tokens[position]){
            perror("malloc");
            exit(1);
        }

        for(size_t j = 0; j < token_length; j++){
            tokens[position][j] = token[j];
        }
        tokens[position][token_length] = '\0'; // Null-terminating token
        position++;
        token_length = 0; //Reset for next
    } 

    tokens[position] = NULL; //terminate array with null

    return tokens;
}

//free allocated tokens
void free_tokens(char** tokens){
    if(!tokens)
        return;
    
    for(size_t i = 0; tokens[i]; i++)
        free(tokens[i]);

    free(tokens);
}

//free the file names held by a redirect_t
void free_redirect(redirect_t* redir){
    free(redir->infile);
    free(redir->outfile);
    redir->infile = NULL;
    redir->outfile = NULL;
    redir->append = 0;
}

static int is_redirect_token(const char* token){
    return my_strcmp(token, "<") == 0 || my_strcmp(token, ">") == 0 || my_strcmp(token, ">>") == 0;
}

// Scans args for <, >, >> (each must be a separate token followed by a file name).
// The operator and file name tokens are removed from args so that args holds only
// the command and its arguments. File names are stored in redir (freed by free_redirect).
// If an operator is repeated the last one wins. Returns 0 on success, -1 on syntax error.
int parse_redirections(char** args, redirect_t* redir){
    size_t i = 0;

    redir->infile = NULL;
    redir->outfile = NULL;
    redir->append = 0;

    while(args[i]){
        if(!is_redirect_token(args[i])){
            i++;
            continue;
        }

        char* file = args[i + 1];
        if(file == NULL || is_redirect_token(file)){
            fprintf(stderr, "syntax error: expected file name after '%s'\n", args[i]);
            return -1;
        }

        if(my_strcmp(args[i], "<") == 0){
            free(redir->infile);
            redir->infile = file;
        }
        else{
            free(redir->outfile);
            redir->outfile = file;
            redir->append = (my_strcmp(args[i], ">>") == 0);
        }

        free(args[i]); //operator token; file token is now owned by redir

        //shift the remaining tokens (and the NULL terminator) left by two
        size_t j = i;
        while(args[j + 2]){
            args[j] = args[j + 2];
            j++;
        }
        args[j] = NULL;
    }

    if(args[0] == NULL && (redir->infile || redir->outfile)){
        fprintf(stderr, "syntax error: missing command before redirection\n");
        return -1;
    }
    return 0;
}
