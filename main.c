#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <windows.h>

#define MAX_FILENAME_LENGTH 1000
#define MAX_COMMIT_MESSAGE_LENGTH 2000
#define MAX_LINE_LENGTH 1000
#define MAX_MESSAGE_LENGTH 1000
#define MAX_PATH_LENGTH 1024 // literally it's 260, IDK why I'm setting this.

#define IS_WHITE_SPACE(_C) (_C == ' ' || _C == '\t' || _C == '\n' || _C == '\r' || _C == '\f' || _C == '\v')

#define debug(x) printf("%s", x);

char cwd[MAX_PATH_LENGTH];
char proj_dir[MAX_PATH_LENGTH];

void print_command(int argc, char * const argv[]);

int run_init(int argc, char * const argv[]);

int run_add(int argc, char * const argv[]);
int add_to_staging(char *filepath);

int run_reset(int argc, char * const argv[]);
int remove_from_staging(char *filepath);

int run_commit(int argc, char * const argv[]);
int inc_last_commit_ID();
bool check_file_directory_exists(char *filepath);
int commit_staged_file(int commit_ID, char *filepath);
int track_file(char *filepath);
bool is_tracked(char *filepath);
int create_commit_file(int commit_ID, char *message);
int find_file_last_commit(char* filepath);

int run_checkout(int argc, char *const argv[]);
int find_file_last_change_before_commit(char *filepath, int commit_ID);
int checkout_file(char *filepath, int commit_ID);

void print_command(int argc, char * const argv[]) {
    for (int i = 0; i < argc; i++) {
        fprintf(stdout, "%s ", argv[i]);
    }
    fprintf(stdout, "\n");
}

int read_write_minigit(char* path, char* element, char line[MAX_LINE_LENGTH], char* _Mode) {
    // TODO : add the ability to write to this function
    int number_of_tabs = 0;
    char absolute_path[MAX_PATH_LENGTH];
    sprintf(absolute_path , "%s\\.MiniGit\\%s", proj_dir, path);
    printf("%s\n", absolute_path);
    unsigned long attribute = GetFileAttributes(absolute_path);
    if (attribute == INVALID_FILE_ATTRIBUTES) return 1;
    FILE* file = fopen(absolute_path, _Mode);
    if (file == NULL) return 1;
    char* token = strtok(element, ".");
    bool found = false;
    while (token && !found) {
        if (fgets(line, MAX_LINE_LENGTH, file) == NULL) {
            fclose(file);
            return 1;
        }
        printf("%s", line);
        for (int i = number_of_tabs - 1; i >= 0; i--) {
            if (line[i] != '\t') {
                fclose(file);
                return 1;
            }
        }
        if (strncmp(line + number_of_tabs, token, strlen(token)) == 0) {
            if (line[number_of_tabs + strlen(token)] == '\n') {
                token = strtok(NULL, ".");
                number_of_tabs++;
            }
            else if (line[number_of_tabs + strlen(token)] == ':') {
                memmove(line, line + number_of_tabs + strlen(token) + 2, strlen(line + number_of_tabs + strlen(token) + 1));
                found = true;
            }
        }
    }
    fclose(file);
    return !found;
}

int find_minigit_directory() {
    if (getcwd(cwd, sizeof(cwd)) == NULL) return 1;
    memcpy(proj_dir, cwd, sizeof(cwd));
    char spath[MAX_PATH_LENGTH];
    bool exists = false;
    bool drive_reached = false;
    do {
        drive_reached = drive_reached || (strcmp(proj_dir + 1, ":\\") == 0);
        char trash;
        // find .neogit
        WIN32_FIND_DATA fdFile; // this data type stores file attributes
        strcpy(spath, proj_dir);
        strcat(spath, "\\*");
        HANDLE handle; // handle is something like a pointer to a file, IDK maybe similar to struct dirent in Linux
        if ((handle = FindFirstFile(spath, &fdFile)) == INVALID_HANDLE_VALUE) {
            perror("Error opening current directory");
            perror("If current directory path has Persian characters, opening has not been implemented.");
            return 1;
        }
        do {
            if (fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && strcmp(fdFile.cFileName, ".MiniGit") == 0) {
                exists = true;
            }
        } while (FindNextFile(handle, &fdFile));
        FindClose(handle);

        // change cwd to parent
        if (!drive_reached && !exists) {
            if (chdir("..") != 0) return 1;
        }

        // update current working directory
        if (getcwd(proj_dir, sizeof(proj_dir)) == NULL) return 1;

    } while (!drive_reached && !exists);
    
    if (chdir(cwd) != 0) return 1;
    if (!exists) proj_dir[0] = '\0';
    return 0;
}

int run_init(int argc, char * const argv[]) {
    if (find_minigit_directory()) return 1;
    // return to the initial cwd
    if (proj_dir[0]) {
        perror("MiniGit repository has already initialized.");
        return 0;
    }
    if (CreateDirectory(".MiniGit", NULL) == 0) return 1;
    if (!SetFileAttributes(".MiniGit", FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_HIDDEN)) return 1;
    strcpy(proj_dir, cwd);

    // Former create_files function
    FILE *file = fopen(".MiniGit/config", "w");
    if (file == NULL) return 1;

    fprintf(file, "username:\n");
    fprintf(file, "email:\n");
    fprintf(file, "last_commit_ID: %d\n", 0);
    fprintf(file, "current_commit_ID: %d\n", 0);
    fprintf(file, "branch: %s\n", "master");

    fclose(file);
    // create commits folder
    if (mkdir(".MiniGit/commits") != 0) return 1;

    // create files folder
    if (mkdir(".MiniGit/files") != 0) return 1;

    file = fopen(".MiniGit/staging", "w");
    fclose(file);

    file = fopen(".MiniGit/tracks", "w");
    fclose(file);

    return 0;
}

/*int run_add(int argc, char *const argv[]) {
    // TODO: handle command in non-root directories 
    if (argc < 3) {
        perror("please specify a file");
        return 1;
    }

    return add_to_staging(argv[2]);
}

int add_to_staging(char *filepath) {
    FILE *file = fopen(".neogit/staging", "r");
    if (file == NULL) return 1;
    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file) != NULL) {
        int length = strlen(line);

        // remove '\n'
        if (length > 0 && line[length - 1] == '\n') {
            line[length - 1] = '\0';
        }

        if (strcmp(filepath, line) == 0) return 0;
    }
    fclose(file);
    
    file = fopen(".neogit/staging","a");
    if (file == NULL) return 1;

    fprintf(file, "%s\n", filepath);
    fclose(file);

    return 0;
}

int run_reset(int argc, char *const argv[]) {
    // TODO: handle command in non-root directories 
    if (argc < 3) {
        perror("please specify a file");
        return 1;
    }
    
    return remove_from_staging(argv[2]);
}

int remove_from_staging(char *filepath) {
    FILE *file = fopen(".neogit/staging", "r");
    if (file == NULL) return 1;
    
    FILE *tmp_file = fopen(".neogit/tmp_staging", "w");
    if (tmp_file == NULL) return 1;

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file) != NULL) {
        int length = strlen(line);

        // remove '\n'
        if (length > 0 && line[length - 1] == '\n') {
            line[length - 1] = '\0';
        }

        if (strcmp(filepath, line) != 0) fputs(line, tmp_file);
    }
    fclose(file);
    fclose(tmp_file);

    remove(".neogit/staging");
    rename(".neogit/tmp_staging", ".neogit/staging");
    return 0;
}

int run_commit(int argc, char * const argv[]) {
    if (argc < 4) {
        perror("please use the correct format");
        return 1;
    }
    
    char message[MAX_MESSAGE_LENGTH];
    strcpy(message, argv[3]);

    int commit_ID = inc_last_commit_ID();
    if (commit_ID == -1) return 1;
    
    FILE *file = fopen(".neogit/staging", "r");
    if (file == NULL) return 1;
    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file) != NULL) {
        int length = strlen(line);

        // remove '\n'
        if (length > 0 && line[length - 1] == '\n') {
            line[length - 1] = '\0';
        }
        
        if (!check_file_directory_exists(line)) {
            char dir_path[MAX_FILENAME_LENGTH];
            strcpy(dir_path, ".neogit/files/");
            strcat(dir_path, line);
            if (mkdir(dir_path, 0755) != 0) return 1;
        }
        printf("commit %s\n", line);
        commit_staged_file(commit_ID, line);
        track_file(line);
    }
    fclose(file); 
    
    // free staging
    file = fopen(".neogit/staging", "w");
    if (file == NULL) return 1;
    fclose(file);

    create_commit_file(commit_ID, message);
    fprintf(stdout, "commit successfully with commit ID %d", commit_ID);
    
    return 0;
}

// returns new commit_ID
int inc_last_commit_ID() {
    FILE *file = fopen(".neogit/config", "r");
    if (file == NULL) return -1;
    
    FILE *tmp_file = fopen(".neogit/tmp_config", "w");
    if (tmp_file == NULL) return -1;

    int last_commit_ID;
    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strncmp(line, "last_commit_ID", 14) == 0) {
            sscanf(line, "last_commit_ID: %d\n", &last_commit_ID);
            last_commit_ID++;
            fprintf(tmp_file, "last_commit_ID: %d\n", last_commit_ID);

        } else fprintf(tmp_file, "%s", line);
    }
    fclose(file);
    fclose(tmp_file);

    remove(".neogit/config");
    rename(".neogit/tmp_config", ".neogit/config");
    return last_commit_ID;
}

bool check_file_directory_exists(char *filepath) {
    DIR *dir = opendir(".neogit/files");
    struct dirent *entry;
    if (dir == NULL) {
        perror("Error opening current directory");
        return 1;
    }
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, filepath) == 0) return true;
    }
    closedir(dir);

    return false;
}

int commit_staged_file(int commit_ID, char* filepath) {
    FILE *read_file, *write_file;
    char read_path[MAX_FILENAME_LENGTH];
    strcpy(read_path, filepath);
    char write_path[MAX_FILENAME_LENGTH];
    strcpy(write_path, ".neogit/files/");
    strcat(write_path, filepath);
    strcat(write_path, "/");
    char tmp[10];
    sprintf(tmp, "%d", commit_ID);
    strcat(write_path, tmp);

    read_file = fopen(read_path, "r");
    if (read_file == NULL) return 1;

    write_file = fopen(write_path, "w");
    if (write_file == NULL) return 1;

    char buffer;
    buffer = fgetc(read_file);
    while(buffer != EOF) {
        fputc(buffer, write_file);
        buffer = fgetc(read_file);
    }
    fclose(read_file);
    fclose(write_file);

    return 0;
}

int track_file(char *filepath) {
    if (is_tracked(filepath)) return 0;

    FILE *file = fopen(".neogit/tracks", "a");
    if (file == NULL) return 1;
    fprintf(file, "%s\n", filepath);
    return 0;
}

bool is_tracked(char *filepath) {
    FILE *file = fopen(".neogit/tracks", "r");
    if (file == NULL) return false;
    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file) != NULL) {
        int length = strlen(line);

        // remove '\n'
        if (length > 0 && line[length - 1] == '\n') {
            line[length - 1] = '\0';
        }
        
        if (strcmp(line, filepath) == 0) return true;

    }
    fclose(file); 

    return false;
}

int create_commit_file(int commit_ID, char *message) {
    char commit_filepath[MAX_FILENAME_LENGTH];
    strcpy(commit_filepath, ".neogit/commits/");
    char tmp[10];
    sprintf(tmp, "%d", commit_ID);
    strcat(commit_filepath, tmp);

    FILE *file = fopen(commit_filepath, "w");
    if (file == NULL) return 1;

    fprintf(file, "message: %s\n", message);
    fprintf(file, "files:\n");
    
    DIR *dir = opendir(".");
    struct dirent *entry;
    if (dir == NULL) {
        perror("Error opening current directory");
        return 1;
    }
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && is_tracked(entry->d_name)) {
            int file_last_commit_ID = find_file_last_commit(entry->d_name);
            fprintf(file, "%s %d\n", entry->d_name, file_last_commit_ID);
        }
    }
    closedir(dir); 
    fclose(file);
    return 0;
}

int find_file_last_commit(char* filepath) {
    char filepath_dir[MAX_FILENAME_LENGTH];
    strcpy(filepath_dir, ".neogit/files/");
    strcat(filepath_dir, filepath);

    int max = -1;
    
    DIR *dir = opendir(filepath_dir);
    struct dirent *entry;
    if (dir == NULL) return 1;

    while((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            int tmp = atoi(entry->d_name);
            max = max > tmp ? max: tmp;
        }
    }
    closedir(dir);

    return max;
}

int run_checkout(int argc, char * const argv[]) {
    if (argc < 3) return 1;
    
    int commit_ID = atoi(argv[2]);

    DIR *dir = opendir(".");
    struct dirent *entry;
    while((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && is_tracked(entry->d_name)) {
            checkout_file(entry->d_name, find_file_last_change_before_commit(entry->d_name, commit_ID));
        }
    }
    closedir(dir);

    return 0;
}

int find_file_last_change_before_commit(char *filepath, int commit_ID) {
    char filepath_dir[MAX_FILENAME_LENGTH];
    strcpy(filepath_dir, ".neogit/files/");
    strcat(filepath_dir, filepath);

    int max = -1;
    
    DIR *dir = opendir(filepath_dir);
    struct dirent *entry;
    if (dir == NULL) return 1;

    while((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            int tmp = atoi(entry->d_name);
            if (tmp > max && tmp <= commit_ID) {
                max = tmp;
            }
        }
    }
    closedir(dir);

    return max;
}

int checkout_file(char *filepath, int commit_ID) {
    char src_file[MAX_FILENAME_LENGTH];
    strcpy(src_file, ".neogit/files/");
    strcat(src_file, filepath);
    char tmp[10];
    sprintf(tmp, "/%d", commit_ID);
    strcat(src_file, tmp);

    FILE *read_file = fopen(src_file, "r");
    if (read_file == NULL) return 1;
    FILE *write_file = fopen(filepath, "w");
    if (write_file == NULL) return 1;
    
    char line[MAX_LINE_LENGTH];

    while (fgets(line, sizeof(line), read_file) != NULL) {
        fprintf(write_file, "%s", line);
    }
    
    fclose(read_file);
    fclose(write_file);

    return 0;
}*/

int main(int argc, char *argv[]) {

    if (argc < 2) {
        fprintf(stdout, "too few arguments to program 'MiniGit'");
        return 1;
    }

    print_command(argc, argv); // Not officially, just used for developing, and debuging

    if (strcmp(argv[1], "init") != 0 && strcmp(argv[1], "config") != 0) {
        if (find_minigit_directory() != 0) {
            perror("MiniGit repository has'n initialized yet.");
            return 1;
        }
    }

    if (strcmp(argv[1], "init") == 0) {
        return run_init(argc, argv);
    }
    else if (strcmp(argv[1], "test") == 0) {
        if (find_minigit_directory() != 0) {
            perror("MiniGit repository has'n initialized yet.");
            return 1;
        }
        char line[MAX_LINE_LENGTH];
        printf("%d\n", read_write_minigit(argv[2], argv[3], line, "r"));
        printf("%s\n", line);
        return 0;
    }
    /* else if (strcmp(argv[1], "add") == 0) {
        return 0;run_add(argc, argv);
    } else if (strcmp(argv[1], "reset") == 0) {
        return run_reset(argc, argv);
    } else if (strcmp(argv[1], "commit") == 0) {
        return run_commit(argc, argv);
    } else if (strcmp(argv[1], "checkout") == 0) {
        return run_checkout(argc, argv);
    }*/
    
    return 0;
}
