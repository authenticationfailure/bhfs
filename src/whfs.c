/*
  WHFS: White Hole Filesystem
  
  Copyright (C) 2022-2023  David Turco  	
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
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
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <stddef.h>
#include <pthread.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "logger.h"
#include "fdmanager.h"

const int MAX_PATH = 1024;

struct whfs_config {
	char *recipient;
	char *rootdir;
	char *mountdir;
	int allow_other;
};

struct whfs_config conf;

int full_path(char *f_path, const char *path) {
	int f_path_size = 0;
	f_path_size = strlen(conf.rootdir) + strlen(path);

	if (f_path_size < MAX_PATH) {
		strcpy(f_path, conf.rootdir);
		strcat(f_path, path);
	}
	else 
		return 1;

	return 0; 
}

static void *whfs_init(struct fuse_conn_info *conn)
{
    // Fuse declares itself FUSE_CAP_ASYNC_READ by default. This causes the kernel to 
	// send multiple read requests in parallel, which causes a race condition.
	// Here we disable async reads because handling async reads would be expensive for this use case
	conn->async_read = 0;	
	return NULL;
}

static int whfs_getattr(const char *path, struct stat *stbuf)
{
	int res;
	char f_path[MAX_PATH];

	bhfs_log(LOG_DEBUG, "In function %s", __func__); 

	// File size information is only approximate as we don't have a reliable way to return
	// the actual size of the decrypted file without decrypting the entire file.
	// This can have unwanted side effects with some applications, for example with tar, which returns the
	// following error ultimately corrupting the file's data by padding the files in the archive:
	//    File <filename> shrunk by <num> bytes, padding with zeros
	// TODO: consider adding a mount option to return reliable file sizes with performance penalty.
	bhfs_log(LOG_DEBUG, "WARNING: Currently, the returned file size is the size of the encrypted GPG file!!", __func__);

	full_path(f_path, path);
	res = lstat(f_path, stbuf);
	if (res == -1)
		return -errno;
	
	return 0;
}

static int whfs_access(const char *path, int mask)
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

static int whfs_readlink(const char *path, char *buf, size_t size)
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


static int whfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
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

static int whfs_mknod(const char *path, mode_t mode, dev_t rdev)
{

	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
	// Do not allow
	return -1;
}

static int whfs_mkdir(const char *path, mode_t mode)
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

static int whfs_unlink(const char *path)
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

static int whfs_rmdir(const char *path)
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

static int whfs_symlink(const char *from, const char *to)
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

static int whfs_rename(const char *from, const char *to)
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

static int whfs_link(const char *from, const char *to)
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

static int whfs_chmod(const char *path, mode_t mode)
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

static int whfs_chown(const char *path, uid_t uid, gid_t gid)
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

static int whfs_truncate(const char *path, off_t size)
{
	// not allowed
	return -1;
}

#ifdef HAVE_UTIMENSAT
static int whfs_utimens(const char *path, const struct timespec ts[2])
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

static int whfs_open(const char *path, struct fuse_file_info *fi)
{
	int ret;
	char f_path[MAX_PATH];
	struct bhfs_open_file *fd;

	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
	
	fi->nonseekable = 1;
	
	// Set Direct IO, so that during the read operation we can return partial data.
	// This is because we don't know the size of the decrypted file and read requests are 
	// longer than the actual file because the size of the GPG file is returned.
	fi->direct_io = 1;

	full_path(f_path, path);

	bhfs_log(LOG_DEBUG, "In function %s - Open file '%s' "
		"in read mode - BEGIN", __func__, path); 

	fd = bhfs_new_open_file();

	/* Do some magic forking/exec and piping :) */
	// Make a pipe
	int pipefds[2];
	int pipe_fd_read, pipe_fd_write;
	int pipe_ret;

	pipe_ret = pipe(pipefds);
	if (pipe_ret < 0) return -errno;

	pipe_fd_read = pipefds[0]; // for the parent
	pipe_fd_write = pipefds[1]; // for the child

	bhfs_log(LOG_DEBUG, "In function %s - Preparing to fork", __func__); 

	pid_t pid = fork();
	if (pid < 0) {
		bhfs_log(LOG_ERROR, "In function %s - Fork failed", __func__);
		return -errno;
	}
	else if (pid == 0) {
		// Child
		bhfs_log(LOG_DEBUG, "In function %s - Inside CHILD", __func__);

		// Connect the end of the pipe to stdout of the process
		dup2(pipe_fd_write, 1);
		close(pipe_fd_read);
		// DO NOT ADD ANY OUTPUT between dup2 and execlp
		ret = execlp("gpg","gpg", \
				"--decrypt", \
				f_path, \
				NULL);

		// If we are here execlp failed
		if (ret < 0) {	
			bhfs_log(LOG_ERROR, "In function %s - Inside CHILD - "
				"Something went wrong when calling the external process. Exiting.", __func__);
			exit(-errno);
		}
	}
	
	// Parent
	bhfs_log(LOG_DEBUG, "In function %s - Inside PARENT", __func__);
	fd->pid = pid;
	close(pipe_fd_write);
	fd->fh = pipe_fd_read;
	bhfs_log(LOG_DEBUG, "In function %s - Inside PARENT. "
		"Children's PID is %d",  __func__, fd->pid);
	bhfs_log(LOG_DEBUG, "In function %s - Inside PARENT. "
		"The read PIPE's file descriptor is %d",  __func__, fd->fh);

	fi->fh = fd->fh;
	
	bhfs_f_list_append(fd);
	bhfs_log(LOG_DEBUG, "In function %s - Open file '%s'"
		"in read mode - END", __func__, path);
	return 0;
}

static int whfs_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	int fd;
	int res;
	struct bhfs_open_file *open_file;

	bhfs_log(LOG_DEBUG, "In function %s - Reading file '%s' with fd=%d", __func__, path, fi->fh); 

	open_file = bhfs_f_list_get(fi->fh);
	
	fd = open_file->fh;
	bhfs_log(LOG_DEBUG, "In function %s - Reading from fd=%d, buf=%x, size=%d, offset=%d", __func__, fd, buf, size, offset); 
	
	// This code is not safe with async reads enabled.
	// Because async reads would be expensive in this implementation, 
	// we just disable them in *whfs_init

	size_t read_size = 0;
	size_t s;

	pthread_mutex_lock(&(open_file->mutex));

	// TODO: there's a bug here with big files. The last block is read but not returned
	// to the invoking command like cp.
	// The error is not present when mounting with the option -o direct_io.
	// As a workaround we enforce direct io for the opened files
	do // keep reading until the size is satisfied or EOF
	{
		s = size - read_size;
		res = read(fd, buf + read_size, s);
		if (res == -1) {
			res = -errno;
			pthread_mutex_unlock(&(open_file->mutex));
			return res;
		}
		read_size = read_size + res;
	} while (res > 0 && read_size < size);

	pthread_mutex_unlock(&(open_file->mutex));

	return read_size;
}

static int whfs_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	//write is not permitted
	return -1;
}

static int whfs_statfs(const char *path, struct statvfs *stbuf)
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

static int whfs_release(const char *path, struct fuse_file_info *fi)
{
	int fd;
	struct bhfs_open_file *open_file;

	bhfs_log(LOG_DEBUG, "In function %s", __func__); 
	
	open_file = bhfs_f_list_get(fi->fh);

        bhfs_log(LOG_DEBUG, "In function %s, returned from bhfs_f_list_get", __func__);

	if (open_file != NULL) {
		fd = open_file->fh;
		bhfs_log(LOG_DEBUG, "In function %s - File descriptor %d is "
			"a pipe. Closing.", __func__, fd); 
		
		close(fd);
		
		int pid = open_file->pid;
		bhfs_log(LOG_DEBUG, "In function %s - Waiting for associated "
			"PID %d to complete.", __func__, pid);

		/* 
		Reap zombie processes without blocking.
		A simple waitpid causes the wait to hang indefinitely
		when files are accessed at the same time.
		This approach leaves some zombie processes temporary
		hanging around until the next time a file is closed.  
		TODO: improved this by reaping the processes in
		a handler registered for the SIGCHLD signal.
		*/
		int p;
		do {
        	p = waitpid(-1, NULL, WNOHANG);
    	} while (p != (pid_t)0 && p != (pid_t)-1);

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

static int whfs_fsync(const char *path, int isdatasync,
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
static int whfs_fallocate(const char *path, int mode,
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
static int whfs_setxattr(const char *path, const char *name, const char *value,
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

static int whfs_getxattr(const char *path, const char *name, char *value,
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

static int whfs_listxattr(const char *path, char *list, size_t size)
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

static int whfs_removexattr(const char *path, const char *name)
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

static struct fuse_operations whfs_oper = {
	.init		= whfs_init,
	.getattr	= whfs_getattr,
	.access		= whfs_access,
	.readlink	= whfs_readlink,
	.readdir	= whfs_readdir,
	.mknod		= whfs_mknod,
	.mkdir		= whfs_mkdir,
	.symlink	= whfs_symlink,
	.unlink		= whfs_unlink,
	.rmdir		= whfs_rmdir,
	.rename		= whfs_rename,
	.link		= whfs_link,
	.chmod		= whfs_chmod,
	.chown		= whfs_chown,
	.truncate	= whfs_truncate,
#ifdef HAVE_UTIMENSAT
	.utimens	= whfs_utimens,
#endif
	.open		= whfs_open,
	.read		= whfs_read,
	.write		= whfs_write,
	.statfs		= whfs_statfs,
	.release	= whfs_release,
	.fsync		= whfs_fsync,
#ifdef HAVE_POSIX_FALLOCATE
	.fallocate	= whfs_fallocate,
#endif
#ifdef HAVE_SETXATTR
	.setxattr	= whfs_setxattr,
	.getxattr	= whfs_getxattr,
	.listxattr	= whfs_listxattr,
	.removexattr	= whfs_removexattr,
#endif
};

void print_usage() {
	fprintf(stderr,
		"usage: whfs [general options] [FUSE options] "
		"rootDir mountPoint\n"
		"\n"
		"general options:\n"
		"    -o opt,[opt...]  mount options\n"
		"    -h   --help      print help\n"
		"    -V   --version   print version\n"
		"\n"
		"FUSE options:\n"
		"-d   -o debug          enable debug output (implies -f)\n"
		"-f                     foreground operation\n"
		"-s                     disable multi-threaded operation\n"
		"\n");
}

/* BEGIN FUSE-aided parsing of options */

enum {
	KEY_HELP,
	KEY_VERSION
};

#define whfs_OPT(t, p, v) { t, offsetof(struct whfs_config, p), v }

static struct fuse_opt whfs_opts[] = {
	//whfs_OPT("-r %s",          recipient, 0),
	//whfs_OPT("--recipient=%s", recipient, 0),
	//whfs_OPT("recipient=%s",   recipient, 0),
	whfs_OPT("allow_other",    allow_other, 1),
	whfs_OPT("allow_root",     allow_other, 1),
	
	FUSE_OPT_KEY("-V",         KEY_VERSION),
	FUSE_OPT_KEY("--version",  KEY_VERSION),
	FUSE_OPT_KEY("-h",         KEY_HELP),
	FUSE_OPT_KEY("--help",     KEY_HELP),
	FUSE_OPT_END
};

static int whfs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs)
{
	static int nonopt_argument_position = 0;

    switch (key) {

		case KEY_HELP:
			print_usage();
			fuse_opt_add_arg(outargs, "-h");
			fuse_main(outargs->argc, outargs->argv, &whfs_oper, NULL);
			exit(1);

		case KEY_VERSION:
			fprintf(stderr, "BHFS version %s\n", PACKAGE_VERSION);
			fuse_opt_add_arg(outargs, "--version");
			fuse_main(outargs->argc, outargs->argv, &whfs_oper, NULL);
			exit(0);

		case FUSE_OPT_KEY_NONOPT:
			switch (nonopt_argument_position) {
				case 0: // first argument is rootdir
					conf.rootdir = strdup(arg);
					nonopt_argument_position++;
					return 0; // don't pass it to fuse
				case 1: // second argument is mountdir
					conf.mountdir = strdup(arg);
					nonopt_argument_position++;
					return 1;
			}
			break;
	}
    return 1;
}
/* END FUSE-aided parsing of options */

int main(int argc, char *argv[])
{
	umask(0);

	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	memset(&conf, 0, sizeof(conf));
	conf.allow_other=0;

	fuse_opt_parse(&args, &conf, whfs_opts, whfs_opt_proc);

	int valid_conf = 0;

	if (conf.rootdir != NULL) {
		//TODO: implement validation of root directory
		fprintf(stderr, "Root directory: %s \n", conf.rootdir);
	} else {
		valid_conf = -1;
		fprintf(stderr,"ERROR: The root directory must be specified\n");
	};

	if (conf.allow_other !=0) {
		//TODO: implement safe handling for when other users access the filesystem.
		fprintf(stderr,"ERROR: the options 'allow_other' and 'allow_root'\n" 
		               "       are currently not permitted for security reasons.\n"
		               "       Really bad things could happen!!!!\n");
		valid_conf = -1;
	}

	if ((getuid() == 0) || (geteuid() == 0)) { 
		//Better avoid running this as root
		//TODO: review the risks of running this as root
		fprintf(stderr,"ERROR: don't run as root! Use a low privileged user.\n");
		valid_conf = -1;
	}

	if (valid_conf < 0) {
		fprintf(stderr,"\n\nThere are errors with your configuration. Exiting.\n");
		print_usage();
		exit(1);
	};

	return fuse_main(args.argc, args.argv, &whfs_oper, &conf);

}
