#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#define BLOOM_SZ 5000
extern int bloom_bitmap[BLOOM_SZ];

#define set(bloom_filter,index)     ( bloom_filter[(index / 32)] |= (1 << (index % 32)) )         
#define test(bloom_filter,index)    ( bloom_filter[(index / 32)] & (1 << (index % 32)) )

void bf_insert(char* key);
int bf_search(char* key);

unsigned hash1(const char* str, int len);
unsigned hash2(const char* str, int len);
unsigned hash3(const char* str, int len);
unsigned hash4(const char* str, int len);
unsigned hash5(const char* str, int len);
unsigned hash6(const char* str, int len);
unsigned hash7(const char* str, int len);