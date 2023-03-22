#include "disk.h"
#include "fs.h"
#include <stdio.h>

int main(){
    printf("%d\n", make_fs("test_fs"));
    printf("%d\n", mount_fs("test_fs"));
    printf("%d\n", fs_create("1"));
    printf("%d\n", fs_create("2"));
    printf("%d\n", fs_create("3"));
    printf("%d\n", fs_create("4"));
    printf("%d\n", fs_create("5"));
    printf("%d\n", fs_create("6"));
    printf("%d\n", fs_create("7"));
    printf("%d\n", fs_create("8"));
    printf("%d\n", fs_create("9"));
    printf("%d\n", fs_create("10"));
    printf("%d\n", fs_create("11"));
    printf("%d\n", fs_create("12"));
    // int fd = fs_open("hai.txt");
    // int fd2 = fs_open("hai.txt");
    // int fd3 = fs_open("hai2.txt");
    // int fd4 = fs_open("hai3.txt");
        printf("%d\n", fs_open("1"));
        printf("%d\n", fs_open("2"));
        printf("%d\n", fs_open("3"));
        printf("%d\n", fs_open("4"));
        printf("%d\n", fs_open("5"));
        printf("%d\n", fs_open("6"));
        printf("%d\n", fs_open("7"));
        printf("%d\n", fs_open("8"));
        printf("%d\n", fs_open("9"));
        printf("%d\n", fs_open("10"));
        printf("%d\n", fs_open("11"));
        printf("%d\n", fs_open("12"));
    
    fs_close(0);
    fs_open("hai.txt");
    // char buf[10];
    // char * m = "hello!";
    // char * o = "heyhommmmm";
    // char n[2];
    // printf("%d\n", fs_write(fd, m, 7));
    // printf("%d\n", fs_write(fd, m, 7));
    // for (int i = 0; i < 7; i++){
    //     printf("%d, ", fs_read(fd2, n, 1));
    //     printf("%s \n", n);
    // }
    
    return 0;
}

