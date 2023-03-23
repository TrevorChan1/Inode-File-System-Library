#include "disk.h"
#include "fs.h"
#include <stdio.h>

int main(){
    printf("%d\n", make_fs("test_fs"));
    printf("%d\n", mount_fs("test_fs"));
    int f = fs_create("hai.txt");
    int f2 = fs_create("hai2.txt");
    int f3 = fs_create("hai3.txt");
    int fd = fs_open("hai.txt");
    int fd2 = fs_open("hai.txt");
    int fd3 = fs_open("hai2.txt");
    int fd4 = fs_open("hai3.txt");
    char buf[10];
    char * m = "hello!";
    char * o = "heyhommmmm";
    char n[2];
    printf("%d\n", fs_write(fd, m, 7));
    printf("%d\n", fs_write(fd, m, 7));
    for (int i = 0; i < 14; i++){
        printf("%d, ", fs_read(fd2, n, 1));
        printf("%s \n", n);
    }
    
    return 0;
}

