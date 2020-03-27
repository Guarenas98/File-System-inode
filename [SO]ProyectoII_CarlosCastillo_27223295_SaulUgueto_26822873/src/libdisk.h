//
// Disk.h
//
// Emulates a very simple disk (no timing issues). Allows user to
// read and write to the disk just as if it was dealing with sectors
//
//

#ifndef __Disk_H__
#define __Disk_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// a few disk parameters
#define SECTOR_SIZE  512
#define NUM_SECTORS  10000 

// disk errors
typedef enum {
	E_MEM_OP,
	E_INVALID_PARAM,
	E_OPENING_FILE,
	E_WRITING_FILE,
	E_READING_FILE,
} disk_error_t;

typedef struct sector {
	char data[SECTOR_SIZE];
} Sector;

extern disk_error_t disk_errno; // used to see what happened w/ disk ops

int disk_init();
int disk_save(char* file);
int disk_load(char* file);
int disk_write(int sector, char* buffer);
int disk_read(int sector, char* buffer);

#endif // __Disk_H__

