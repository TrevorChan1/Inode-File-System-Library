# File System Library

File System Library that implements creating, mounting, and unmounting filesystem metadata which can then be modified by creating files, deleting files, writing to files, reading from files, etc. This file system is implemented based on a disk of 8192 blocks where each is 4096 bytes large. It also assumes there can only be 64 files at a time and that all files are stored within one directory (the root directory).

This filesystem implements an inode-based system where each individual file's metadata is stored on an inode block.

## Metadata
The metadata blocks stored are:

### 0: Superblock
Contains the block numbers of each of the metadata data structures

#### Values:
uint16_t dentries;
uint16_t free_inode_bitmap;
uint16_t free_data_bitmap;
uint16_t inode_table;

### 1: Directory Entries
An array of 64 directory entries that map file names (maximum of 15 characters) to inode numbers (the file metadata). Stored in block 1

#### Values:
uint8_t is_used;
uint8_t inode_number;
char name[15];

### 2: Free Data Bitmap
A bitmap of size NUM_BLOCKS (8192 in this case) where each bit corresponds to a block number. This is used to find the first free block of data for writing data for files

### Values:
uint8_t curFreeData[NUM_BLOCKS / 8];

### 3: Free Inode Bitmap
A bitmap of size MAX_NUM_FILES (64 in this case) where each bit corresponds to a free Inode number. This is used to find the first free inode in the inode table to initialize file metadata when fs_create is called

### Values:
uint8_t curFreeInode[MAX_NUM_FILE / 8];

### 4: Inode Table
An array of 64 inodes where each inode corresponds to a file. Indexed by inode number

#### Values:
struct inode curTable[MAX_NUM_FILES];

I did not use any outside sources (Larry was big help though, king dropped his crown 👑)