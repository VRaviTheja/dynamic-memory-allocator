#ifndef DEFINES_H
#define DEFINES_H
#include "sfmm.h"

#define WSIZE 8 /* Word and header/footer size (bytes) */
#define DSIZE 16 /* Double word size (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))
/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(size_t *)(p))
#define PUT(p, val) (*(size_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) ((p) & ~0x3)
#define GET_PREV_ALLOC(p) ((p) & 0x1)
#define GET_ALLOC(p) ((p) & 0x2)


/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) (*(size_t *)((char *)(bp) + WSIZE)) // Can use
#define FTRP(bp) ((char *)(bp) + GET_SIZE(((sf_block *)(bp))->header) - DSIZE) // Can use

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((sf_block *)(bp))->header))  // Can use
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

// For unit tests purpose
extern size_t calculate_size_of_block(size_t);
extern size_t get_sizelist_index(size_t);


#endif
