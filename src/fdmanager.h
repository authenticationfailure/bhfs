/*
  BHFS: Black Hole Filesystem
  
  Copyright (C) 2017-2023	   David Turco  	

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#include <stdlib.h>
#include <stdio.h>

struct bhfs_open_file {
    int fh;
    size_t size; // not currently used
    int pid;
    struct bhfs_open_file *next;
    pthread_mutex_t mutex; // used by whfs
};

struct bhfs_open_file *bhfs_new_open_file();

void bhfs_free_open_file(struct bhfs_open_file *new_open_file);

void bhfs_f_list_append(struct bhfs_open_file *new_open_file);

struct bhfs_open_file *bhfs_f_list_get(int fd);

void bhfs_f_list_delete(struct bhfs_open_file *open_file_to_delete);
