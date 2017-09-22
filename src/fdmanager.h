#include <stdlib.h>
#include <stdio.h>

struct bhfs_open_file {
    int fh;
    size_t size; // not currently used
    int pid;
    struct bhfs_open_file *next;
};

struct bhfs_open_file *bhfs_new_open_file();

void bhfs_free_open_file(struct bhfs_open_file *new_open_file);

void bhfs_f_list_append(struct bhfs_open_file *new_open_file);

struct bhfs_open_file *bhfs_f_list_get(uint64_t fd);

void bhfs_f_list_delete(struct bhfs_open_file *open_file_to_delete);