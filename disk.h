#ifndef _DISK_H_
#define _DISK_H_

/******************************************************************************/
#define DISK_BLOCKS  15000      /* number of blocks on the disk                */
#define BLOCK_SIZE   4096      /* block size on "disk"                        */

/******************************************************************************/
int make_disk(const char *name);     /* create an empty, virtual disk file          */
int open_disk(const char *name);     /* open a virtual disk (file)                  */
int close_disk();              /* close a previously opened disk (file)       */
int is_disk_open();

int block_write(int block, const void *buf);
                               /* write a block of size BLOCK_SIZE to disk    */
int block_read(int block, void *buf);
                               /* read a block of size BLOCK_SIZE from disk   */
/******************************************************************************/

#endif
