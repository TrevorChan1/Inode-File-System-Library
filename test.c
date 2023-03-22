#include "disk.h"
#include "fs.h"
#include <stdio.h>

int main(){
    printf("%d\n", make_fs("test_fs"));
    printf("%d\n", mount_fs("test_fs"));
    printf("%d\n", fs_create("hai.txt"));
    int fd = fs_open("hai.txt");
    int fd2 = fs_open("hai.txt");
    char buf[10];
    char * m = "hello!";
    char n[2];
    printf("%d\n", fs_write(fd, m, 7));
    for (int i = 0; i < 7; i++){
        printf("%d, ", fs_read(fd2, n, 1));
        printf("%s \n", n);
    }
    
    return 0;
}

// int main(){
//     printf("%d\n", make_fs("test_fs"));
//     printf("%d\n", mount_fs("test_fs"));
//     printf("%d\n", fs_create("hai.txt"));
//     for (int i = 0; i < 32; i++){
//         printf("%d\n", fs_open("hai.txt"));
//     }
// }
