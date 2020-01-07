/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"
// Added
#include "defines.h"
#include <errno.h>

size_t calculate_size_of_block(size_t size) {
	//size_t minimum_required = 32;
	size_t block_size = 0;
	size_t footer_size = 8;
	size_t header_size = 8;
	size_t next_prev_size = 16;

	block_size += footer_size;
	block_size += header_size;
	size_t payload;
	if(size < 16)
		payload = next_prev_size;
	else
		if((size%16) > 0)
			payload = ((size/16) + 1) * 16;
		else
			payload = size;

	block_size += payload;
	return block_size;
}


/* Find the appropriate index from the size list that fits block_size given */
size_t get_sizelist_index(size_t block_size) {

	size_t pow2 = 32;
	size_t prevPow2 = 0;
	for(size_t i = 0; i<NUM_FREE_LISTS-1; i++){
		if( (block_size <= pow2)  &&  (block_size > prevPow2) )
			return i;
		prevPow2 = pow2;
		pow2 = (pow2<<1);
	}

	// Indirectly  if( block_size > prevPow2 )
	return NUM_FREE_LISTS-1;

}

void remove_block(sf_block *alloc_block) {
        sf_block *prev_block = alloc_block->body.links.prev;
        sf_block *next_block = alloc_block->body.links.next;
        prev_block->body.links.next = next_block;
        next_block->body.links.prev = prev_block;

        //change_allocation_bits(alloc_block);
}


void free_list_insert(sf_block* block_start) {
	//block_start = (sf_block *)block_start;
	
	size_t block_size = ((block_start->header) & BLOCK_SIZE_MASK);
	// Changing next block prev bit to 0 (free)
	sf_block *nextblock_start = (sf_block *)(((char *)(block_start)) + block_size);
	nextblock_start->header = ( (nextblock_start->header) & (size_t)(~1) );
	size_t next_block_size = ((nextblock_start->header) & BLOCK_SIZE_MASK);
	if(next_block_size != 0) {
		sf_block *next_nextblock_start = (sf_block *)(((char *)(nextblock_start)) + next_block_size);
		next_nextblock_start->prev_footer = ( (nextblock_start->header)^sf_magic() );
	}

	size_t free_list_index = get_sizelist_index(block_size);

	if((sf_free_list_heads[free_list_index].body.links.next == &sf_free_list_heads[free_list_index]) &&\
	  (sf_free_list_heads[free_list_index].body.links.prev == &sf_free_list_heads[free_list_index]) ) {
		
		block_start->body.links.next = sf_free_list_heads[free_list_index].body.links.next;
		block_start->body.links.prev = sf_free_list_heads[free_list_index].body.links.prev;
		sf_free_list_heads[free_list_index].body.links.next = block_start;
		sf_free_list_heads[free_list_index].body.links.prev = block_start;
		
		//printf("free_list_insert");		
		//printf("+++++++++++ HEAP +++++++++++++++");
		//sf_show_heap();
		return;
	}
	sf_block *next_bl = (sf_block *)( sf_free_list_heads[free_list_index].body.links.next );
	block_start->body.links.next = sf_free_list_heads[free_list_index].body.links.next;
	block_start->body.links.prev = &sf_free_list_heads[free_list_index];

	sf_free_list_heads[free_list_index].body.links.next = block_start;
	next_bl->body.links.prev = block_start;

	//printf("+++++++++++ HEAP +++++++++++++++");
        //sf_show_heap();

	return;

}

/* Return pointer to the bestfit free list, If not found return -1 */
//void get_free_list_block(size) {
// Iterate through all size lists from start and find the best fit
//}

void create_prologue(char *heap_start) {
	sf_prologue *prologue = (sf_prologue *)heap_start;
	prologue->padding1 = 0; /* Alignment padding */
	prologue->header = PACK(2*DSIZE, 3); /* Prologue header */
	prologue->unused1 = (size_t *)0; /* Alignment padding */
	prologue->unused2 = (size_t *)0; /* Alignment padding */
	prologue->footer = (PACK(2*DSIZE, 3) )^( sf_magic() ); /* Prologue footer Xor'd */

}


void extend_epilogue(char *heap_end, size_t alloc) {
	heap_end = heap_end - (1*WSIZE);
	sf_epilogue *epilogue = (sf_epilogue *)(heap_end);
	epilogue->header = PACK(0, alloc); /* Epilogue header */
}

void create_epilogue(char *heap_end, size_t alloc) {
	heap_end = heap_end - (1*WSIZE);
	sf_epilogue *epilogue = (sf_epilogue *)(heap_end);
	epilogue->header = PACK(0, alloc); /* Epilogue header */
}

void create_free_block(sf_block *block_start, size_t block_size) {
	size_t alloc = GET_ALLOC((block_start->prev_footer)^sf_magic());
	alloc = (alloc>>1);
	block_start->header = PACK(block_size, alloc);               /* block header */
	block_start->body.links.next = (sf_block *)PACK((size_t)block_start, 0); /* next pointer */
	block_start->body.links.prev = (sf_block *)PACK((size_t)block_start, 0); /* prev pointer */
	char *block_footer = ((char *)(block_start)) + block_size;
	PUT(block_footer, ( PACK(block_size, alloc) )^( sf_magic() ));   /* footer Xor'd */

}


void *first_call_malloc() {

	char *heap_start;
	if( NULL == (heap_start = sf_mem_grow()) )    		  	 // Creating extra memory
		return NULL; 					  	 // errno initialized to ENOMEM

	char *heap_end = sf_mem_end();
	create_prologue(heap_start);
	create_epilogue(heap_end, 2);
	
	size_t block_size = PAGE_SZ - (6*WSIZE);
	sf_block *block_start = (sf_block *)(heap_start + (4*WSIZE));	 // Pointing to footer of prologue for prev_footer
	create_free_block(block_start, block_size);
	free_list_insert(block_start);   			  	 //assign the remainder to the freelist as a single block
	return block_start;
}



void *coalesce(sf_block *bp) {
	size_t prev_alloc = GET_ALLOC( (bp->prev_footer)^sf_magic() );
	size_t next_alloc = GET_ALLOC( HDRP(NEXT_BLKP(bp)) );		 // If required replace with HDRP
	size_t size = GET_SIZE(bp->header);
	
	//size_t prev_size_1 = (size_t)GET_SIZE( (bp->prev_footer)^sf_magic() );
	//size_t next_size_1 = (size_t)GET_SIZE( HDRP(NEXT_BLKP(bp)) );


	//printf("prev_alloc: %ld   next_alloc %ld size %ld next_size_1 %ld prev %ld\n", prev_alloc, next_alloc, size, next_size_1, prev_size_1);

	if (prev_alloc && next_alloc) { /* Case 1 */
		free_list_insert(bp);	// Inserting the block
		return bp;
	}

	else if (prev_alloc && !next_alloc) { /* Case 2 */
		size_t curr_size = size;
		size_t next_size = (size_t)GET_SIZE( HDRP(NEXT_BLKP(bp)) );
		size = curr_size + next_size;
		
		// Extra added 0 ---> prev-alloc
		size_t prev_alloc = GET_PREV_ALLOC( bp->header );
		bp->header = PACK(size, prev_alloc);

		size_t *bp_footer = (size_t *)(((char *)(bp)) + size);
		PUT(bp_footer, ( (bp->header)^sf_magic() ));

		//We can write a test case here 2 test this case (prev_block allocated next_block free
		sf_block *nextblock_start = (sf_block *)(((char *)(bp)) + curr_size);

		// Removing previous block and adding new free block to the free list
                remove_block(nextblock_start);       // Nothing but nextblock start pointer
                free_list_insert(bp);
	}

	else if (!prev_alloc && next_alloc) { /* Case 3 */
		size_t curr_size = size;
		size_t prev_size = (size_t)GET_SIZE( (bp->prev_footer)^sf_magic() );
		size += prev_size;

		// Added prev_prev_alloc reversed header and footer allocation positions
		size_t prev_prev_alloc = GET_PREV_ALLOC( (bp->prev_footer)^sf_magic() );
		sf_block *bp_prev = (sf_block *)(((char *)(bp)) - prev_size);
                bp_prev->header = PACK(size, prev_prev_alloc);

		size_t *bp_footer = (size_t *)(((char *)(bp)) + curr_size); 		// Can replace with FTRP
		PUT(bp_footer, ( (bp_prev->header)^sf_magic() ));

		// Removing previous block and adding new free block to the free list
		remove_block(bp_prev);
	        free_list_insert(bp_prev);
		
		// Check for the size and replace the block in desired free_list (front)
		// If it is in the same free_list should we should put in the front of the same free_list
		bp = bp_prev;
	}

	else { /* Case 4 */
		size_t curr_size = size;
		size_t prev_size = (size_t)GET_SIZE( (bp->prev_footer)^sf_magic() );
		size_t next_size = (size_t)GET_SIZE( HDRP(NEXT_BLKP(bp)) );
		size = prev_size + curr_size + next_size;
		
		// Position replace of header and footer replaced prev_prev_alloc
		size_t prev_prev_alloc = GET_PREV_ALLOC( (bp->prev_footer)^sf_magic() );

		sf_block *bp_prev = (sf_block *)(((char *)(bp)) - prev_size);
		bp_prev->header = PACK(size, prev_prev_alloc);

		size_t *bp_new_footer = (size_t *)(((char *)(bp)) + curr_size + next_size); // Can replace with FTRP
                PUT(bp_new_footer, ( (bp_prev->header)^sf_magic() ));
		
		
		sf_block *nextblock_start = (sf_block *)(((char *)(bp)) + curr_size);

                // Removing previous block and adding new free block to the free list
		//printf("removing prev and next to add all combinely");
                remove_block(nextblock_start);       // Nothing but nextblock start pointer
		remove_block(bp_prev);       	     // Nothing but prevblock footer
                free_list_insert(bp_prev);
		
		//sf_show_heap();
		bp = bp_prev;
	}
	return bp;
}


/* request more mem that satisfies block_size required If not sufficient iteratively
   increase the memory. Use the function sf_mem_grow.  

   Coalesce is required to the newly allocated page with any free block immediately
   preceding it, in order to build blocks larger than one page.  
   Insert the new block at the beginning of the appropriate free list.*/
void *request_more_memory() {

	char *heap_start;
	if( NULL == (heap_start = sf_mem_grow()) ) { 				// Creating extra memory
		//printf("11\n");
		return NULL;							// errno initialized to ENOMEM
	}
	//printf("00\n");

	char *heap_end = sf_mem_end();
	extend_epilogue(heap_end, 2);
	
	size_t block_size = PAGE_SZ;
	sf_block *block_start = (sf_block *)(heap_start - (2*WSIZE));    	// Pointing to footer of prologue for prev_footer
	create_free_block(block_start, block_size);
	sf_block *coalesce_block_start = (sf_block *)coalesce(block_start);
	return coalesce_block_start;

}

void change_allocation_bits(sf_block *pointer) {
	size_t block_size = ((pointer->header) & BLOCK_SIZE_MASK);
        size_t alloc = THIS_BLOCK_ALLOCATED;
        alloc = alloc | ((pointer->header) & PREV_BLOCK_ALLOCATED);
        pointer->header = PACK(block_size, alloc);

        size_t *pointer_footer = (size_t *)( ((char *)(pointer)) + block_size );
        PUT(pointer_footer, ( (pointer->header)^sf_magic() ));
}



/* Get the pointer to the empty block in the already existing list
   Also iterate from the sizelist_index provided to the maximum sizelist_index to find
   free blocks to assign. Check against the size of block required and already present there 

   @return -1 if no block is found, else pointer to that block
*/
void *traverse_size_nine(size_t block_size) {
	// Check in the last size class along with the size if size is more intimate that to prev func or call extra memory here itself
	size_t i = NUM_FREE_LISTS-1;
	sf_block *sentinel = &sf_free_list_heads[i];
	sf_block *temp = sf_free_list_heads[i].body.links.next;
	while(temp != sentinel) {
		size_t alloc = ((temp->header) & THIS_BLOCK_ALLOCATED);
		if (alloc == 0){
			size_t curr_block_size = ((temp->header) & BLOCK_SIZE_MASK);
			if(block_size <= curr_block_size)
				return temp;
		}
		temp = temp->body.links.next;
	}
	return NULL;
}


void *traverse_get_ptr(size_t sizelist_index, size_t block_size) {
	for(size_t i = sizelist_index; i<NUM_FREE_LISTS-1; i++){
		sf_block *sentinel = &sf_free_list_heads[i];
		sf_block *temp = sf_free_list_heads[i].body.links.next;
		while(temp != sentinel) {
			size_t alloc = ((temp->header) & THIS_BLOCK_ALLOCATED);
			if (alloc == 0){
				// Changed
				size_t curr_block_size = ((temp->header) & BLOCK_SIZE_MASK);
				if(block_size <= curr_block_size){
					remove_block(temp);
					change_allocation_bits(temp);
					return temp;
				}
			}
			temp = temp->body.links.next;
		}
	}

	sf_block *temp = traverse_size_nine(block_size);
	if(temp != NULL){
		remove_block(temp);
		change_allocation_bits(temp);
		return temp;
	}

	// If it came here tackle extra memory
	//sf_show_heap();
	//printf("111\n");
	while(1){
		//sf_show_heap();
		//printf("111''''''''''''''''''\n");
		temp = (sf_block *)request_more_memory();
		if(temp == NULL)
			return NULL;
		size_t curr_block_size = ((temp->header) & BLOCK_SIZE_MASK);
		if(block_size <= curr_block_size){
			remove_block(temp);
			change_allocation_bits(temp);
			return temp;
		}
	}

}


void initialize_sentinel() {
	for(size_t i = 0; i<NUM_FREE_LISTS; i++){
		sf_block *sentinel = &sf_free_list_heads[i];
		sf_free_list_heads[i].body.links.next = sentinel;
                sf_free_list_heads[i].body.links.prev = sentinel;
        }
}


// Split the block and place it in appropriate sizelist at the start
void split_block(sf_block *pointer, size_t block_size, size_t extra_size) {
	size_t alloc = ((pointer->header) & THIS_BLOCK_ALLOCATED);
	alloc = alloc | ((pointer->header) & PREV_BLOCK_ALLOCATED);
	pointer->header = PACK(block_size, alloc);

	size_t *pointer_footer = (size_t *)( ((char *)(pointer)) + block_size );
	PUT(pointer_footer, ( (pointer->header)^sf_magic() ));

	sf_block *block_start = (sf_block *)(pointer_footer);
	create_free_block(block_start, extra_size);
	coalesce(block_start);
	//free_list_insert(block_start);
}




void *sf_malloc(size_t size) {
	if(size == 0)
		return NULL;  // Without setting errno
	
	// Check for highest block in free list size and check whether it has any values inside
	if((char *)sf_mem_start() == (char *)sf_mem_end()){
		initialize_sentinel();
		char *ptr = first_call_malloc();
		if (ptr == NULL){
                	if(sf_errno != ENOMEM)
                        	sf_errno = ENOMEM;
                	return NULL;
        	}

	}

	size_t block_size = calculate_size_of_block(size); 				// Find the size of block
	size_t sizelist_index = get_sizelist_index(block_size);
	sf_block *pointer = traverse_get_ptr(sizelist_index, block_size); 	// add extra page Handled in traverse_get_ptr

	if (pointer == NULL){
		if(sf_errno != ENOMEM)
			sf_errno = ENOMEM;
		return NULL;
	}

	size_t given_block_size = ((pointer->header) & BLOCK_SIZE_MASK);
	size_t extra_size = given_block_size - block_size;
	if(given_block_size > block_size &&  extra_size>=32  )
		split_block(pointer, block_size, extra_size);				//Scope for splitting

	//printf("-----------free_lists----------------");
	//sf_show_free_lists();
	//printf("+++++++++++ HEAP +++++++++++++++");
	//printf("bbb");
	//sf_show_heap();
	//printf("!!!!!!!!!! Block !!!!!!!!!!!!!");
	//sf_show_block(pointer);
	//1sf_show_blocks();


    	return pointer->body.payload;
}

void sf_free(void *pp) {
    
	sf_block *block_p = (sf_block *) ((size_t *)(pp) - 2);
	if((size_t *)pp==NULL)			// Pointer null check
		abort();

	size_t allocated_bit = GET_ALLOC( block_p->header );
	if(allocated_bit == 0)		// Allocation bit set check
		abort();

	size_t block_size = GET_SIZE( block_p->header );

	if(block_size < 32)		// BlockSize
		abort();

	size_t *block_footer = (size_t *)( (char *)block_p + block_size );

	sf_prologue *prologue = (sf_prologue *)(char *)sf_mem_start();				// Pointer to prologue
	sf_epilogue *epilogue = (sf_epilogue *)(((size_t *)sf_mem_end()) - 1);		// Pointer to epilogue

	//The header of the block is before the end of the prologue, or the footer of the block is after the beginning of the epilogue.
	//size_t p_diff = (size_t)( (size_t *)(&block_p->header) - (size_t *)(&prologue->footer ) );
	//size_t f_diff = (size_t)( (size_t *)(block_footer) - (size_t *)(&epilogue->header) );
	//long int p_diff = (long int)( (size_t *)(&block_p->header) - (size_t *)(&prologue->footer ) );
        //long int f_diff = (long int)( (size_t *)(block_footer) - (size_t *)(&epilogue->header) );
	//if( ( p_diff < 1 ) || ( f_diff < 1 ) ){	// Above test
	//printf("prologue_diff: %ld  epilogue_diff: %ld\n", p_diff, f_diff);

	if ( ( (size_t *)(&block_p->header) <= (size_t *)(&prologue->footer ) ) ||  ( (size_t *)(&epilogue->header) <= (size_t *)(block_footer) ) )
		abort();
    	

	size_t prev_allocated_bit = GET_PREV_ALLOC( block_p->header );
	size_t prev_alloc_from_footer = GET_ALLOC( ((block_p->prev_footer)^sf_magic()) );

	if(prev_allocated_bit != (prev_alloc_from_footer>>1) )		// Check of prev_alloc bit from header and prev_footer
		abort();

	if( (block_p->header) != ( (*block_footer)^sf_magic() ) )	// Check whether header and footer are same
		abort();

	block_p->header = (size_t)( (block_p->header) ^ (0x2) );	// Frreing the block at header
	PUT(block_footer, ( (block_p->header)^sf_magic() ));		// Frreing the block at footer


        
	coalesce(block_p); 						// Coalesce and insert in desired place

	//printf("aaa");
	//sf_show_heap();

    return;
}

void *sf_realloc(void *pp, size_t rsize) {

	sf_block *block_p = (sf_block *) ((size_t *)(pp) - 2);
        if((size_t *)pp==NULL){                  // Pointer null check
		sf_errno = EINVAL;
         	return NULL;
	}

        size_t allocated_bit = GET_ALLOC( block_p->header );
        if(allocated_bit == 0) {         // Allocation bit set check
         	sf_errno = EINVAL;
                return NULL;
	}

        size_t block_size = GET_SIZE( block_p->header );

        if(block_size < 32){             // BlockSize
        	sf_errno = EINVAL;
                return NULL; 
	}

        size_t *block_footer = (size_t *)( (char *)block_p + block_size );

        sf_prologue *prologue = (sf_prologue *)(char *)sf_mem_start();                  // Pointer to prologue
        sf_epilogue *epilogue = (sf_epilogue *)(((size_t *)sf_mem_end()) - 1);          // Pointer to epilogue

        //The header of the block is before the end of the prologue, or the footer of the block is after the beginning of the epilogue.
        //size_t p_diff = (size_t)( (size_t *)(&block_p->header) - (size_t *)(&prologue->footer ) );
        //size_t f_diff = (size_t)( (size_t *)(block_footer) - (size_t *)(&epilogue->header) );

        //if( ( p_diff < 1 ) || ( f_diff < 1 ) )  // Above test
	

	if ( ( (size_t *)(&block_p->header) <= (size_t *)(&prologue->footer ) ) ||  ( (size_t *)(&epilogue->header) <= (size_t *)(block_footer) ) ) {
		sf_errno = EINVAL;
                return NULL;
        }

        size_t prev_allocated_bit = GET_PREV_ALLOC( block_p->header );
        size_t prev_alloc_from_footer = GET_ALLOC( ((block_p->prev_footer)^sf_magic()) );

        if(prev_allocated_bit != (prev_alloc_from_footer>>1) ) {         // Check of prev_alloc bit from header and prev_footer
         	sf_errno = EINVAL;
                return NULL;
	}

        if( (block_p->header) != ( (*block_footer)^sf_magic() ) ){       // Check whether header and footer are same
         	sf_errno = EINVAL;
                return NULL;
	}

	if( rsize == 0){

		sf_free(pp);
		//block_p->header = (size_t)( (block_p->header) ^ (0x2) );        // Frreing the block
		// Added
		//PUT(block_footer, ( (block_p->header)^sf_magic() ));            // Frreing the block at footer
	        //coalesce(block_p);                                              // Coalesce and insert in desired place
	        //printf("ccc");
		return NULL;
	}

	size_t new_block_size = calculate_size_of_block(rsize);                  // Find the size of block
	if( new_block_size > block_size ){
		char *memory = (char *)sf_malloc(rsize);
		if( memory == NULL ){
			return NULL;
		}
		memcpy( memory, (char *)pp, (block_size - 16) );
		sf_free(pp);
		//sf_show_heap();
		return memory;
	}
	else{
		if( (block_size - new_block_size) < 32 )		// new_block_size == block_size taken care here
			return pp;
		else{
	        	size_t extra_size = block_size - new_block_size;
                	split_block(block_p, new_block_size, extra_size);                           //Scope for splitting
			return pp;
		}

	}

}

