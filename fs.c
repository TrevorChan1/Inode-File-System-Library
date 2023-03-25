#include "fs.h"
#include "disk.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NUM_BLOCKS 8192
#define MAX_NUM_FILES 64
#define MAX_OPEN_FILES 32

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
    uint32_t file_type;
    uint32_t file_size;
    uint16_t direct_offset[10];
    uint16_t single_indirect_offset;
    uint16_t double_indirect_offset;
};
struct inode * curTable;

// Directory Entries
struct dir_entry {
    uint8_t is_used;
    uint8_t inode_number;
    char name[15];
};
struct dir_entry * curDir;
int file_count;

// File descriptors: Contain inode block #, if it's open, and file offset
struct fd {
    uint8_t open;
    uint16_t inode;
    uint16_t file_offset;
};
struct fd fileDescriptors[MAX_OPEN_FILES];
int fd_count;

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
        mask = 255 - (128 >> shift);
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
    
    // Use a buffer with all unused bytes set to 0 (clear garbage before write)
    char block_buf[BLOCK_SIZE];
    memset(block_buf, 0, sizeof(block_buf));
    memcpy(block_buf, curSuper_block, sizeof(struct super_block));
    // printf("%lu %lu\n", sizeof(curSuper_block), sizeof(struct super_block));
    
    if (block_write(0, block_buf) != 0){
        printf("ERROR: Failed to write super block to disk\n");
        return -1;
    }

    // 2. Set up inodes, allocate memory, and set inode table
    curTable = (struct inode *) malloc(MAX_NUM_FILES * sizeof(struct inode));
    for (int i = 0; i < MAX_OPEN_FILES; i++){
        curTable[i].double_indirect_offset = 0;
        curTable[i].single_indirect_offset = 0;
    }
    
    memset(block_buf, 0, sizeof(block_buf));
    memcpy(block_buf, curTable, MAX_NUM_FILES * sizeof(struct inode));
    if (block_write(4, block_buf) != 0){
        printf("ERROR: Failed to write inode table to disk\n");
        return -1;
    }

    // 3. Set up directory entries and entry array (can only be MAX_NUM_FILES at a time)
    curDir = (struct dir_entry *) malloc(MAX_NUM_FILES * sizeof(struct dir_entry));
    for (int i = 0; i < MAX_OPEN_FILES; i++){
        curDir[i].is_used = 0;
        curDir[i].inode_number = 0;
        strcpy(curDir[i].name, "");
    }
    file_count = 0;

    memset(block_buf, 0, sizeof(block_buf));
    memcpy(block_buf, curDir, MAX_NUM_FILES * sizeof(struct dir_entry));
    if (block_write(1, block_buf) != 0){
        printf("ERROR: Failed to write directory entry block to disk\n");
        return -1;
    }

    // 4. inode free bitmap and initialize to all ones (uses uint8 so same functions can be used)
    curFreeInodes = (uint8_t *) malloc(8 * sizeof(uint8_t));
    for (int i = 0; i < MAX_NUM_FILES; i++){
        setNbit(curFreeInodes, MAX_NUM_FILES, i, 1);
    }

    memset(block_buf, 0, sizeof(block_buf));
    memcpy(block_buf, curFreeInodes, 8 * sizeof(uint8_t));
    if (block_write(3, block_buf) != 0){
        printf("ERROR: Failed to write inode free bitmap to disk\n");
        return -1;
    }

    // 5. Data free bitmap and initialize to ones (except for what's used for bitmaps and superblock)
    curFreeData = (uint8_t *) malloc(NUM_BLOCKS / 8 * sizeof(uint8_t));
    for (int i = 5; i < NUM_BLOCKS; i++){
        setNbit(curFreeData, NUM_BLOCKS, i, 1);
    }

    for (int j = 0; j < 5; j++){
        setNbit(curFreeData, NUM_BLOCKS, j, 0);
    }

    memset(block_buf, 0, sizeof(block_buf));
    memcpy(block_buf, curFreeData, NUM_BLOCKS / 8 * sizeof(uint8_t));
    if (block_write(2, block_buf) != 0){
        printf("ERROR: Failed to write data free bitmap to disk\n");
        return -1;
    }

    // 6. Set all file descriptors to be closed
    for (int i = 0; i < MAX_OPEN_FILES; i++){
        fileDescriptors[i].open = 0;
        fileDescriptors[i].file_offset = 0;
    }
    fd_count = 0;

    if (close_disk() != 0){
        printf("ERROR: Failed to close disk created\n");
        return -1;
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

    char block_buf[BLOCK_SIZE];
    if (block_read(0, block_buf) < 0){
        printf("ERROR: Failed to read from superblock\n");
        return -1;
    }
    memcpy(curSuper_block, block_buf, sizeof(struct super_block));

    int block_dir = curSuper_block->dentries;
    int block_freedata = curSuper_block->free_data_bitmap;
    int block_freeinode = curSuper_block->free_inode_bitmap;
    int block_inodes = curSuper_block->inode_table;
    // printf("%d %d %d %d\n", block_dir, block_freedata, block_freeinode, block_inodes);

    // 2. Load inode table based on superblock
    struct inode * curTable = (struct inode *) malloc(MAX_NUM_FILES * sizeof(struct inode));
    if (block_read(block_inodes, block_buf) < 0){
        printf("ERROR: Failed to load inode table\n");
        return -1;
    }
    // Only read bytes need
    memcpy(curTable, block_buf, MAX_NUM_FILES * sizeof(struct inode));
    // printf("%d\n", curTable[0].double_indirect_offset);

    // 3. Load directory entries based on superblock
    struct dir_entry * curDir = (struct dir_entry *) malloc(MAX_NUM_FILES * sizeof(struct dir_entry));
    if (block_read(block_dir, block_buf) < 0){
        printf("ERROR: Failed to load directory entries\n");
        return -1;
    }
    memcpy(curDir, block_buf, MAX_NUM_FILES * sizeof(struct dir_entry));

    file_count = 0;
    for (int i = 0; i < MAX_NUM_FILES; i++){
        if (curDir[i].is_used)
            file_count++;
    }

    // 4. Load inode free bitmap based on superblock
    uint8_t * free_inode_bitmap = (uint8_t *) malloc(8 * sizeof(uint8_t));
    if (block_read(block_freeinode, block_buf) < 0){
        printf("ERROR: Failed to load free inode bitmap\n");
        return -1;
    }
    memcpy(free_inode_bitmap, block_buf, 8 * sizeof(uint8_t));

    // 5. Load data free bitmap based on superblock
    uint8_t * free_block_bitmap = (uint8_t *) malloc(NUM_BLOCKS / 8 * sizeof(uint8_t));
    if (block_read(block_freedata, block_buf) < 0){
        printf("ERROR: Failed to load free data bitmap\n");
        return -1;
    }
    memcpy(free_block_bitmap, block_buf, NUM_BLOCKS / 8 * sizeof(uint8_t));

    // 6. Initialize all file descriptors to closed and offset 0
    for (int i = 0; i < MAX_OPEN_FILES; i++){
        fileDescriptors[i].open = 0;
        fileDescriptors[i].file_offset = 0;
    }
    fd_count = 0;

    return 0;
}

// Disk function that unmounts virtual disk and saves any changes made to file system
int umount_fs(const char *disk_name){

    if (open_disk(disk_name) == 0){
        printf("ERROR: Not an open disk\n");
        close_disk();
        return -1;
    }

    // Second, save all metadata to the disk (only need to write superblock once)

    // Directory entries, then free allocated memory
    // Use a buffer with all unused bytes set to 0 (clear garbage before write)
    char block_buf[BLOCK_SIZE];
    memset(block_buf, 0, sizeof(block_buf));
    memcpy(block_buf, curDir, MAX_NUM_FILES * sizeof(struct dir_entry));
    if (block_write(1, block_buf) != 0){
        printf("ERROR: Failed to write directory entry block to disk\n");
        return -1;
    }
    free(curDir);

    // Free data bitmap, then free allocated memory
    memset(block_buf, 0, sizeof(block_buf));
    memcpy(block_buf, curFreeData, NUM_BLOCKS / 8 * sizeof(uint8_t));
    if (block_write(2, block_buf) != 0){
        printf("ERROR: Failed to write data free bitmap to disk\n");
        return -1;
    }
    free(curFreeData);

    // Free inode bitmap, then free allocated memory
    memset(block_buf, 0, sizeof(block_buf));
    memcpy(block_buf, curFreeInodes, 8 * sizeof(uint8_t));
    if (block_write(3, block_buf) != 0){
        printf("ERROR: Failed to write inode free bitmap to disk\n");
        return -1;
    }
    free(curFreeInodes);

    // Inode table, then free allocated memory
    memset(block_buf, 0, sizeof(block_buf));
    memcpy(block_buf, curTable, MAX_NUM_FILES * sizeof(struct inode));
    if (block_write(4, block_buf) != 0){
        printf("ERROR: Failed to write inode table to disk\n");
        return -1;
    }
    free(curTable);

    // Close all file descriptors
    for (int i = 0; i < MAX_OPEN_FILES; i++){
        fileDescriptors[i].open = 0;
        fileDescriptors[i].file_offset = 0;
    }
    fd_count = 0;

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
    if (fd < 0 || fd >= MAX_OPEN_FILES){
        printf("ERROR: invalid file descriptor\n");
        return -1;
    }

    // Check if fd is open
    if (fileDescriptors[fd].open != 1){
        printf("ERROR: Not an open file descriptor\n");
        return -1;
    }

    //Otherwise, return valid
    return 0;
}

// File system helper function that checks if a file exists, if it does return inode number
int fs_exists(const char * name){
    // Iterate through all directories. If file exists and matches the name, return 0 for success
    for (int i = 0; i < MAX_NUM_FILES; i++){
        if(strcmp(curDir[i].name, name) == 0 && curDir[i].is_used){
            return curDir[i].inode_number;
        }
    }
    // If not found, return -1
    return -1;
}

// File system helper function that checks if there are any open file descriptors of the file
int fs_isopen(const char * name){
    // Check if file exists
    int inum = fs_exists(name);
    if (inum < 0){
        printf("ERROR: File %s doesn't exist\n", name);
        return -1;
    }

    // If open file descriptor found with same inode number, return 1
    for (int i = 0; i < MAX_OPEN_FILES; i++){
        if (fileDescriptors[i].inode == inum && fileDescriptors[i].open)
            return 1;
    }
    return 0;
}

// File system function that finds the first free file descriptor and returns it
int fs_freefd(){
    // Iterate through all file descriptors and find if any are unused
    for (int i = 0; i < MAX_OPEN_FILES; i++){
        if (fileDescriptors[i].open == 0){
            return i;
        }
    }
    // If found none, return -1
    return -1;
}

// Directory entry helper function that finds first directory entry index that's unused
int de_free(){
    for (int i = 0; i < MAX_NUM_FILES; i++){
        if (!curDir[i].is_used)
            return i;
    }
    return -1;
}

// Directory entry helper that finds directory entry value where open
int de_find(const char * name){
    for (int i = 0; i < MAX_NUM_FILES; i++){
        if (strcmp(name, curDir[i].name) == 0)
            return i;
    }
    return -1;
}

// File system function that opens file and generates a file descriptor if file name valid
int fs_open(const char *name){

    // If the file doesn't exist, print error
    int inum = fs_exists(name);
    if (inum < 0){
        printf("ERROR: File %s does not exist\n", name);
        return -1;
    }

    // Check if number of file descriptors is at max
    if (fd_count >= MAX_OPEN_FILES){
        printf("ERROR: Unable to open file: Max Number of File Descriptors\n");
        return -1;
    }

    // Get first free file descriptor (shouldn't reach error if above passed)
    int fd = fs_freefd();
    if (fd < 0){
        printf("ERROR: No open file descriptors");
        return -1;
    }
    fileDescriptors[fd].file_offset = 0;
    fileDescriptors[fd].open = 1;
    fileDescriptors[fd].inode = inum;
    fd_count++;

    return fd;
}

// File system function that closes file descriptor
int fs_close(int fd){
    // Check that the fd is valid
    if (validfd(fd) != 0){
        return -1;
    }

    // If fd valid, close it and set fd as unused
    fileDescriptors[fd].file_offset = 0;
    fileDescriptors[fd].open = 0;
    fileDescriptors[fd].inode = 0;
    fd_count--;

    return 0;
}

// File system function that creates a new empty file of given name
int fs_create(const char *name){
    // Check that name is valid
    if (strlen(name) < 0 || strlen(name) > 15){
        printf("ERROR: File name exceeds limit\n");
        return -1;
    }

    // Check that file name doesn't already exist
    for (int i = 0; i < MAX_NUM_FILES; i++){
        if (strcmp(name, curDir[i].name) == 0){
            printf("ERROR: File name already exists\n");
            return -1;
        }
    }

    // Check that directory is not full
    if (file_count >= MAX_NUM_FILES){
        printf("ERROR: Root directory is full\n");
        return -1;
    }
    
    // Find first free inode number (shouldn't fail if passed above)
    int inum = find1stFree(curFreeInodes, MAX_NUM_FILES);
    
    if (inum < 0){
        printf("ERROR: No free inodes\n");
        return -1;
    }

    // Find first free directory entry (shouldn't fail if passed above)
    int dirEntry = de_free();
    if (dirEntry < 0){
        printf("ERROR: No free directory entries\n");
        return -1;
    }

    // Initialize directory entry
    curDir[dirEntry].inode_number = inum;
    curDir[dirEntry].is_used = 1;
    strcpy(curDir[dirEntry].name, name);

    // Set inode bitmap bit to used
    setNbit(curFreeInodes, MAX_NUM_FILES, inum, 0);

    // Initialize inode (most initialization will happen on first write)
    curTable[inum].file_size = 0;
    curTable[inum].file_type = 1;

    return 0;
}

// File system function that deletes file of given name if exists and is closed
int fs_delete(const char *name){
    // Check if file exists in directory entries
    int inum = fs_exists(name);
    if (inum < 0){
        printf("ERROR: File %s does not exist\n", name);
        return -1;
    }

    // Check if file is open
    if (fs_isopen(name)){
        printf("ERROR: File %s is currently open / in use\n", name);
        return -1;
    }

    // Delete file:

    // 1. Close directory entry
    curDir[de_find(name)].is_used = 0;
    file_count--;

    // 2. Set inode entry to free
    setNbit(curFreeInodes, MAX_NUM_FILES, inum, 1);
    
    // 3. Free inode values (all indirect blocks)
    int numblocks = (curTable[inum].file_size / BLOCK_SIZE);
    if (curTable[inum].file_size % BLOCK_SIZE)
        numblocks++;
    uint16_t single_indir_block[BLOCK_SIZE / 2];
    uint16_t double_indir_block[BLOCK_SIZE/2];
    uint16_t current_double_block[BLOCK_SIZE/2];

    int num_direct = 10;
    if (numblocks < 10)
        num_direct = numblocks;

    // Free all direct offsets
    for (int i = 0; i < num_direct; i++){
        setNbit(curFreeData, NUM_BLOCKS, curTable[inum].direct_offset[i], 1);
        curTable[inum].direct_offset[i] = 0;
    }

    // Free all indirect offsets (if there are any)
    if (numblocks > 10){
        if (block_read(curTable[inum].single_indirect_offset, single_indir_block) < 0){
            printf("ERROR: Failed to read single indirection block from disk\n");
            return -1;
        }

        int index = 0;
        while ((index < BLOCK_SIZE/2) && (single_indir_block[index] != 0)){
            setNbit(curFreeData, NUM_BLOCKS, single_indir_block[index], 1);
            index++;
        }
        // Free the single indirection offset value
        setNbit(curFreeData, NUM_BLOCKS, curTable[inum].single_indirect_offset, 1);
    }
    
    // Free all double indirection offsets (if there are any)
    if (numblocks > (10 + BLOCK_SIZE / 2)){
        if (block_read(curTable[inum].double_indirect_offset, double_indir_block) < 0){
            printf("ERROR: Failed to read single indirection block from disk\n");
            return -1;
        }
        
        int double_index = 0;
        while ((double_index < BLOCK_SIZE/2) && (double_indir_block[double_index] != 0)){
            if (block_read(double_indir_block[double_index], current_double_block) < 0){
                printf("ERROR: Failed to read single indirection block from disk\n");
                return -1;
            }
            int single_index = 0;
            // Free each block in each value of the double indirection block (and the blocks that hold them)
            while ((single_index < BLOCK_SIZE/2) && (current_double_block[single_index] != 0)){
                setNbit(curFreeData, NUM_BLOCKS, current_double_block[single_index], 1);
                single_index++;
            }
            
            // Free the single indirection block that was just iterated through
            setNbit(curFreeData, NUM_BLOCKS, double_indir_block[double_index], 1);
            double_index++;

        }

        // Free the double indirection offset value
        setNbit(curFreeData, NUM_BLOCKS, curTable[inum].double_indirect_offset, 1);
    }

    
    curTable[inum].file_size = 0;
    curTable[inum].single_indirect_offset = 0;
    curTable[inum].double_indirect_offset = 0;


    return 0;
}

// File system function that reads nbytes from file into buf
int fs_read(int fd, void *buf, size_t nbyte){
    // Check if file descriptor is valid
    if (validfd(fd) != 0){
        return -1;
    }
    
    // Initialize variables to be used when iterating through blocks
    char block_buf[BLOCK_SIZE]; // Buffer that will read from blocks in disk
    int cur_block = fileDescriptors[fd].file_offset / BLOCK_SIZE;       // Current block (starts based on offset)
    int block_offset = fileDescriptors[fd].file_offset % BLOCK_SIZE;    // Byte offset (due to file offset)
    int bytes_read = 0; // How many bytes have been read so far
    struct inode * node = &curTable[fileDescriptors[fd].inode];    // inode
    uint16_t single_indirect_block[BLOCK_SIZE/2];
    int single_indirect_open = 0;
    uint16_t double_indir_block[BLOCK_SIZE/2];
    uint16_t current_double_block[BLOCK_SIZE/2];
    int current_open_double = -1;
    

    // Calculate number of blocks that can be read (assuming all metadata is correct)
    int bytes_left;
    int bytesRemaining = curTable[fileDescriptors[fd].inode].file_size - fileDescriptors[fd].file_offset;
    int double_indir_open = 0;

    // If there are enough bytes to read nbyte bytes, set read to nbytes
    if (bytesRemaining >= nbyte)
        bytes_left = nbyte;
    // If at end of file or file empty, set read to 0
    else if (bytesRemaining <= 0)
        bytes_left = 0;
    else
    // Otherwise, just set number able to read to the rest of the bytes left in the file
        bytes_left = bytesRemaining;

    // Loop through reading block by block until there are no more bytes left to read
    while (bytes_left > 0){

        // Calculate the block number (mainly for indirection purposes)
        int block;
        int double_index = (cur_block - 10 - BLOCK_SIZE) / BLOCK_SIZE;
        int double_offset = (cur_block - 10 - BLOCK_SIZE) % BLOCK_SIZE;
        // Case 1: Direct block number, just use regular index
        if (cur_block < 10)
            block = node->direct_offset[cur_block];
        // Case 2: Single indirection = read from
        else if (cur_block >= 10 && cur_block < (BLOCK_SIZE / 2 + 10)){
            printf("read single indirection\n");
            // Check if single indirect block has been read from yet (don't want to open twice)
            if (!single_indirect_open){
                if (block_read(node->single_indirect_offset, single_indirect_block) < 0){
                    printf("ERROR: Failed to read single indirect offset block\n");
                    return bytes_read;
                }
                single_indirect_open = 1;
            }

            // Read block number from the single indirect block
            block = single_indirect_block[cur_block - 10];
        }
        // Case 3: Double indirection, read from blocks assuming that double indirection exists if got here
        else if (cur_block >= (BLOCK_SIZE / 2 + 10) && cur_block < (BLOCK_SIZE * BLOCK_SIZE / 4 + BLOCK_SIZE / 2 + 10)){
            printf("read double indirection\n");
            // Open double indirection block
            if (!double_indir_open){
                if (block_read(node->double_indirect_offset, double_indir_block) < 0){
                    printf("ERROR: Failed to read double indirection block from disk\n");
                    return bytes_read;
                }
                double_indir_open = 1;
            }

            // Check if in the correct double indirection block
            if (double_index != current_open_double){
                // Set the double indirection block and index
                if (block_read(double_indir_block[double_index], current_double_block) < 0){
                    printf("ERROR: Failed to read single indirection block from disk\n");
                    current_open_double = double_index;
                }
            }
            block = current_double_block[double_offset];
        }
        else{
            printf("ERROR: Idk how you got here\n");
            return bytes_read;
        }

        // Index into block number and read block into block buffer (TO ADD: INDIRECTION)
        if (block_read(block, block_buf) != 0){
            printf("ERROR: Unable to read from block\n");
            return -1;
        }
        
        // Set the buffer size to be read from the file
        int read_size = 0;
        if (block_offset + bytes_left >= BLOCK_SIZE)    // If enough bytes left to read into next block
            read_size = BLOCK_SIZE - block_offset;
        else                    // If not enough bytes to read into next block, grab last bytes
            read_size = bytes_left;
        
        // Store bytes into the buf
        memcpy(buf + bytes_read, block_buf + block_offset, read_size);
        
        // Prep for the next iteration of the loop (or for it to end)
        bytes_read += read_size;
        bytes_left -= read_size;
        if (block_offset)
            block_offset = 0;
        cur_block++;
    }
    fileDescriptors[fd].file_offset += bytes_read;
    return bytes_read;
}

// File system function that writes nbytes of buf into file using file descriptor
int fs_write(int fd, void *buf, size_t nbyte){
    // Check if file descriptor is valid
    if (validfd(fd) != 0){
        return -1;
    }

    // Initialize variables to know where to start writing
    int cur_block = fileDescriptors[fd].file_offset / BLOCK_SIZE;       // Current block (starts based on offset)
    int block_offset = fileDescriptors[fd].file_offset % BLOCK_SIZE;    // Byte offset (due to file offset)
    struct inode * node = &curTable[fileDescriptors[fd].inode];
    uint16_t single_indirect_block[BLOCK_SIZE / 2];
    int bytes_written = 0;
    int bytes_left = nbyte;
    int single_indir_open = 0;

    //Variables for double indirection
    int double_indir_open = 0;
    uint16_t double_indir_block[BLOCK_SIZE/2];
    uint16_t current_double_block[BLOCK_SIZE/2];
    int current_open_double = -1;

    // Calculate the number of blocks that will be written to (accounting for nonzero offsets)
    int num_block_write = (nbyte + block_offset) / BLOCK_SIZE;
    if ((nbyte + block_offset) % BLOCK_SIZE) num_block_write++;

    // Iterate through all blocks need to write
    for(int i = 0; i < num_block_write; i++){

        // If writing current block requires a new block, grab first free block to be used
        uint8_t new_block = 0;
        uint16_t block;
        int double_index = (cur_block - 10 - BLOCK_SIZE) / BLOCK_SIZE;
        int double_offset = (cur_block - 10 - BLOCK_SIZE) % BLOCK_SIZE;

        if (node->file_size == 0 || node->file_size < (BLOCK_SIZE * (cur_block))){
            new_block = 1;
        }
        // If writing to a block that already exists, simply set block number to current block

        // Set which block writing to (check if direct, single indirect, or double indirect)
        // Case 1: Direct, if new block allocate but otherwise just set to next one in direct
        if (cur_block < 10){
            if (new_block){
                block = find1stFree(curFreeData, NUM_BLOCKS);
                // If full, return bytes_written (number of bytes currently written to disk)
                if (block < 0){
                    printf("ERROR: Disk is full\n");
                    return bytes_written;
                }
                node->direct_offset[cur_block] = block;
                setNbit(curFreeData, NUM_BLOCKS, block, 0);
            }
            else
                block = node->direct_offset[cur_block];
        }
        // Case 2: Single indirect, check if need to create indirect block
        else if (cur_block >= 10 && cur_block < (BLOCK_SIZE / 2 + 10)){

            // Create indirect block if not already set and update metadata
            if (node->single_indirect_offset == 0){
                int indir_block = find1stFree(curFreeData, NUM_BLOCKS);
                if (indir_block < 0){
                    printf("ERROR: Disk is full\n");
                    return bytes_written;
                }
                char write_buf[BLOCK_SIZE];
                memset(write_buf, 0, BLOCK_SIZE);
                if (block_write(indir_block, write_buf) < 0){
                    printf("ERROR: Failed to initialize single indirection block\n");
                    return bytes_written;
                }
                setNbit(curFreeData, NUM_BLOCKS, indir_block, 0);
                node->single_indirect_offset = indir_block;
            }
            
            // Check if single indirect block has been read from yet (don't want to open twice)
            if (!single_indir_open){
                if (block_read(node->single_indirect_offset, single_indirect_block) < 0){
                    printf("ERROR: Failed to read single indirect offset block\n");
                    return bytes_written;
                }
                single_indir_open = 1;
            }

            // Repeat code but done so that will only find 1st free block if had space to create indirect
            if (new_block){
                block = find1stFree(curFreeData, NUM_BLOCKS);
                // If full, return bytes_written (number of bytes currently written to disk)
                if (block < 0){
                    printf("ERROR: Disk is full\n");
                    return bytes_written;
                }
                single_indirect_block[cur_block - 10] = block;
                setNbit(curFreeData, NUM_BLOCKS, block, 0);
            }
            else
                block = single_indirect_block[cur_block - 10];
        }
        // Case 3: Double indirection, have to read in double indirection block and individual indirection blocks
        else if (cur_block >= (BLOCK_SIZE / 2 + 10) && cur_block < (BLOCK_SIZE * BLOCK_SIZE / 4 + BLOCK_SIZE / 2 + 10)){

            // Create double indirection block
            if (node->double_indirect_offset == 0){
                int free_double = find1stFree(curFreeData, NUM_BLOCKS);
                if (free_double < 0){
                    printf("ERROR: Not enough disk space to allocate double block\n");
                    return bytes_written;
                }
                char zeros[BLOCK_SIZE];
                memset(zeros, 0, BLOCK_SIZE);
                if (block_write(free_double, zeros) < 0){
                    printf("ERROR: Failed to write double indirection to disk\n");
                    return bytes_written;
                }
                setNbit(curFreeData, NUM_BLOCKS, free_double, 0);
                node->double_indirect_offset = free_double;
            }

            // Open double indirection block
            if (!double_indir_open){
                if (block_read(node->double_indirect_offset, double_indir_block) < 0){
                    printf("ERROR: Failed to read double indirection block from disk\n");
                    return bytes_written;
                }
                double_indir_open = 1;
            }

            // Check if in the correct double indirection block
            if (double_index != current_open_double){
                // If single indir block in double indirection block isn't made, initialize it
                if (double_indir_block[double_index] == 0){
                    int free_single = find1stFree(curFreeData, NUM_BLOCKS);
                    if (free_single < 0){
                        printf("ERROR: Not enough disk space to write indirection block\n");
                        return bytes_written;
                    }
                    // Initialize to all zeros
                    char zeros[BLOCK_SIZE];
                    memset(zeros, 0, BLOCK_SIZE);
                    if (block_write(free_single, zeros) < 0){
                        printf("ERROR: Failed to write double indirection to disk\n");
                        return bytes_written;
                    }
                    
                    setNbit(curFreeData, NUM_BLOCKS, free_single, 0);
                    double_indir_block[double_index] = free_single;
                    current_open_double = double_index;
                }
                // Set the double indirection block and index
                if (block_read(double_indir_block[double_index], current_double_block) < 0){
                    printf("ERROR: Failed to read single indirection block from disk\n");
                    current_open_double = double_index;
                }
            }

            // Now have the single indirection block, so find block number
            if (new_block){
                block = find1stFree(curFreeData, NUM_BLOCKS);
                // If full, return bytes_written (number of bytes currently written to disk)
                if (block < 0){
                    printf("ERROR: Disk is full\n");
                    return bytes_written;
                }
                current_double_block[double_offset] = block;
                setNbit(curFreeData, NUM_BLOCKS, block, 0);
            }
            // If next double block will be different, save the single indirection block
            if (current_open_double != ((cur_block + 1 - 10 - BLOCK_SIZE) / BLOCK_SIZE)){
                if (block_write(double_indir_block[current_open_double], current_double_block) < 0){
                    printf("ERROR: Failed to save double single indirection block to disk\n");
                    return bytes_written;
                }
            }
            else
                block = current_double_block[double_offset];
        }
        else {
            printf("ERROR: Reached maximum file size\n");
            return bytes_written;
        }

        // Calculate the number of blocks to be written on this write
        int this_write = 0;
        if (bytes_left + block_offset >= BLOCK_SIZE)
            this_write = BLOCK_SIZE - block_offset;
        else
            this_write = bytes_left;

        // Write to the block number provided
        // If new block, set unused bytes to 0. Otherwise, copy current block to write over
        char block_buf[BLOCK_SIZE]; // Buffer that will read from blocks in disk
        if (new_block)
            memset(block_buf, 0, sizeof(block_buf));
        else{
            if (block_read(block, block_buf) != 0){
                printf("ERROR: Unable to read from file data\n");
                return bytes_written;
            }
        }

        // Write to location block_buf + offset this_write bytes
        memcpy(block_buf + block_offset, buf + bytes_written, this_write);
        if (block_write(block, block_buf) != 0){
            printf("ERROR: Failed to write file data to disk\n");
            return bytes_written;
        }

        // Prepare for next write
        if (block_offset) block_offset = 0;
        bytes_written += this_write;
        node->file_size += bytes_written;
        fileDescriptors[fd].file_offset += bytes_written;
        cur_block++;

        // If done, then update the indirection blocks
        if (bytes_left == 0){
            if (single_indir_open){
                if (block_write(node->single_indirect_offset, single_indirect_block) < 0){
                    printf("ERROR: Failed to update single indirection block\n");
                    return bytes_written;
                }
            }
            if (double_indir_open){
                if (block_write(node->double_indirect_offset, double_indir_block) < 0){
                    printf("ERROR: Failed to update double indirection block\n");
                    return bytes_written;
                }
            }
        }
    }

    return bytes_written;
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
    // Iterate through all files in directory, if open then add name
    int curNum = 0;
    for (int i = 0; i < MAX_NUM_FILES; i++){
        if (curDir[i].is_used){
            *files[curNum++] = curDir[i].name;
        }
    }

    if (curNum == 0)
        printf("No files found\n");

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
    // Check if file descriptor is valid
    if (!validfd(fd)){
        return -1;
    }

    struct inode * node = &curTable[fileDescriptors[fd].inode];

    // Check if requested length is greater than the file size
    if (length > node->file_size){
        printf("ERROR: Requested file length exceeds file size\n");
        return -1;
    }

    // If no change is made to the length, do nothing
    if (length == node->file_size)
        return 0;


    int bytes_delete = node->file_size - length;
    int blocks_delete = bytes_delete / BLOCK_SIZE;

    int block_start = (length / BLOCK_SIZE);
    int block_offset = length % BLOCK_SIZE;

    // If there are bytes in a block being deleted, delete them (sets them to zeros)
    if (block_offset){
        char read_buf[BLOCK_SIZE];
        char write_buf[BLOCK_SIZE];

        if (block_start < 10){
            if (block_read(node->direct_offset[block_start], read_buf) < 0){
                printf("ERROR: Failed to read block from disk\n");
                return -1;
            }
            memset(write_buf, 0, BLOCK_SIZE);
            memcpy(write_buf, read_buf, block_offset);

            if (block_write(node->direct_offset[block_start], write_buf) < 0){
                printf("ERROR: Failed to write block to disk\n");
                return -1;
            }
        }
        else{
            uint16_t single_indir_block[BLOCK_SIZE / 2];
            if (block_read(node->single_indirect_offset, single_indir_block) < 0){
                printf("ERROR: Failed to read single indirection block from disk\n");
                return -1;
            }

            if (block_read(single_indir_block[block_start - 10], read_buf) < 0){
                printf("ERROR: Failed to read single indirection block from disk\n");
                return -1;
            }

            memset(write_buf, 0, BLOCK_SIZE);
            memcpy(write_buf, read_buf, block_offset);

            if (block_read(single_indir_block[block_start - 10], write_buf) < 0){
                printf("ERROR: Failed to write single indirection block to disk\n");
                return -1;
            }
        }

        block_start++;
    }
    uint16_t single_indir_block[BLOCK_SIZE/2];
    int single_indir_open = 0;

    // Now, all the blocks should just be FULL blocks (don't need to set to zeros, just free the disk block)
    for (int i = block_start; i < block_start + blocks_delete; i++){
        // Case 1: Direct offset
        if (i < 10){
            setNbit(curFreeData, NUM_BLOCKS, node->direct_offset[i], 1);
            node->direct_offset[i] = 0;
        }
        // Case 2: Single indirect offset
        else{
            // If first time reading from single indirection block, grab it from memory
            if (!single_indir_open){
                if (block_read(node->single_indirect_offset, single_indir_block) < 0){
                    printf("ERROR: Failed to read single indirection block from disk\n");
                    return -1;
                }
            }
            setNbit(curFreeData, NUM_BLOCKS, single_indir_block[i - 10], 1);
            single_indir_block[i - 10] = 0;
        }
    }
    if (single_indir_open){
        if (block_write(node->single_indirect_offset, single_indir_block) < 0){
            printf("ERROR: Failed to update single indirection block to disk\n");
            return -1;
        }
    }

    // Update file length (and file descriptor offset if necessary)
    node->file_size = length;
    if (node->file_size < fileDescriptors[fd].file_offset)
        fileDescriptors[fd].file_offset = length;


    return 0;
}
