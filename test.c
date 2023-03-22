#include "disk.h"
#include "fs.h"
#include <stdio.h>

int main(){
    printf("%d\n", make_fs("test_fs"));
    printf("%d\n", mount_fs("test_fs"));
    printf("%d\n", fs_create("hai.txt"));
    printf("%d\n", fs_open("m"));
    int fd = fs_open("hai.txt");
    int fd2 = fs_open("hai.txt");
    printf("%d %d\n", fd, fd2);
    printf("%d\n", fs_close(fd));
    char buf[10];
    char * m = "hello!";
    printf("%d\n", fs_write(fd2, m, 7));
    printf("%d\n", fs_read(fd2, buf, 7));
    printf("%s\n", buf);
    
    return 0;
}
