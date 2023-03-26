#include "disk.h"
#include "fs.h"
#include <stdio.h>

int main(){
    printf("mount: %d\n", mount_fs("test_fs"));
    int fd2 = fs_open("hai.txt");
    char n[7];
    printf("Bytes read: %d\n", fs_read(fd2, n, 7));
    printf("String: %s\n", n);
}