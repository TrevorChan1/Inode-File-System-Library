#include "disk.h"
#include "fs.h"
#include <stdio.h>
#include <string.h>

int main(){
    printf("%d\n", make_fs("test_fs"));
    printf("%d\n", mount_fs("test_fs"));
    printf("%d\n", fs_create("hai.txt"));
    printf("%d\n", fs_create("hai2.txt"));
    printf("%d\n", fs_create("hai3.txt"));
    int fd = fs_open("hai.txt");
    int fd2 = fs_open("hai.txt");
    char * m = "a";
    char n[2];
    int num_written = 0;
    int num_read = 0;
    for (int i = 0; i < 4096 * 2060; i++){
        int wrote = fs_write(fd, m, 1);
        if (wrote <= 0){
            printf("didn't work\n");
            return -1;
        }
        num_written += wrote;
    }
    printf("Bytes written: %d\n", num_written);
    for (int j = 0; j < 4096 * 2060; j++){
        int read = fs_read(fd2, n, 1);
        if (read <= 0){
            printf("read didn't work\n");
            return -1;
        }
        if (strcmp(n, "a") == 0)
            num_read += read;
    }

    printf("Bytes written: %d\n", num_written);
    printf("Bytes read: %d\n", num_read);
    
    return 0;
}

