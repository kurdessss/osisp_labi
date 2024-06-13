#define _DEFAULT_SOURCE
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char** environ;

 void init_child(char* child_path, char type) {
    static int child_counter = 0;
    char* child_argv[3] = {(char*)0};

    if(child_path == NULL) {
        puts("Child's path is empty.");
        return;
    }

    if(child_counter > 99) {
        puts("Too many child streams.");
        return;
    }
    
    pid_t pid;
    switch (pid = fork()) {

        case -1:
            perror("Fork");
            exit(EXIT_FAILURE);
        break;

        case 0:
            child_argv[0] = (char*)malloc(10 * sizeof(char));
            child_argv[1] = (char*)malloc(1 * sizeof(char));

            if(child_counter < 10) {
                sprintf(child_argv[0], "child_0%d", child_counter);
            } else {
                sprintf(child_argv[0], "child_%d", child_counter);
            }
            sprintf(child_argv[1], "%c", type);

        
            if (execve(child_path, child_argv, environ) == -1) {
                perror("Execve");
                exit(1);
            }
            break;

        default:
            child_counter++;
            break;
    }
    return;
 }

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

 void sorting(char** environ) {
    for(int i = 0; environ[i + 1]; i++) {
        for(int j = 0; environ[j + 1]; j++) {
            if(strcoll(environ[j], environ[j + 1]) > 0) {
                char* buf = environ[j + 1];
                environ[j + 1] = environ[j];
                environ[j] = buf;
            }
        }
    }
    return;
 }

int main(int argc, char** argv, char** envir) {
    if (argc < 2) {
        puts("Environment file path empty");
        exit(EXIT_FAILURE);
    }
    char childPathName[] = "CHILD_PATH";
    char *path = NULL,
         *environPath = NULL,
         *envirPath = NULL;
    setlocale(LC_COLLATE, "C");
    setenv("ENV_VAR", argv[1], 1);
    
    sorting(environ);
    for(int i = 0; environ[i]; i++) {
        puts(environ[i]);
    }

    path = getenv(childPathName);
    environPath = find(childPathName, environ);
    envirPath = find(childPathName, envir);

    while (1) {
        switch (getchar()) {
            case '+':
                init_child(path, '+');
                break;

            case '*':
                init_child(envirPath, '*');
                break;

            case '&':
                init_child(environPath, '&');
                break;

            case 'q':
                return 0;
                break;

            default:
                break;
        }
    }

    return 0;
}