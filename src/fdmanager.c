#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "fdmanager.h"

struct bhfs_open_file *bhfs_f_list;
pthread_mutex_t bhfs_f_list_mutex; //FUSE is mutithreaded

struct bhfs_open_file *bhfs_new_open_file() {

    struct bhfs_open_file *new_file;
    
    new_file = malloc(sizeof(struct bhfs_open_file));
    new_file->fh = 0;
    new_file->size = 0;
    new_file->pid = 0;
    new_file->next = NULL;

    return new_file;
} 

void bhfs_free_open_file(struct bhfs_open_file *new_open_file) {
    free(new_open_file);
}

void bhfs_f_list_append(struct bhfs_open_file *new_open_file) {
    
    pthread_mutex_lock(&bhfs_f_list_mutex);
    new_open_file->next = bhfs_f_list;
    bhfs_f_list = new_open_file;
    pthread_mutex_unlock(&bhfs_f_list_mutex);
}

struct bhfs_open_file *bhfs_f_list_get(uint64_t fd) {
    pthread_mutex_lock(&bhfs_f_list_mutex);

    struct bhfs_open_file *cur_open_file;
    if (bhfs_f_list == NULL) return NULL;
    
    cur_open_file = bhfs_f_list;
    do {
        if (cur_open_file->fh == fd) return cur_open_file;
        cur_open_file = cur_open_file->next;
    } while (cur_open_file != NULL);

    pthread_mutex_lock(&bhfs_f_list_mutex);
    return NULL;
}

void bhfs_f_list_delete(struct bhfs_open_file *open_file_to_delete) {
    struct bhfs_open_file *next_open_file;

    pthread_mutex_lock(&bhfs_f_list_mutex);
    if (bhfs_f_list == NULL) return;

    next_open_file = bhfs_f_list;

    // Is it the last file?
    if (next_open_file == open_file_to_delete) {
        bhfs_f_list = NULL;
        return;
    }

    while (next_open_file->next != NULL && 
           next_open_file->next != open_file_to_delete) {
            next_open_file = next_open_file->next;
    }

    if (next_open_file->next == open_file_to_delete) {
        next_open_file->next = open_file_to_delete->next;
    }

    pthread_mutex_lock(&bhfs_f_list_mutex);
}

/*
int main(int argc, char *argv[]) {
    struct bhfs_open_file *openfile;
    struct bhfs_open_file *retrieved_openfile;
 
    openfile = bhfs_new_open_file();
    openfile->fh = 12;
    openfile->pid = 1200;

    bhfs_f_list_append(openfile);

    openfile = bhfs_new_open_file();
    openfile->fh = 13;
    openfile->pid = 1300;

    struct bhfs_open_file *openfile_to_delete;
    openfile_to_delete = openfile;
    
    bhfs_f_list_append(openfile);

    openfile = bhfs_new_open_file();
    openfile->fh =14;
    openfile->pid =1400;
    openfile->next =NULL;
    
    bhfs_f_list_append(openfile);

    openfile = bhfs_new_open_file();
    openfile->fh =15;
    openfile->pid =1500;
    openfile->next =NULL;
    
    bhfs_f_list_append(openfile);
    
    bhfs_f_list_delete(openfile_to_delete);
    free(openfile_to_delete);

    retrieved_openfile = bhfs_f_list_get(14);

    if (retrieved_openfile == NULL) {
        printf("Not found");
    } else {
        printf("\n The pid is %d", retrieved_openfile->pid);
    }

}*/

