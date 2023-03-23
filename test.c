#include "disk.h"
#include "fs.h"
#include <stdio.h>

int main(){
    printf("%d\n", make_fs("test_fs"));
    printf("%d\n", mount_fs("test_fs"));
    printf("%d\n", fs_create("hai.txt"));
    printf("%d\n", fs_create("hai2.txt"));
    printf("%d\n", fs_create("hai3.txt"));
    int fd = fs_open("hai.txt");
    int fd2 = fs_open("hai.txt");
    char * m = "hello!";
    char n[2];
    printf("%d\n", fs_write(fd, m, 7));
    printf("%d\n", fs_write(fd, m, 7));
    for (int i = 0; i < 14; i++){
        printf("%d, ", fs_read(fd2, n, 1));
        printf("%s \n", n);
    }
    
    return 0;
}

