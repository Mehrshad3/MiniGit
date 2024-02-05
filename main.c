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

#if MAX_FILENAME_LENGTH > MAX_PATH_LENGTH
#error "MAX_FILENAME_LENGTH cannot be greater than MAX_PATH_LENGTH"
#endif

#define IS_WHITE_SPACE(_C) (_C == ' ' || _C == '\t' || _C == '\n' || _C == '\r' || _C == '\f' || _C == '\v')

#define PROGRAM_NAME "MiniGit"
#define USERNAME "Asus"
#define GLOBAL_CONFIG "C:\\Users\\" USERNAME "\\." PROGRAM_NAME "Config"

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

void replace_characters_in_string(char* string, char to_replace, char replace_with) {
    while (*string != '\0') {
        if (*string == to_replace) {
            *string = replace_with;
        }
        string++;
    }
}

int insert_or_delete(FILE* file, char* absolute_path, char** lines, int* number_of_lines, int number_of_tabs, char* token, char* value_to_insert, char line[MAX_LINE_LENGTH], char* _Mode) {
    int new_num_lines = *number_of_lines;
    // printf("we're in insert_or_delete funct.\n");
    bool EOF_reached = false;
    if (_Mode[0] == 'd') {
        bool has_tabs = true;
        while (has_tabs && !(EOF_reached = !fgets(line, MAX_LINE_LENGTH, file))) {
            has_tabs = true;
            for (int i = 0; i <= number_of_tabs; i++) {
                if (line[i] != '\t') {
                    has_tabs = false;
                    break;
                }
            }
            if (!has_tabs) break;
        }
    }
    else {
        do {
            if (!(new_num_lines & 15)) lines = realloc(lines, (((new_num_lines) + 16)) * sizeof(char*));
            char new_line[MAX_LINE_LENGTH];
            memset(new_line, '\t', number_of_tabs);
            strcpy(new_line + number_of_tabs, token);
            if ((token = strtok(NULL, ".")) == NULL && value_to_insert) {
                int len = strlen(new_line + number_of_tabs);
                sprintf(new_line + number_of_tabs + len, ": %s\n", value_to_insert);
            }
            else {
                strcat(new_line, "\n");
            }
            lines[new_num_lines++] = malloc((strlen(new_line) + 1) * sizeof(char));
            strcpy(lines[new_num_lines - 1], new_line);
            number_of_tabs++;
        } while (token);
    }
    while (!EOF_reached) {
        if (!(new_num_lines & 15)) lines = realloc(lines, (((new_num_lines) + 16)) * sizeof(char*));
        lines[new_num_lines] = malloc((strlen(line) + 1) * sizeof(char));
        strcpy(lines[new_num_lines++], line);
        EOF_reached = fgets(line, MAX_LINE_LENGTH, file) == NULL;
    }
    if (fclose(file)) return 1;
    if (!(file = fopen(absolute_path, "w"))) return 1;
    for (int i = 0; i < new_num_lines; i++) {
        if (fputs(lines[i], file) == EOF) return 1;
    }
    if (fclose(file)) return 1;
    *number_of_lines = new_num_lines;
    return 0;
}

int read_write_minigit(char* path, char* element, char line[MAX_LINE_LENGTH], char* value_to_insert, char* _Mode) {
    // TODO : add the ability to write to this function, and other modes
    // modes: 'f' for find, 'i' for insert and edit, 'd' to delete, 'n' to go to next subitem, 't' to go to first subitem, 'c' to close file
    // If second character or _Mode is g, then it'll change global variables.
    // Unfortunately, line should always be passed to the function.

    static FILE* file = NULL;
    if (_Mode[0] == 'c') {
        bool return_value = fclose(file);
        file = NULL;
        return return_value;
    }
    bool file_exists = false;

    static int number_of_tabs = 0;
    char absolute_path[MAX_PATH_LENGTH];
    if (path != NULL || _Mode[1] == 'g') {
        number_of_tabs = 0;
        if (file != NULL) {
            fclose(file);
        }
        if (_Mode[1] != 'g') {
            if (path[1] != ':') sprintf(absolute_path , "%s\\." PROGRAM_NAME "\\%s", proj_dir, path);
            else strcpy(absolute_path, path);
        }
        else strcpy(absolute_path, GLOBAL_CONFIG);
        unsigned long attribute = GetFileAttributes(absolute_path);
        if (attribute != INVALID_FILE_ATTRIBUTES) file_exists = true;
        if (!file_exists && _Mode[1] == 'g') {
            perror("Global configuration file hasn't been set.");
        }
        if (!file_exists) {
            if (_Mode[0] != 'i') return 1;
            if ((file = fopen(absolute_path, "w")) == NULL) return 1;
            if (fclose(file)) return 1;
        }
        file = fopen(absolute_path, "r");
    }
    // printf("absolute path: %s\n", absolute_path);
    char** lines = NULL;
    int number_of_lines = 0;
    if (file == NULL) return 1;
    char* element_copy = NULL;
    if (element) {
        element_copy = (char*) malloc((strlen(element) + 1) * sizeof(char));
        strcpy(element_copy, element);
    }
    char* token = strtok(element_copy, ".");
    if (_Mode[0] == 'n') number_of_tabs--; // This number is increased in previous function call, so I turn it back.
    else if (_Mode[0] == 'f' || _Mode[0] == 'i' || _Mode[0] == 'd') number_of_tabs = 0;
    bool found = false;
    bool inserted_or_deleted = false;
    while (token || (_Mode[0] == 't' || _Mode[0] == 'n')) {
        if (fgets(line, MAX_LINE_LENGTH, file) == NULL) {
            if (_Mode[0] == 'i') {
                line[0] = '\0';
                inserted_or_deleted = !insert_or_delete(file, absolute_path, lines, &number_of_lines, number_of_tabs, token, value_to_insert, line, _Mode);
            }
            else if (fclose(file)) return 1;
            file = NULL;
            break;
        }
        found = false;
        for (int i = number_of_tabs - 1; i >= 0; i--) {
            if (line[i] != '\t') {
                line[0] = '\0';
                if (_Mode[0] == 'i') inserted_or_deleted = !insert_or_delete(file, absolute_path, lines, &number_of_lines, number_of_tabs, token, value_to_insert, line, _Mode);
                file = NULL;
                break;
            }
        }
        bool EOL = false, element = false;
        if (_Mode[0] == 't') {
            memmove(line, line + number_of_tabs, strlen(line + number_of_tabs) + 1);
            found = true;
        }
        else if (_Mode[0] == 'n') {
            if (line[number_of_tabs] != '\t') {
                memmove(line, line + number_of_tabs, strlen(line + number_of_tabs) + 1);
                found = true;
            }
        }
        else if (strncmp(line + number_of_tabs, token, strlen(token)) == 0) {
            char* token_copy = token;
            bool EOL = false, element = false;
            if (line[number_of_tabs + strlen(token)] == '\n') {
                EOL = true;
                if (_Mode[0] != 'd' && _Mode[0] != 'i') memmove(line, line + number_of_tabs, strlen(line + number_of_tabs) + 1);
            }
            else if (line[number_of_tabs + strlen(token)] == ':') {
                element = true;
                if (_Mode[0] != 'd' && _Mode[0] != 'i') memmove(line, line + number_of_tabs + strlen(token) + 2, strlen(line + number_of_tabs + strlen(token) + 1));
            }
            if (found = EOL | element) token = strtok(NULL, ".");
            if ((_Mode[0] == 'i' || _Mode[0] == 'd') && token == NULL) {
                line[0] = '\0';
                if (_Mode[0] == 'i') token = token_copy;
                inserted_or_deleted = !insert_or_delete(file, absolute_path, lines, &number_of_lines, number_of_tabs, token, value_to_insert, line, _Mode);
                file = NULL;
                break;
            }
        }
        if ((_Mode[0] == 'i' || _Mode[0] == 'd') && token) {
            if (!(number_of_lines & 15)) lines = realloc(lines, (((number_of_lines) + 16)) * sizeof(char*));
            lines[number_of_lines++] = malloc((strlen(line) + 1) * sizeof(char));
            strcpy(lines[number_of_lines - 1], line);
        }
        else line[number_of_tabs + strlen(line + number_of_tabs) - 1] = '\0';
        if (found) number_of_tabs++;
        if (_Mode[0] == 't' || _Mode[0] == 'n' && found) break;
    }
    for (int i = 0; i < number_of_lines; i++) {
        free(lines[i]);
    }
    free(lines);
    free(element_copy);
    // printf("line:%s\n", line);
    // printf("found:%d\n", found);
    return !(found || inserted_or_deleted);
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
            if (fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && strcmp(fdFile.cFileName, "."PROGRAM_NAME) == 0) {
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
        perror(PROGRAM_NAME " repository has already initialized.");
        return 0;
    }
    if (GetFileAttributes(GLOBAL_CONFIG) == INVALID_FILE_ATTRIBUTES) {
        FILE* gf = fopen(GLOBAL_CONFIG, "w");
        if (gf == NULL) return 1;
        fprintf(gf, "user\n");
        fprintf(gf, "\tname: \n");
        fprintf(gf, "\temail: \n");
        if (fclose(gf)) return 1;
    }
    char username[MAX_LINE_LENGTH];
    char email[MAX_LINE_LENGTH];
    if (read_write_minigit(NULL, "user.name", username, NULL, "fg")) return 1;
    fprintf(stdout, "username: %s\n", username);
    if (read_write_minigit(NULL, "user.email", email, NULL, "fg")) return 1;
    read_write_minigit(NULL, NULL, NULL, NULL, "c");
    char string[MAX_PATH_LENGTH];
    char line[MAX_LINE_LENGTH];
    sprintf(string, "projects.%s", cwd);
    replace_characters_in_string(string + 9, '.', '|');
    if (read_write_minigit(NULL, string, line, NULL, "ig")) return 1;
    printf("success\n");
    if (CreateDirectory("." PROGRAM_NAME, NULL) == 0) return 1;
    if (!SetFileAttributes("." PROGRAM_NAME, FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_HIDDEN)) return 1;
    strcpy(proj_dir, cwd);

    // Former create_files function
    FILE *file = fopen("." PROGRAM_NAME "\\config", "w");
    if (file == NULL) return 1;

    fprintf(file, "user\n");
    fprintf(file, "\tname: %s\n", username);
    fprintf(file, "\temail: %s\n", email);
    fprintf(file, "last_commit_ID: %d\n", 1000000);
    fprintf(file, "current_commit_ID: %d\n", 1000000);
    fprintf(file, "branch: %s\n", "main");

    fclose(file);
    // create commits folder
    if (mkdir("." PROGRAM_NAME "\\commits") != 0) return 1;

    // create files folder
    if (mkdir("." PROGRAM_NAME "\\files") != 0) return 1;

    file = fopen("." PROGRAM_NAME "\\staging", "w");
    fclose(file);

    file = fopen("." PROGRAM_NAME "\\tracks", "w");
    fclose(file);

    return 0;
}

int run_config(int argc, char* const argv[]) {
    bool global = false;
    if (argc >= 3) global = !strcmp(argv[2], "-global");
    if (!global) {
        if (find_minigit_directory()) return 1;
    }
    if (argc < 3 + (int)global) {
        perror("please specify something to config.");
        return 1;
    }
    char value[MAX_LINE_LENGTH];
    char line[MAX_LINE_LENGTH];
    if (argc == 3 + (int)global) {
        value[0] = '\0';
        if (!global) {
            if (read_write_minigit("config", argv[2], line, value, "f")) {
                perror("this config hasn't been set");
            }
            else printf("%s\n", line);
        }
        else {
            if (read_write_minigit(NULL, argv[3], line, value, "fg")) {
                perror("this config hasn't been set");
            }
            else printf("%s\n", line);
        }
    }
    if (argc == 4 + (int)global) {
        if (global) {
            char line[MAX_LINE_LENGTH];
            if (read_write_minigit(NULL, argv[3], line, argv[4], "ig")) return 1;
            if (!read_write_minigit(NULL, "projects", line, NULL, "fg")) {
                if (!read_write_minigit(NULL, NULL, line, NULL, "t")) {
                    char** projects_to_override = NULL;
                    int number_of_overrides = 0;
                    do {
                        char path[MAX_PATH_LENGTH];
                        sprintf(path, "%s\\." PROGRAM_NAME, line);
                        unsigned long attributes= GetFileAttributes(path);
                        if (attributes != INVALID_FILE_ATTRIBUTES) {
                            if (!(number_of_overrides & 15)) projects_to_override = realloc(projects_to_override, ((number_of_overrides + 16)) * sizeof(char*));
                            projects_to_override[number_of_overrides] = (char*) malloc((strlen(line) + 1) * sizeof(char));
                            strcpy(projects_to_override[number_of_overrides++], line);
                        }
                    } while (!read_write_minigit(NULL, NULL, line, NULL, "n"));
                    printf("%d\n", number_of_overrides);
                    for (int i = 0; i < number_of_overrides; i++) {
                        char path[MAX_LINE_LENGTH];
                        sprintf(path, "%s\\.MiniGit\\config", projects_to_override[i]);
                        fprintf(stdout, "%s\n", path);
                        if (read_write_minigit(path, argv[3], line, argv[4], "i")) {
                            char error[MAX_MESSAGE_LENGTH];
                            sprintf(error, "Project %s is corrupted", projects_to_override[i]);
                            perror(error);
                        }
                    }
                    for (int i = 0; i < number_of_overrides; i++) {
                        char key[MAX_LINE_LENGTH];
                        replace_characters_in_string(projects_to_override[i], '|', '.');
                        sprintf(key, "projects.%s", projects_to_override[i]);
                        if (read_write_minigit(NULL, key, line, NULL, "i")) return 1;
                        free(projects_to_override[i]);
                    }
                    // TODO: increase performance of loop above by passing a file with mode "r+" to read_write_minigit function
                    free(projects_to_override);
                }
            }
        }
        else {
            if (read_write_minigit("config", argv[2], line, argv[3], "i")) return 1;
        }
    }
    printf("%s\n", cwd);
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
        //char* argv2[] = {"main", "config", "-global", "user.name", "Mehr"};
        //return run_config(sizeof(argv2) / sizeof(argv2[0]), argv2);
        //char* argv3[] = {"main", "init"};
        //return run_init(sizeof(argv3) / sizeof(char*), argv3);
        fprintf(stdout, "too few arguments to program '" PROGRAM_NAME "'");
        return 1;
    }

    print_command(argc, argv); // Not officially, just used for developing, and debuging

    if (strcmp(argv[1], "init") != 0 && strcmp(argv[1], "config") != 0) {
        if (find_minigit_directory() != 0) {
            perror(PROGRAM_NAME " repository has'n initialized yet.");
            return 1;
        }
    }

    if (strcmp(argv[1], "init") == 0) {
        return run_init(argc, argv);
    }
    else if (strcmp(argv[1], "test") == 0) {
        if (find_minigit_directory() != 0) {
            perror(PROGRAM_NAME " repository has'n initialized yet.");
            return 1;
        }
        char line[MAX_LINE_LENGTH];
        char line2[MAX_LINE_LENGTH];
        do {
            fgets(line2, sizeof(line2), stdin);
            if (!strcmp(line2, "end\n")) break;
            if (!strcmp(line2, "end")) break;
            char t1[MAX_LINE_LENGTH];
            char t2[MAX_LINE_LENGTH];
            char t3[MAX_LINE_LENGTH];
            char t4[MAX_LINE_LENGTH];
            sscanf(line2, "%s %s %s %s", t1, t2, t3, t4);
            char *s1 = t1, *s2 = t2, *s3 = t3, *s4 = t4;
            if (!strcmp(s1, "NULL")) s1 = NULL;
            if (!strcmp(s2, "NULL")) s2 = NULL;
            if (!strcmp(s3, "NULL")) s3 = NULL;
            if (!strcmp(s4, "NULL")) s4 = NULL;
            int failure = read_write_minigit(s1, s2, line, s3, s4);
            printf("%d\n", failure);
            printf("%.40s\n", line);
        } while(true);
        read_write_minigit(NULL, NULL, NULL, NULL, "c");
        printf("%s\n", line);
        return 0;
    }
    else if (strcmp(argv[1], "config") == 0) {
        run_config(argc, argv);
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
