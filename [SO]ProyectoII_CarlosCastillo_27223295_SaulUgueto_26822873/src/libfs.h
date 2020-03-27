#ifndef __LibFS_h__
#define __LibFS_h__

/*
 * DO NOT MODIFY THIS FILE
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* used for errors */
extern int os_errno;

/* error types - don't change anything about these!! (even the order!) */
typedef enum {
	E_GENERAL,      /* general */
	E_CREATE, 
	E_NO_SUCH_FILE, 
	E_TOO_MANY_OPEN_FILES, 
	E_BAD_FD, 
	E_NO_SPACE, 
	E_FILE_TOO_BIG, 
	E_SEEK_OUT_OF_BOUNDS, 
	E_FILE_IN_USE, 
	E_BUFFER_TOO_SMALL, 
	E_DIR_NOT_EMPTY,
	E_ROOT_DIR,
} fs_error_t;

/* file system generic call */
int fs_boot(char *path);
int fs_sync(void);

/* file ops */
int file_create(char *file);
int file_open(char *file);
int file_read(int fd, void *buffer, int size);
int file_write(int fd, void *buffer, int size);
int file_seek(int fd, int offset);
int file_close(int fd);
int file_unlink(char *file);

#endif /* __LibFS_h__ */

