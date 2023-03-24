/*************************************************************************
    > File Name: t.c
    > Author: gnblao
    > Mail: gnbalo
    > Created Time: 2023年03月24日 星期五 03时40分39秒
 ************************************************************************/

#include<stdio.h>
#include "bitmap.h"

#define XXX_SIZE 64

BMP_DECLARE(bmp, XXX_SIZE);

void pr(unsigned long *bmp) {
    for (unsigned long i=0; i< XXX_SIZE; i++) {
        if (bitmap_test_bit(bmp, i)) 
            printf("1");
        else
            printf("0");
    }
    
    printf("\n");
}

int main(int argc, char **argv) {
    bitmap_fill(bmp, XXX_SIZE);
    bitmap_clear_bit(bmp, 7);
    printf("idx:%u\n", bitmap_find_first_zero_bit(bmp, 0, XXX_SIZE));
    
    //bitmap_zero(bmp, XXX_SIZE);
    //bitmap_set_bit(bmp, 7); 
    //printf("idx:%d\n", bitmap_find_first_bit(bmp, 0, XXX_SIZE));
	pr(bmp);
    return 0;
}
