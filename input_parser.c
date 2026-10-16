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
    if(tokens)
        return;
    
    for(size_t i = 0; tokens[i]; i++)
        free(tokens[i]);

    free(tokens);
}