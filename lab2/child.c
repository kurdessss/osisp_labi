#define _DEFAULT_SOURCE
#define _POSIX_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

extern char** environ;

char* find(char* name, char** envir) {
    char* result = NULL;
    for(int i = 0; envir[i]; i++) {
        if(strncmp(name, envir[i], strlen(name)) == 0) {
            char* buf = NULL;
            buf = (char*)malloc(sizeof(char) * strlen(envir[i]));
            strcpy(buf, envir[i]);

            result = strtok(envir[i], "=");

            strcpy(envir[i], buf);
            free(buf);

            result = strtok(NULL, "\0");
        }
    }
    return result;
 }

int main(int argc, char** argv, char** env) {
    FILE* environment_variables;
    char* envFileName = "ENV_VAR";
    char* filepath = NULL;
    char type = argv[1][0];

    if(argc < 2)
    {
        puts("Invalid child args");
        exit(EXIT_FAILURE);
    }

    puts(argv[0]);
    printf("%i\n%i\n", (int)getpid(), (int)getppid());

    //Getting filepath by type
    filepath = type == '+' ? getenv(envFileName) : 
            type == '*' ? find(envFileName, env) :
            type == '&' ? find(envFileName, environ) : NULL;

    if(filepath == NULL){
        puts("Getting by type error");
        exit(EXIT_FAILURE);
    }

    environment_variables = fopen(filepath, "r");
    if(environment_variables != NULL)
    {
        //Reading and output variables
        char str[512];
        while(EOF != fscanf(environment_variables, "%s\n", str)){
            if(getenv(str))
                printf("%s=%s\n", str, getenv(str));
            else
                continue;
        }
        fclose(environment_variables);
    }
    else
        perror("Environment variables file");

    exit(EXIT_SUCCESS);
    return 0;
}