#include "disk.h"
#include "fs.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

int main(){

    pid_t m = fork();
    if (m == 0){
        printf("Make: %d\n", make_fs("test_fs"));
        printf("Mount: %d\n", mount_fs("test_fs"));
        printf("Create file: %d\n", fs_create("hai.txt"));
        int fd = fs_open("hai.txt");
        char * m = "test_file";
        printf("Bytes written: %d\n", fs_write(fd, m, 9));

        char ** files = NULL;
        printf("Files: %d\n", fs_listfiles(&files));
        for (int i = 0; i < 1; i++){
            printf("%p\n", files);
            printf("%s\n", files[i]);
        }
        printf("Close: %d\n", fs_close(fd));
        printf("Unmount: %d\n", umount_fs("test_fs"));
        return 0;
    }
    else{
        waitpid(m, 0, 0);
        printf("\n\n");
        printf("mount: %d\n", mount_fs("test_fs"));
        int fd2 = fs_open("hai.txt");
        char n[7];
        printf("Bytes read: %d\n", fs_read(fd2, n, 9));
        printf("String: %s\n", n);
        char ** files = NULL;
        printf("Files: %d\n", fs_listfiles(&files));
        for (int i = 0; i < 1; i++){
            printf("%p\n", files);
            printf("%s\n", files[i]);
        }
        printf("%d\n", memcmp(n, "test_file", 9));
    }
}