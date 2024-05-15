#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

#define MAX_PATHS 1024 

char *paths[MAX_PATHS]; 
long unsigned int paths_count = 0; 

int lstat();
int getopt();

char * check_for_path(int argc, char * argv[]){
    long unsigned int path_size = 0;
    char * path = (char *)malloc(path_size*sizeof(char));
    for(int i = 1; i < argc; i++){
        if(argv[i][0] != '-'){
            for(int j = 0; argv[i][j] != '\0'; j++){
                path_size++;
                path = (char *)realloc(path, path_size*sizeof(char));
                path[j] = argv[i][j];
            }
            return path;
        }
    }
    printf("No path entered, yet.\n");
    return NULL;
}

int char_equals(char * char_1, char * char_2){
    int i = 0;
    for(i = 0; char_1[i] != '\0' || char_2[i] != '\0'; i++){
        if(char_1[i] != char_2[i]){
            return 0;
        }
    }
    if(char_1[i] == char_2[i] && char_2[i] == '\0'){
        return 1;
    }
    else{
        return 0;
    }
}

char * get_new_path(char * old_path, char * new_element){
    long unsigned int new_path_size = 0;
    char * new_path = (char *)malloc(new_path_size*sizeof(char));
    int i;
    for(i = 0; old_path[i] != '\0'; i++){
        new_path_size++;
        new_path = (char *)realloc(new_path, new_path_size*sizeof(char));
        new_path[i] = old_path[i];
    }
    if(new_path[i-1] != '/'){
        new_path_size++;
        new_path = (char *)realloc(new_path, new_path_size*sizeof(char));
        new_path[new_path_size-1] = '/';
        i++;
    }
    for(int j = 0; new_element[j] != '\0'; j++){
        new_path_size++;
        new_path = (char *)realloc(new_path, new_path_size*sizeof(char));
        new_path[i] = new_element[j];
        i++;
    }
    return new_path;
}


void dir_walking(char * path, int * opt_flags){
    DIR *dir = opendir(path);
    if(!dir){
        perror("Error opening path\n");
        return;
    }

    struct dirent * entry;
    while((entry = readdir(dir)) != NULL){
        if(char_equals(entry->d_name, ".") || char_equals(entry->d_name, "..")){
            continue;
        }

        char * newPath = get_new_path(path, entry->d_name);
        struct stat info;
        if(lstat(newPath, &info) == 0){
            if(S_ISDIR(info.st_mode)){
                paths[paths_count++] = newPath;
                dir_walking(newPath, opt_flags);
            }
            else if(S_ISLNK(info.st_mode) && opt_flags[0] == 1){
                paths[paths_count++] = newPath;
            }
            else if(S_ISREG(info.st_mode) && opt_flags[2] == 1){
                paths[paths_count++] = newPath;
            }
        }
    }
    closedir(dir);
}

int main(int argc, char * argv[]){
    int opt_index;
    int opt_flags[4] = {0, 0, 0, 0};
    int no_opt = 1;
    while((opt_index = getopt(argc, argv, ":ldfs")) != -1){
        switch (opt_index){
        case 'l':
            opt_flags[0] = 1;
            no_opt = 0;
            break;
        case 'd':
            opt_flags[1] = 1;
            no_opt = 0;
            break;
        case 'f':
            opt_flags[2] = 1;
            no_opt = 0;
            break;
        case 's':
            opt_flags[3] = 1;
            no_opt = 0;
            break;
        case '?':
            printf("Wrong parametres!\n");
            return 0;
        }
    }

    if(no_opt){
        for(int i = 0; i < 4; i++){
            opt_flags[i] = 1;
        }
    }

    char * path = check_for_path(argc, argv);
    if(path == NULL){
        return 0;
    }
    
    dir_walking(path, opt_flags);

    if(opt_flags[3] == 1){
        qsort(paths, paths_count, sizeof(char *), (int (*)(const void *, const void *))strcoll);
    }

    for(long unsigned int i = 0; i < paths_count; i++){
        printf("%s\n", paths[i]);
    }

    return 0;
}