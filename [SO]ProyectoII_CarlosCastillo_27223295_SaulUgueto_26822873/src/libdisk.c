#include "libdisk.h"
#include <string.h>

// the disk in memory (static makes it private to the file)
static Sector* disk;

// used to see what happened w/ disk ops
disk_error_t disk_errno; 

// used for statistics
// static int lastsector = 0;
// static int seekCount = 0;

/*
 * disk_init
 *
 * Initializes the disk area (really just some memory for now).
 *
 * THIS FUNCTION MUST BE CALLED BEFORE ANY OTHER FUNCTION IN HERE CAN BE USED!
 *
 */
int disk_init()
{
	// create the disk image and fill every sector with zeroes
	disk = (Sector *) calloc(NUM_SECTORS, sizeof(Sector));
	if(disk == NULL) {
		disk_errno = E_MEM_OP;
		return -1;
	}
	return 0;
}

/*
 * disk_save
 *
 * Makes sure the current disk image gets saved to memory - this
 * will overwrite an existing file with the same name so be careful
 */
int disk_save(char* file) {
	FILE* diskFile;

	// error check
	if (file == NULL) {
		disk_errno = E_INVALID_PARAM;
		return -1;
	}

	// open the diskFile
	if ((diskFile = fopen(file, "w")) == NULL) {
		disk_errno = E_OPENING_FILE;
		return -1;
	}

	// actually write the disk image to a file
	if ((fwrite(disk, sizeof(Sector), NUM_SECTORS, diskFile)) != NUM_SECTORS) {
		fclose(diskFile);
		disk_errno = E_WRITING_FILE;
		return -1;
	}

	// clean up and return
	fclose(diskFile);
	return 0;
}

/*
 * disk_load
 *
 * Loads a current disk image from disk into memory - requires that
 * the disk be created first.
 */
int disk_load(char* file) {
	FILE* diskFile;

	// error check
	if (file == NULL) {
		disk_errno = E_INVALID_PARAM;
		return -1;
	}

	// open the diskFile
	if ((diskFile = fopen(file, "r")) == NULL) {
		disk_errno = E_OPENING_FILE;
		return -1;
	}

	// actually read the disk image into memory
	if ((fread(disk, sizeof(Sector), NUM_SECTORS, diskFile)) != NUM_SECTORS) {
		fclose(diskFile);
		disk_errno = E_READING_FILE;
		return -1;
	}

	// clean up and return
	fclose(diskFile);
	return 0;
}

/*
 * disk_read
 *
 * Reads a single sector from "disk" and puts it into a buffer provided
 * by the user.
 */
int disk_read(int sector, char* buffer) {
	// quick error checks
	if ((sector < 0) || (sector >= NUM_SECTORS) || (buffer == NULL)) {
		disk_errno = E_INVALID_PARAM;
		return -1;
	}

	// copy the memory for the user
	if((memcpy((void*)buffer, (void*)(disk + sector), sizeof(Sector))) == NULL) {
		disk_errno = E_MEM_OP;
		return -1;
	}

	return 0;
}

/*
 * disk_write
 *
 * Writes a single sector from memory to "disk".
 */
int disk_write(int sector, char* buffer) 
{
	// quick error checks
	if((sector < 0) || (sector >= NUM_SECTORS) || (buffer == NULL)) {
		disk_errno = E_INVALID_PARAM;
		return -1;
	}

	// copy the memory for the user
	if((memcpy((void*)(disk + sector), (void*)buffer, sizeof(Sector))) == NULL) {
		disk_errno = E_MEM_OP;
		return -1;
	}
	return 0;
}

