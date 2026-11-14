
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define MAX_INODES 1024
#define NAME_SIZE 32

typedef struct {
    uint32_t inode;
    uint32_t parent;
    char type;
    char name[NAME_SIZE];
} Inode;
int save_inodes(Inode inodes[], int used[]) {
    FILE *file = fopen("inodes_list", "wb");
    if (file == NULL) {
        perror("Could not save inodes_list");
        return 1;
    }

    for (int i = 0; i < MAX_INODES; i++) {
        if (used[i] && fwrite(&inodes[i], sizeof(Inode), 1, file) != 1) {
            perror("Error writing inodes_list");
            fclose(file);
            return 1;
        }
    }
    if (fclose(file) != 0) {
        perror("Error closing inodes_list");
        return 1;
    }

    return 0;
}
int main(int argc, char *argv[]) {
    Inode inodes[MAX_INODES] = {0};
    int used[MAX_INODES] = {0};
    uint32_t current = 0;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s filesystem_directory\n", argv[0]);
        return 1;
    }

    if (chdir(argv[1]) != 0) {
        perror("Could not open filesystem directory");
        return 1;
    }

    FILE *file = fopen("inodes_list", "rb");
    if (file == NULL) {
        perror("Could not open inodes_list");
        return 1;
    }

    Inode entry;
    while (fread(&entry, sizeof(entry), 1, file) == 1) {
        if (entry.inode >= MAX_INODES) {
            fprintf(stderr, "Ignoring invalid inode number: %u\n", entry.inode);
            continue;
        }
	if (entry.parent >= MAX_INODES) {
   	 fprintf(stderr, "Ignoring invalid parent inode for inode %u\n",
            entry.inode);
   	 continue;
}

        if (entry.type != 'd' && entry.type != 'f') {
            fprintf(stderr, "Ignoring invalid type for inode %u\n", entry.inode);
            continue;
        }

        inodes[entry.inode] = entry;
        used[entry.inode] = 1;
    }

    if (ferror(file)) {
        perror("Error reading inodes_list");
        fclose(file);
        return 1;
    }

    fclose(file);

    if (!used[0] || inodes[0].type != 'd') {
        fprintf(stderr, "Error: inode 0 must be an in-use directory.\n");
        return 1;
    }

   char line[128];

while (1) {
    char *command;
    char *argument;
    char *extra;

    printf("> ");

    if (fgets(line, sizeof(line), stdin) == NULL) {
        break;
    }

    command = strtok(line, " \t\n");
    if (command == NULL) {
        continue;
    }

    argument = strtok(NULL, " \t\n");
    extra = strtok(NULL, " \t\n");

    if (strcmp(command, "exit") == 0) {
        if (argument != NULL) {
            printf("Usage: exit\n");
            continue;
        }
        break;
    } else if (strcmp(command, "ls") == 0) {
        if (argument != NULL) {
            printf("Usage: ls\n");
            continue;
        }

        for (int i = 0; i < MAX_INODES; i++) {
            if (used[i] && inodes[i].parent == current) {
                printf("inode: %u, type: %c, name: %.*s\n",
                       inodes[i].inode,
                       inodes[i].type,
                       NAME_SIZE,
                       inodes[i].name);
            }
        }
    } else if (strcmp(command, "cd") == 0) {
        if (argument == NULL || extra != NULL) {
            printf("Usage: cd <name>\n");
            continue;
        }

        if (strcmp(argument, "..") == 0) {
            current = inodes[current].parent;
            continue;
        }

        int found = 0;

        for (int i = 0; i < MAX_INODES; i++) {
            if (used[i] &&
                inodes[i].parent == current &&
                inodes[i].type == 'd' &&
                strncmp(inodes[i].name, argument, NAME_SIZE) == 0) {
                current = inodes[i].inode;
                found = 1;
                break;
            }
        }

        if (!found) {
            printf("Error: directory not found.\n");
        }
    } else if (strcmp(command, "mkdir") == 0) {
        if (argument == NULL || extra != NULL) {
            printf("Usage: mkdir <name>\n");
            continue;
        }

        if (strlen(argument) > NAME_SIZE) {
            printf("Error: name must be at most 32 characters.\n");
            continue;
        }

        int duplicate = 0;
        for (int i = 0; i < MAX_INODES; i++) {
            if (used[i] &&
                inodes[i].parent == current &&
                strncmp(inodes[i].name, argument, NAME_SIZE) == 0) {
                duplicate = 1;
                break;
            }
        }

        if (duplicate) {
            printf("Error: that name already exists.\n");
            continue;
        }

        int new_inode = -1;
        for (int i = 1; i < MAX_INODES; i++) {
            if (!used[i]) {
                new_inode = i;
                break;
            }
        }

        if (new_inode == -1) {
            printf("Error: no free inodes.\n");
            continue;
        }

        char inode_name[16];
        snprintf(inode_name, sizeof(inode_name), "%d", new_inode);

        FILE *directory_file = fopen(inode_name, "w");
        if (directory_file == NULL) {
            perror("Could not create directory file");
            continue;
        }

        int write_failed =
            fprintf(directory_file, "%d .\n%u ..\n",
                    new_inode, current) < 0;

        if (fclose(directory_file) != 0) {
            write_failed = 1;
        }

        if (write_failed) {
            fprintf(stderr, "Error writing new directory entries.\n");
            remove(inode_name);
            continue;
        }

        Inode new_entry = {0};
        new_entry.inode = new_inode;
        new_entry.parent = current;
        new_entry.type = 'd';
        strncpy(new_entry.name, argument, NAME_SIZE);

        inodes[new_inode] = new_entry;
        used[new_inode] = 1;
    } else if (strcmp(command, "touch") == 0) {
        if (argument == NULL || extra != NULL) {
            printf("Usage: touch <name>\n");
            continue;
        }

        /* Store no more than the allowed 32 characters. */
        char name[NAME_SIZE + 1];
        strncpy(name, argument, NAME_SIZE);
        name[NAME_SIZE] = '\0';

        int duplicate = 0;
        for (int i = 0; i < MAX_INODES; i++) {
            if (used[i] &&
                inodes[i].parent == current &&
                strncmp(inodes[i].name, name, NAME_SIZE) == 0) {
                duplicate = 1;
                break;
            }
        }

        /* Per the assignment, an existing name makes touch do nothing. */
        if (duplicate) {
            continue;
        }

        int new_inode = -1;
        for (int i = 1; i < MAX_INODES; i++) {
            if (!used[i]) {
                new_inode = i;
                break;
            }
        }

        if (new_inode == -1) {
            printf("Error: no free inodes.\n");
            continue;
        }

        char inode_name[16];
        snprintf(inode_name, sizeof(inode_name), "%d", new_inode);

        FILE *regular_file = fopen(inode_name, "w");
        if (regular_file == NULL) {
            perror("Could not create file");
            continue;
        }

        if (fputs(name, regular_file) == EOF) {
            perror("Could not write file name");
            fclose(regular_file);
            remove(inode_name);
            continue;
        }

        if (fclose(regular_file) != 0) {
            perror("Could not close file");
            remove(inode_name);
            continue;
        }

        Inode new_entry = {0};
        new_entry.inode = new_inode;
        new_entry.parent = current;
        new_entry.type = 'f';
        strncpy(new_entry.name, name, NAME_SIZE);

        inodes[new_inode] = new_entry;
        used[new_inode] = 1;
    } else {
	    printf("Error: unknown command.\n");
	}
}
if (save_inodes(inodes, used) != 0) {
    return 1;
}
return 0;
}
