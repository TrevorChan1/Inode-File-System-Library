#include "fs.h"
#include "disk.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NUM_BLOCKS 8192

// Global variables of disk

// Superblock
struct super_block {
    uint16_t used_block_bitmap_count;
    uint16_t used_block_bitmap_offset;
    uint16_t inode_metadata_blocks;
    uint16_t inode_metadata_offset;
};
struct super_block curSuper_block;

// inodes
struct inode {
    uint8_t file_type;
    uint16_t file_size;
    uint32_t direct_offset;
    uint32_t single_indirect_offset;
    uint32_t double_indirect_offset;
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
uint64_t curFreeInodes;
uint8_t * curFreeData;

// Bitwise helper function that takes the free block map and returns nth bit (0 or 1)
int getNdatabit(uint8_t free_block_bitmap[NUM_BLOCKS / 8], int n){
    // If n is out of block number range, print error and do nothing
    if (n < 0 || n >= NUM_BLOCKS){
        printf("ERROR: free block index out of bounds\n");
        return -1;
    }

    // Go into the n/8th block, and in that block go to the bit shift bits from the left
    int arrIndex = n / 8;
    int shift = n % 8;
    return ((free_block_bitmap[arrIndex] >> (7 - shift)) & 1);
}

// Bitwise helper function that takes free block map and sets nth bit (to 0 or 1)
void setNdatabit(uint8_t free_block_bitmap[NUM_BLOCKS / 8], int n, int value){
    // If n is out of block number range, print error and do nothing
    if (n < 0 || n >= NUM_BLOCKS){
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
        free_block_bitmap[arrIndex] = free_block_bitmap[arrIndex] | mask;
    }
    // If value is setting a 0, create mask of 1's and a 0 in the bit place, then AND with number
    else{
        mask = 255;
        mask = (mask >> (7-shift)) & 0;
        free_block_bitmap[arrIndex] = free_block_bitmap[arrIndex] & mask;
    }
    
}

// Bitwise helper function that takes the free block map and returns nth bit (0 or 1)
int getNinodebit(uint64_t * free_inode_bitmap, int n){
// If n is out of inode range, print error and do nothing
    if (n < 0 || n >= 64){
        printf("ERROR: free inode index out of bounds\n");
        return -1;
    }

    // Go into the n/8th block, and in that block go to the bit shift bits from the left
    return ((*free_inode_bitmap >> (64 - n)) & 1);
}

// Bitwise helper function that takes free block map and sets nth bit (to 0 or 1)
void setNinodebit(uint64_t * free_inode_block, int n, int value){
    // If n is out of block number range, print error and do nothing
    if (n < 0 || n >= 64){
        printf("ERROR: free inode index out of bounds\n");
        return;
    }

    // Go into the n/8th block, and in that block go to the bit shift bits from the left

    uint64_t mask;
    // If value is gonna set a 1, shift the 1 to the bit number and OR the mask with the number
    if (value){
        mask = 1;
        mask = mask << (63-n);
        *free_inode_block = *free_inode_block | mask;
    }
    // If value is setting a 0, create mask of 1's and a 0 in the bit place, then AND with number
    else{
        mask = 92233720367;
        mask = (mask >> (63-n)) & 0;
        *free_inode_block = *free_inode_block & mask;
    }
    
}


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
    curTable = inode_table;

    if (block_write(3, inode_table) != 0){
        printf("ERROR: Failed to write inode table to disk\n");
        return -1;
    }

    // 3. Set up directory entries and entry array (can only be 64 at a time)
    struct dir_entry dirEntries[64];
    for (int i = 0; i < 32; i++){
        dirEntries[i].is_used = 0;
    }
    curDir = dirEntries;

    if (block_write(4, dirEntries) != 0){
        printf("ERROR: Failed to write directory entry block to disk\n");
        return -1;
    }

    // 4. inode free bitmap and initialize to all ones
    uint64_t free_inode_bitmap;
    for (int i = 0; i < 64; i++){
        setNinodebit(&free_inode_bitmap, i, 1);
    }
    curFreeInodes = free_inode_bitmap;

    if (block_write(1, &free_inode_bitmap) != 0){
        printf("ERROR: Failed to write inode free bitmap to disk\n");
        return -1;
    }

    // 5. Data free bitmap and initialize to ones (except for what's used for bitmaps and superblock)
    uint8_t free_block_bitmap[NUM_BLOCKS / 8];
    for (int i = 4; i < NUM_BLOCKS; i++){
        setNdatabit(free_block_bitmap, i, 1);
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
    return 0;
}

// Disk function that unmounts virtual disk and saves any changes made to file system
int umount_fs(const char *disk_name){
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
    return 0;
}

// File system function that creates a NULL terminated array of file names in root directory
int fs_listfiles(char ***files){
    return 0;
}

// File system function that sets the file pointer offset of a file descriptor
int fs_lseek(int fd, off_t offset){
    return 0;
}

// File system function that truncates bytes (can't extend length)
int fs_truncate(int fd, off_t length){
    return 0;
}