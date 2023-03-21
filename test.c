#include "disk.h"
#include "fs.h"
#include <stdio.h>

int main(){
    printf("%d\n", make_fs("test_fs"));
    printf("%d\n", umount_fs("test_fs"));
    printf("%d\n", mount_fs("test_fs"));
    return 0;
}
