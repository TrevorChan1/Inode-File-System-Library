#include "fs.h"
#include "disk.h"
#include <stdint.h>

// Global variables of disk

// Superblock
struct super_block {
    uint16_t used_block_bitmap_count;
    uint16_t used_block_bitmap_offset;
    uint16_t inode_metadata_blocks;
    uint16_t inode_metadata_offset;
};

// inodes
struct inode {
    uint8_t file_type;
    uint32_t direct_offset;
    uint32_t single_direct_offset;
    uint32_t double_direct_offset;
    uint16_t file_size;
};

// File descriptors: Contain inode block #, if it's open, and file offset
struct fd {
    uint8_t open;
    uint16_t inode;
    uint16_t file_offset;
};
struct fd fileDescriptors[32];

/*
TO DO:

- Create file descriptor data type
- Create inode data type
- Create freed data bitmap data type
- 

*/

// Disk function that creates new disk and initializes global variables
int make_fs(const char *disk_name){
    // Only way for code to fail is if it fails to create the disk
    if (make_disk(disk_name) != 0){
        printf("ERROR: Unable to create disk with name %s\n", disk_name);
        return -1;
    }

    // Initialize file system datastructures:

    // 1. Initialize a superblock with file system metadata and write to disk
    struct super_block sb;
    sb.inode_metadata_offset = 1;
    sb.inode_metadata_blocks = 1;
    sb.used_block_bitmap_offset = 2;
    sb.used_block_bitmap_count = 0;

    if (block_write(0, &sb) != 0){
        printf("ERROR: Failed to write super block to disk\n");
        return -1;
    }

    // 2. Set up inodes and inode table
    struct inode inode_table[64];


    // 3. Set all file descriptors to be closed
    for (int i = 0; i < 32; i++){
        fileDescriptors[i].open = 0;
        fileDescriptors[i].file_offset = 0;
    }


    return 0;
}

// Disk function that mounts an existing virtual disk using a given name
int mount_fs(const char *disk_name){

}

// Disk function that unmounts virtual disk and saves any changes made to file system
int umount_fs(const char *disk_name){

}

// File system function that opens file and generates a file descriptor if file name valid
int fs_open(const char *name){

}

// File system function that closes file descriptor
int fs_close(int fd){

}

// File system function that creates a new empty file of given name
int fs_create(const char *name){

}

// File system function that deletes file of given name if exists and is closed
int fs_delete(const char *name){

}

// File system function that reads nbytes from file into buf
int fs_read(int fd, void *buf, size_t nbyte){

}

// File system function that writes nbytes of buf into file using file descriptor
int fs_write(int fd, void *buf, size_t nbyte){

}

// File system function that returns the filesize of given file
int fs_get_filesize(int fd){

}

// File system function that creates a NULL terminated array of file names in root directory
int fs_listfiles(char ***files){

}

// File system function that sets the file pointer offset of a file descriptor
int fs_lseek(int fd, off_t offset){

}

// File system function that truncates bytes (can't extend length)
int fs_truncate(int fd, off_t length){

}