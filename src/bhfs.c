/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall fusexmp.c `pkg-config fuse --cflags --libs` -o fusexmp
*/

#define FUSE_USE_VERSION 26

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "logger.h"
#include "fdmanager.h"

const int MAX_PATH = 1024;

char *base_path;

struct bhfs_open_file *bhfs_f_list; 


int full_path(char *f_path, const char *path) {
	u_int f_path_size = 0;
	f_path_size = strlen(base_path) + strlen(path);

	if (f_path_size < MAX_PATH) {
		strcpy(f_path, base_path);
		strcat(f_path, path);
	}
	else 
		return 1;

	return 0; 
}

static int bhfs_getattr(const char *path, struct stat *stbuf)
{
	int res;
	char f_path[MAX_PATH];

	bhfs_log(LOG_DEBUG, "In function %s", __func__); 

	full_path(f_path, path);

	res = lstat(f_path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int bhfs_access(const char *path, int mask)
{
	int res;
	char f_path[MAX_PATH];
	
	bhfs_log(LOG_DEBUG, "In function %s", __func__); 

	full_path(f_path, path);

	res = access(f_path, mask);
	if (res == -1)
		return -errno;

	return 0;
}

static int bhfs_readlink(const char *path, char *buf, size_t size)
{
	int res;
	char f_path[MAX_PATH];
	
	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
	
	full_path(f_path, path);

	res = readlink(f_path, buf, size - 1);
	if (res == -1)
		return -errno;

	buf[res] = '\0';
	return 0;
}


static int bhfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;
	char f_path[MAX_PATH];
	
	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
		
	full_path(f_path, path);
	
	dp = opendir(f_path);
	if (dp == NULL)
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0))
			break;
	}

	closedir(dp);
	return 0;
}

static int bhfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res;
	char f_path[MAX_PATH];
	
	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
		
	full_path(f_path, path);
	
	/* On Linux this could just be 'mknod(f_path, mode, rdev)' but this
	   is more portable */
	if (S_ISREG(mode)) {
		res = open(f_path, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISFIFO(mode))
		res = mkfifo(f_path, mode);
	else
		res = mknod(f_path, mode, rdev);
	if (res == -1)
		return -errno;

	return 0;
}

static int bhfs_mkdir(const char *path, mode_t mode)
{
	int res;
	char f_path[MAX_PATH];
	
	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
		
	full_path(f_path, path);
	
	res = mkdir(f_path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int bhfs_unlink(const char *path)
{
	int res;
	char f_path[MAX_PATH];
	
	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
		
	full_path(f_path, path);
	
	res = unlink(f_path);
	if (res == -1)
		return -errno;

	return 0;
}

static int bhfs_rmdir(const char *path)
{
	int res;
	char f_path[MAX_PATH];
	
	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
		
	full_path(f_path, path);
	
	res = rmdir(f_path);
	if (res == -1)
		return -errno;

	return 0;
}

static int bhfs_symlink(const char *from, const char *to)
{
	int res;
	char f_to[MAX_PATH];
	
	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
	
	full_path(f_to, to);
	
	res = symlink(from, f_to);
	if (res == -1)
		return -errno;

	return 0;
}

static int bhfs_rename(const char *from, const char *to)
{
	int res;
	char f_from[MAX_PATH];
	char f_to[MAX_PATH];
	
	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
		
	full_path(f_from, from);
	full_path(f_to, to);
		
	res = rename(f_from, f_to);
	if (res == -1)
		return -errno;

	return 0;
}

static int bhfs_link(const char *from, const char *to)
{
	int res;
	char f_to[MAX_PATH];
	
	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
	
	full_path(f_to, to);

	res = link(from, f_to);
	if (res == -1)
		return -errno;

	return 0;
}

static int bhfs_chmod(const char *path, mode_t mode)
{
	int res;
	char f_path[MAX_PATH];

	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
			
	full_path(f_path, path);
	
	res = chmod(f_path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int bhfs_chown(const char *path, uid_t uid, gid_t gid)
{
	int res;
	char f_path[MAX_PATH];

	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
			
	full_path(f_path, path);
	
	res = lchown(f_path, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

static int bhfs_truncate(const char *path, off_t size)
{
	int res;
	char f_path[MAX_PATH];

	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
	bhfs_log(LOG_DEBUG, "In function %s - Truncate file '%s'", __func__, path); 
			
	full_path(f_path, path);
	
	res = truncate(f_path, size);
	if (res == -1)
		return -errno;

	return 0;
}

#ifdef HAVE_UTIMENSAT
static int bhfs_utimens(const char *path, const struct timespec ts[2])
{
	int res;
	char f_path[MAX_PATH];
	
	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
		
	full_path(f_path, path);
	
	/* don't use utime/utimes since they follow symlinks */
	res = utimensat(0, path, ts, AT_SYMLINK_NOFOLLOW);
	if (res == -1)
		return -errno;

	return 0;
}
#endif

static int bhfs_open(const char *path, struct fuse_file_info *fi)
{
	int res;
	int ret;
	char f_path[MAX_PATH];
	struct bhfs_open_file *fd;

	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
	
	fi->nonseekable = 1;

	full_path(f_path, path);

	if (fi->flags & O_WRONLY || fi->flags & O_RDWR) {

		bhfs_log(LOG_DEBUG, "In function %s - Open file '%s' "
			"in write mode - BEGIN", __func__, path); 

        fd = bhfs_new_open_file();

        /* Do some magic forking/exec and piping :) */
        // Make a pipe
        int pipefds[2];
        int pipe_fd_read, pipe_fd_write;

        pipe(pipefds);
        pipe_fd_read = pipefds[0]; // for the child
        pipe_fd_write = pipefds[1]; // for the parent

		bhfs_log(LOG_DEBUG, "In function %s - Preparing to fork", __func__); 

        pid_t pid = fork();

        if (pid < 0) return -1;
        else if (pid == 0) {
			// Child
			bhfs_log(LOG_DEBUG, "In function %s - Inside CHILD", __func__);

            // Connect the end of the pipe to stdin of the process
            dup2(pipe_fd_read, 0);
			close(pipe_fd_write);

			bhfs_log(LOG_DEBUG, "In function %s - Inside CHILD - "
				"Executing exernal process", __func__);
			ret = execl("/usr/bin/base64","base64","-o",f_path, NULL);  
			bhfs_log(LOG_DEBUG, "In function %s - Inside CHILD - "
				"Something went wrong when calling the external process. Exiting.", __func__);
			
			exit(ret);
		}
		
		// Parent
		bhfs_log(LOG_DEBUG, "In function %s - Inside PARENT", __func__);
        fd->pid = pid;
        close(pipe_fd_read);
		fd->fh = pipe_fd_write;
		bhfs_log(LOG_DEBUG, "In function %s - Inside PARENT. "
			"Children's PID is %d",  __func__, fd->pid);
		bhfs_log(LOG_DEBUG, "In function %s - Inside PARENT. "
			"The write PIPE's file descriptor is %d",  __func__, fd->fh);

		fi->fh = fd->fh;
		
        bhfs_f_list_append(fd);
		bhfs_log(LOG_DEBUG, "In function %s - Open file '%s'"
			"in write mode - END", __func__, path);
        return 0;
        
    }
	bhfs_log(LOG_DEBUG, "In function %s - Open file '%s' "
		"in read mode - BEGIN", __func__, path); 

	res = open(f_path, fi->flags);
	if (res == -1)
		return -errno;

	fi->fh = res;

	bhfs_log(LOG_DEBUG, "In function %s - Inside PARENT. "
		"The write PIPE's file descriptor is %d",  __func__, res);

	bhfs_log(LOG_DEBUG, "In function %s - Open file '%s'"
		"in read mode - END", __func__, path);
	return 0;
}

static int bhfs_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	int fd;
	int res;
	struct bhfs_open_file *open_file;

	bhfs_log(LOG_DEBUG, "In function %s - Reading file '%s' with fd=%d", __func__, path, fi->fh); 
		
	open_file = bhfs_f_list_get(fi->fh);

	/* 
	TODO: is there a better way to handle this? 
	Returning 0 seems to be OK with dd, 
	which so far is the only one known to cause the issue.
	*/
	if (open_file != NULL) {
		bhfs_log(LOG_WARNING, "In function %s - WARNING: Read attempt of file '%s' "
			"which is opened for writing as a pipe with fd=%d", __func__, path, fi->fh);
		return 0;
	}
	
	fd = fi->fh;

	res = pread(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	return res;
}

static int bhfs_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	int fd;
	int res;
	struct bhfs_open_file *open_file;

	bhfs_log(LOG_DEBUG, "In function %s - Writing file '%s' with fd=%d", __func__, path, fi->fh); 
	
	open_file = bhfs_f_list_get(fi->fh);
	fd = open_file->fh;

	res = write(fd, buf, size);
	if (res == -1)
		res = -errno;

	return res;
}

static int bhfs_statfs(const char *path, struct statvfs *stbuf)
{
	int res;
	char f_path[MAX_PATH];

	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
		
	full_path(f_path, path);
	
	res = statvfs(f_path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int bhfs_release(const char *path, struct fuse_file_info *fi)
{
	int fd;
	struct bhfs_open_file *open_file;

	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
	
	open_file = bhfs_f_list_get(fi->fh);
	
	if (open_file != NULL) {
		fd = open_file->fh;
		bhfs_log(LOG_DEBUG, "In function %s - File descriptor %d is "
			"a pipe. Closing.", __func__, fd); 
		close(fd);
		
		int status;
		int pid = open_file->pid;
		bhfs_log(LOG_DEBUG, "In function %s - Waiting for associated "
			"PID %d to complete.", __func__, pid);
		waitpid(pid, &status, 0);

		bhfs_f_list_delete(open_file);
		bhfs_free_open_file(open_file);

		bhfs_log(LOG_DEBUG, "In function %s - Returning.", __func__, pid);
		return 0;
	}

	fd = fi->fh;
	bhfs_log(LOG_DEBUG, "In function %s - File descriptor %d is a file. "
		"Closing and returning.", __func__, fd);

	close(fd);
	return 0;
}

static int bhfs_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
	   
	   
	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}

#ifdef HAVE_POSIX_FALLOCATE
static int bhfs_fallocate(const char *path, int mode,
			off_t offset, off_t length, struct fuse_file_info *fi)
{
	int fd;
	int res;
	char f_path[MAX_PATH];

	bhfs_log(LOG_DEBUG, "In function %s", __func__); 	
	
	full_path(f_path, path);
	
	(void) fi;

	if (mode)
		return -EOPNOTSUPP;

	fd = open(f_path, O_WRONLY);
	if (fd == -1)
		return -errno;

	res = -posix_fallocate(fd, offset, length);

	close(fd);
	return res;
}
#endif

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int bhfs_setxattr(const char *path, const char *name, const char *value,
			size_t size, int flags)
{
	int res;
	char f_path[MAX_PATH];

	bhfs_log(LOG_DEBUG, "In function %s", __func__); 	
	
	full_path(f_path, path); 
	
	res = lsetxattr(f_path, name, value, size, flags);
	if (res == -1)
		return -errno;
	return 0;
}

static int bhfs_getxattr(const char *path, const char *name, char *value,
			size_t size)
{
	int res;
	char f_path[MAX_PATH];

	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
			
	full_path(f_path, path);
	
	res = lgetxattr(f_path, name, value, size);
	if (res == -1)
		return -errno;
	return res;
}

static int bhfs_listxattr(const char *path, char *list, size_t size)
{
	int res;
	char f_path[MAX_PATH];

	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
		
	full_path(f_path, path);

	res = llistxattr(f_path, list, size);
	if (res == -1)
		return -errno;
	return res;
}

static int bhfs_removexattr(const char *path, const char *name)
{
	int res;
	char f_path[MAX_PATH];

	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
			
	full_path(f_path, path);
	
	res = lremovexattr(f_path, name);
	if (res == -1)
		return -errno;
	return 0;
}
#endif /* HAVE_SETXATTR */

static struct fuse_operations bhfs_oper = {
	.getattr	= bhfs_getattr,
	.access		= bhfs_access,
	.readlink	= bhfs_readlink,
	.readdir	= bhfs_readdir,
	.mknod		= bhfs_mknod,
	.mkdir		= bhfs_mkdir,
	.symlink	= bhfs_symlink,
	.unlink		= bhfs_unlink,
	.rmdir		= bhfs_rmdir,
	.rename		= bhfs_rename,
	.link		= bhfs_link,
	.chmod		= bhfs_chmod,
	.chown		= bhfs_chown,
	.truncate	= bhfs_truncate,
#ifdef HAVE_UTIMENSAT
	.utimens	= bhfs_utimens,
#endif
	.open		= bhfs_open,
	.read		= bhfs_read,
	.write		= bhfs_write,
	.statfs		= bhfs_statfs,
	.release	= bhfs_release,
	.fsync		= bhfs_fsync,
#ifdef HAVE_POSIX_FALLOCATE
	.fallocate	= bhfs_fallocate,
#endif
#ifdef HAVE_SETXATTR
	.setxattr	= bhfs_setxattr,
	.getxattr	= bhfs_getxattr,
	.listxattr	= bhfs_listxattr,
	.removexattr	= bhfs_removexattr,
#endif
};

int main(int argc, char *argv[])
{

	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
		
	umask(0);

	// TODO: Change this so we can pass it from the command line
	char *b_path = "/tmp/destination";
	base_path = b_path;
	return fuse_main(argc, argv, &bhfs_oper, NULL);
}
