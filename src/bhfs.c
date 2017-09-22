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
	char f_path[MAX_PATH];

	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
	
	fi->nonseekable = 1;

	full_path(f_path, path);
	
	res = open(f_path, fi->flags);
	if (res == -1)
		return -errno;

	fi->fh = res;

	return 0;
}

static int bhfs_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	int fd;
	int res;
	char f_path[MAX_PATH];

	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
		
	full_path(f_path, path);
	
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
	char f_path[MAX_PATH];

	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
		
	full_path(f_path, path);
	
	fd = fi->fh;

	res = pwrite(fd, buf, size, offset);
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

	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
	
	fd = fi->fh;

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
