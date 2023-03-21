#include "fs.h"
#include "disk.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NUM_BLOCKS 8192

// Global variables of disk

// Superblock
struct super_block {
    uint16_t dentries;
    uint16_t free_inode_bitmap;
    uint16_t free_data_bitmap;
    uint16_t inode_table;
};
struct super_block * curSuper_block;

// inodes
struct inode {
    uint16_t file_type;
    uint16_t file_size;
    uint8_t direct_offset[10];
    uint8_t single_indirect_offset;
    uint8_t double_indirect_offset;
};
struct inode * curTable;

// Directory Entries
struct dir_entry {
    uint8_t is_used;
    uint8_t inode_number;
    char name[15];
};
struct dir_entry * curDir;

// File descriptors: Contain inode block #, if it's open, and file offset
struct fd {
    uint8_t open;
    uint16_t inode;
    uint16_t file_offset;
};
struct fd fileDescriptors[32];

// Free bitmaps global variables
uint8_t * curFreeInodes;
uint8_t * curFreeData;

// Bitwise helper function that takes a bitmap and returns nth bit (0 or 1)
int getNbit(uint8_t * bitmap, int size, int n){
    // If n is out of block number range, print error and do nothing
    if (n < 0 || n >= size){
        printf("ERROR: bitmap index out of bounds\n");
        return -1;
    }

    // Go into the n/8th block, and in that block go to the bit shift bits from the left
    int arrIndex = n / 8;
    int shift = n % 8;
    return ((bitmap[arrIndex] >> (7 - shift)) & 1);
}

// Bitwise helper function that finds first 1 in a bitmap (first free block number)
int find1stFree(uint8_t * bitmap, int size){
    // Iterate through all bits and return the first 1
    for (int i = 0; i < size; i++){
        if (getNbit(bitmap, size, i))
            return i;
    }
    // If no 1's are found, return -1 (no available block numbers)
    return -1;
}

// Bitwise helper function that takes bitmap and sets nth bit (to 0 or 1)
void setNbit(uint8_t * bitmap, int size, int n, int value){
    // If n is out of block number range, print error and do nothing
    if (n < 0 || n >= size){
        printf("ERROR: free block index out of bounds\n");
        return;
    }

    // Go into the n/8th block, and in that block go to the bit shift bits from the left
    int arrIndex = n / 8;
    int shift = n % 8;
    uint8_t mask;
    // If value is gonna set a 1, shift the 1 to the bit number and OR the mask with the number
    if (value){
        mask = 1;
        mask = mask << (7-shift);
        bitmap[arrIndex] = bitmap[arrIndex] | mask;
    }
    // If value is setting a 0, create mask of 1's and a 0 in the bit place, then AND with number
    else{
        mask = 255;
        mask = (mask >> (7-shift)) & 0;
        bitmap[arrIndex] = bitmap[arrIndex] & mask;
    }
    
}

// Disk function that creates new disk and initializes global variables
int make_fs(const char *disk_name){
    // Only way for code to fail is if it fails to create the disk
    if (make_disk(disk_name) != 0){
        printf("ERROR: Unable to create disk with name %s\n", disk_name);
        return -1;
    }

    if (open_disk(disk_name) != 0){
        printf("ERROR: Unable to open disk with name %s\n", disk_name);
        return -1;
    }

    // Initialize file system datastructures:

    // 1. Initialize a superblock with file system metadata and write to disk
    curSuper_block = (struct super_block *) malloc(sizeof(struct super_block));
    curSuper_block->dentries = 1;
    curSuper_block->free_data_bitmap = 2;
    curSuper_block->free_inode_bitmap = 3;
    curSuper_block->inode_table = 4;

    if (block_write(0, curSuper_block) != 0){
        printf("ERROR: Failed to write super block to disk\n");
        return -1;
    }

    // 2. Set up inodes, allocate memory, and set inode table
    struct inode * curTable = (struct inode *) malloc(64 * sizeof(struct inode));

    if (block_write(4, curTable) != 0){
        printf("ERROR: Failed to write inode table to disk\n");
        return -1;
    }

    // 3. Set up directory entries and entry array (can only be 64 at a time)
    struct dir_entry * curDir = (struct dir_entry *) malloc(64 * sizeof(struct dir_entry));
    for (int i = 0; i < 32; i++){
        curDir[i].is_used = 0;
        curDir[i].inode_number = 0;
        strcpy(curDir[i].name, "");
    }

    if (block_write(1, curDir) != 0){
        printf("ERROR: Failed to write directory entry block to disk\n");
        return -1;
    }

    // 4. inode free bitmap and initialize to all ones (uses uint8 so same functions can be used)
    uint8_t * free_inode_bitmap = (uint8_t *) malloc(8 * sizeof(uint8_t));
    for (int i = 0; i < 64; i++){
        setNbit(free_inode_bitmap, 64, i, 1);
    }
    curFreeInodes = free_inode_bitmap;

    if (block_write(3, &free_inode_bitmap) != 0){
        printf("ERROR: Failed to write inode free bitmap to disk\n");
        return -1;
    }

    // 5. Data free bitmap and initialize to ones (except for what's used for bitmaps and superblock)
    uint8_t * free_block_bitmap = (uint8_t *) malloc(NUM_BLOCKS / 8 * sizeof(uint8_t));
    for (int i = 5; i < NUM_BLOCKS; i++){
        setNbit(free_block_bitmap, NUM_BLOCKS, i, 1);
    }

    for (int j = 0; j < 5; j++){
        setNbit(free_block_bitmap, NUM_BLOCKS, j, 0);
    }
    curFreeData = free_block_bitmap;

    if (block_write(2, free_block_bitmap) != 0){
        printf("ERROR: Failed to write data free bitmap to disk\n");
        return -1;
    }

    // 6. Set all file descriptors to be closed
    for (int i = 0; i < 32; i++){
        fileDescriptors[i].open = 0;
        fileDescriptors[i].file_offset = 0;
    }

    return 0;
}

// Disk function that mounts an existing virtual disk using a given name
int mount_fs(const char *disk_name){
    // Check if disk exists and, if so, open it
    if (open_disk(disk_name) < 0)
        return -1;
    
    // Read in super block and dynamically allocate memory for all global metadata datastructures

    // 1. Read in the superblock from the 1st block of the disk
    curSuper_block = (struct super_block *) malloc(sizeof(struct super_block));
    if (block_read(0, curSuper_block) < 0){
        printf("ERROR: Failed to read from superblock\n");
        return -1;
    }

    int block_dir = curSuper_block->dentries;
    int block_freedata = curSuper_block->free_data_bitmap;
    int block_freeinode = curSuper_block->free_inode_bitmap;
    int block_inodes = curSuper_block->inode_table;

    // 2. Load inode table based on superblock
    struct inode * curTable = (struct inode *) malloc(64 * sizeof(struct inode));
    if (block_read(block_inodes, curTable) < 0){
        printf("ERROR: Failed to load inode table\n");
        return -1;
    }

    // 3. Load directory entries based on superblock
    struct dir_entry * curDir = (struct dir_entry *) malloc(64 * sizeof(struct dir_entry));
    if (block_read(block_dir, curDir) < 0){
        printf("ERROR: Failed to load directory entries\n");
        return -1;
    }

    // 4. Load inode free bitmap based on superblock
    uint8_t * free_inode_bitmap = (uint8_t *) malloc(8 * sizeof(uint8_t));
    if (block_read(block_freeinode, free_inode_bitmap) < 0){
        printf("ERROR: Failed to load free inode bitmap\n");
        return -1;
    }

    // 5. Load data free bitmap based on superblock
    uint8_t * free_block_bitmap = (uint8_t *) malloc(NUM_BLOCKS / 8 * sizeof(uint8_t));
    if (block_read(block_freedata, free_block_bitmap) < 0){
        printf("ERROR: Failed to load free data bitmap\n");
        return -1;
    }

    // 6. Initialize all file descriptors to closed and offset 0
    for (int i = 0; i < 32; i++){
        fileDescriptors[i].open = 0;
        fileDescriptors[i].file_offset = 0;
    }

    return 0;
}

// Disk function that unmounts virtual disk and saves any changes made to file system
int umount_fs(const char *disk_name){

    // // First, check if disk is open
    // if (!is_disk_open()){
    //     printf("ERROR: Disk not open\n");
    //     return -1;
    // }

    // Second, save all metadata to the disk (only need to write superblock once)

    // Directory entries, then free allocated memory
    if (block_write(1, curDir) != 0){
        printf("ERROR: Failed to write directory entries to disk\n");
        return -1;
    }
    free(curDir);

    // Free data bitmap, then free allocated memory
    if (block_write(2, curFreeData) != 0){
        printf("ERROR: Failed to write free data bitmap to disk\n");
        return -1;
    }
    free(curFreeData);

    // Free inode bitmap, then free allocated memory
    if (block_write(3, curFreeInodes) != 0){
        printf("ERROR: Failed to write free inode bitmap to disk\n");
        return -1;
    }
    free(curFreeInodes);

    // Inode table, then free allocated memory
    if (block_write(4, curTable) != 0){
        printf("ERROR: Failed to write free inode bitmap to disk\n");
        return -1;
    }
    free(curTable);

    // Close all file descriptors
    for (int i = 0; i < 32; i++){
        fileDescriptors[i].open = 0;
        fileDescriptors[i].file_offset = 0;
    }

    // Last, close the disk after all metadata was written to it
    if (close_disk() < 0){
        printf("ERROR: Failed to close disk\n");
        return -1;
    }

    // Return success once closed
    return 0;
}

// Helper function that checks if file descriptor is valid
int validfd(int fd){

    // Check if fd is within range
    if (fd < 0 || fd >= 32){
        printf("ERROR: invalid file descriptor\n");
        return -1;
    }

    // Check if fd is open
    if (!fileDescriptors[fd].open){
        printf("ERROR: Not an open file descriptor\n");
        return -1;
    }

    //Otherwise, return valid
    return 0;
}

// File system function that opens file and generates a file descriptor if file name valid
int fs_open(const char *name){
    return 0;
}

// File system function that closes file descriptor
int fs_close(int fd){
    return 0;
}

// File system function that creates a new empty file of given name
int fs_create(const char *name){
    return 0;
}

// File system function that deletes file of given name if exists and is closed
int fs_delete(const char *name){
    return 0;
}

// File system function that reads nbytes from file into buf
int fs_read(int fd, void *buf, size_t nbyte){
    return 0;
}

// File system function that writes nbytes of buf into file using file descriptor
int fs_write(int fd, void *buf, size_t nbyte){
    return 0;
}

// File system function that returns the filesize of given file
int fs_get_filesize(int fd){
    
    // Check if the file descriptor is valid
    if (!validfd(fd)){
        return -1;
    }

    // Return the file size of the inode pointed to by file descriptor
    return (int) curTable[fileDescriptors[fd].inode].file_size;
}

// File system function that creates a NULL terminated array of file names in root directory
int fs_listfiles(char ***files){
    
    return 0;
}

// File system function that sets the file pointer offset of a file descriptor
int fs_lseek(int fd, off_t offset){
    
    // Check if file descriptor is valid
    if (!validfd(fd)){
        return -1;
    }

    int filesize = curTable[fileDescriptors[fd].inode].file_size;
    if (offset < 0 || offset > filesize){
        printf("ERROR: offset out of range\n");
        return -1;
    }

    fileDescriptors[fd].file_offset = offset;

    return 0;
}

// File system function that truncates bytes (can't extend length)
int fs_truncate(int fd, off_t length){
    return 0;
}
