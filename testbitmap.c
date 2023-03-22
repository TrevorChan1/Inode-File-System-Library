#include <stdint.h>
#include <stdio.h>

// Bitwise helper function that takes a bitmap and returns nth bit (0 or 1)
int getNbit(uint8_t * bitmap, int size, int n){
    // If n is out of block number range, print error and do nothing
    if (n < 0 || n >= size){
        printf("ERROR: bitmap index out of bounds\n");
        return -1;
    }

    // Go into the n/8th block, and in that block go to the bit shift bits from the left
    int arrIndex = n / 8;
    int shift = n % 8;
    return ((bitmap[arrIndex] >> (7 - shift)) & 1);
}

// Bitwise helper function that takes bitmap and sets nth bit (to 0 or 1)
void setNbit(uint8_t * bitmap, int size, int n, int value){
    // If n is out of block number range, print error and do nothing
    if (n < 0 || n >= size){
        printf("ERROR: free block index out of bounds\n");
        return;
    }

    // Go into the n/8th block, and in that block go to the bit shift bits from the left
    int arrIndex = n / 8;
    int shift = n % 8;
    uint8_t mask;
    // If value is gonna set a 1, shift the 1 to the bit number and OR the mask with the number
    if (value){
        mask = 1;
        mask = mask << (7-shift);
        bitmap[arrIndex] = bitmap[arrIndex] | mask;
    }
    // If value is setting a 0, create mask of 1's and a 0 in the bit place, then AND with number
    else{
        mask = 255 - (128 >> shift);
        printf("mask: %d\n", mask);
        bitmap[arrIndex] = bitmap[arrIndex] & mask;
    }
}

// Bitwise helper function that finds first 1 in a bitmap (first free block number)
int find1stFree(uint8_t * bitmap, int size){
    // Iterate through all bits and return the first 1
    for (int i = 0; i < size; i++){
        if (getNbit(bitmap, size, i))
            return i;
    }
    // If no 1's are found, return -1 (no available block numbers)
    return -1;
}

int main(){
    uint8_t * test;
    *test = 255;

    for (int i = 0; i < 8; i++){
        setNbit(test, 8, i, 0);
        printf("%d \n", *test);
        for (int j = 0; j < 8; j++){
            printf("%d ", getNbit(test, 8, j));
        }
        printf("\n");
    }
    // uint8_t test[8];
    //     test[0] = 0;
    //     test[1] = 0;
    // for (int i = 0; i < 64; i++){
        
    //     setNbit(test, 64, i, 1);
    //     printf("%d %d\n", test[0], test[1]);
    //     for (int j = 0; j < 64; j++){
    //         printf("%d ", getNbit(test, 64, j));
    //     }
    //     printf("\n");

    //     printf("%d\n", find1stFree(test, 64));
    // }

}