#include <criterion/criterion.h>
#include <errno.h>
#include <signal.h>
#include "sfmm.h"
// Added
#include "defines.h"
#include "debug.h"

#define MIN_BLOCK_SIZE (32)

void assert_free_block_count(size_t size, int count);
void assert_free_list_block_count(size_t size, int count);

/*
 * Assert the total number of free blocks of a specified size.
 * If size == 0, then assert the total number of all free blocks.
 */
void assert_free_block_count(size_t size, int count) {
    int cnt = 0;
    for(int i = 0; i < NUM_FREE_LISTS; i++) {
	sf_block *bp = sf_free_list_heads[i].body.links.next;
	while(bp != &sf_free_list_heads[i]) {
	    if(size == 0 || size == (bp->header & BLOCK_SIZE_MASK))
		cnt++;
	    bp = bp->body.links.next;
	}
    }
    cr_assert_eq(cnt, count, "Wrong number of free blocks of size %ld (exp=%d, found=%d)",
		 size, count, cnt);
}

Test(sf_memsuite_student, malloc_an_Integer_check_freelist, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	int *x = sf_malloc(sizeof(int));

	cr_assert_not_null(x, "x is NULL!");

	*x = 4;

	cr_assert(*x == 4, "sf_malloc failed to give proper space for an int!");

	assert_free_block_count(0, 1);
	assert_free_block_count(4016, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
	cr_assert(sf_mem_start() + PAGE_SZ == sf_mem_end(), "Allocated more than necessary!");
}

Test(sf_memsuite_student, malloc_three_pages, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	void *x = sf_malloc(3 * PAGE_SZ - 2 * sizeof(sf_block));

	cr_assert_not_null(x, "x is NULL!");
	assert_free_block_count(0, 0);
	cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

Test(sf_memsuite_student, malloc_over_four_pages, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	void *x = sf_malloc(PAGE_SZ << 2);

	cr_assert_null(x, "x is not NULL!");
	assert_free_block_count(0, 1);
	assert_free_block_count(16336, 1);
	cr_assert(sf_errno == ENOMEM, "sf_errno is not ENOMEM!");
}

Test(sf_memsuite_student, free_quick, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	/* void *x = */ sf_malloc(8);
	void *y = sf_malloc(32);
	/* void *z = */ sf_malloc(1);

	sf_free(y);

	assert_free_block_count(0, 2);
	assert_free_block_count(3936, 1);
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, free_no_coalesce, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	/* void *x = */ sf_malloc(8);
	void *y = sf_malloc(200);
	/* void *z = */ sf_malloc(1);

	sf_free(y);

	assert_free_block_count(0, 2);
	assert_free_block_count(224, 1);
	assert_free_block_count(3760, 1);
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, free_coalesce, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	/* void *w = */ sf_malloc(8);
	void *x = sf_malloc(200);
	void *y = sf_malloc(300);
	/* void *z = */ sf_malloc(4);

	sf_free(y);
	sf_free(x);

	assert_free_block_count(0, 2);
	assert_free_block_count(544, 1);
	assert_free_block_count(3440, 1);
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, freelist, .init = sf_mem_init, .fini = sf_mem_fini) {
	void *u = sf_malloc(200);
	/* void *v = */ sf_malloc(300);
	void *w = sf_malloc(200);
	/* void *x = */ sf_malloc(500);
	void *y = sf_malloc(200);
	/* void *z = */ sf_malloc(700);

	sf_free(u);
	sf_free(w);
	sf_free(y);

	assert_free_block_count(0, 4);
	assert_free_block_count(224, 3);
	assert_free_block_count(1808, 1);

	// First block in list should be the most recently freed block.
	int i = 3;
	sf_block *bp = sf_free_list_heads[i].body.links.next;
	cr_assert_eq(bp, (char *)y - 2*sizeof(sf_header),
		     "Wrong first block in free list %d: (found=%p, exp=%p)",
                     i, bp, (char *)y - 2*sizeof(sf_header));
}

Test(sf_memsuite_student, realloc_larger_block, .init = sf_mem_init, .fini = sf_mem_fini) {
	void *x = sf_malloc(sizeof(int));
	/* void *y = */ sf_malloc(10);
	x = sf_realloc(x, sizeof(int) * 10);

	cr_assert_not_null(x, "x is NULL!");
	sf_block *bp = (sf_block *)((char *)x - 2*sizeof(sf_header));
	cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((bp->header & BLOCK_SIZE_MASK) == 64, "Realloc'ed block size not what was expected!");

	assert_free_block_count(0, 2);
	assert_free_block_count(3920, 1);
}

Test(sf_memsuite_student, realloc_smaller_block_splinter, .init = sf_mem_init, .fini = sf_mem_fini) {
	void *x = sf_malloc(sizeof(int) * 8);
	void *y = sf_realloc(x, sizeof(char));

	cr_assert_not_null(y, "y is NULL!");
	cr_assert(x == y, "Payload addresses are different!");

	sf_block *bp = (sf_block *)((char*)y - 2*sizeof(sf_header));
	cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((bp->header & BLOCK_SIZE_MASK) == 48, "Block size not what was expected!");

	// There should be only one free block of size 4000.
	assert_free_block_count(0, 1);
	assert_free_block_count(4000, 1);
}

Test(sf_memsuite_student, realloc_smaller_block_free_block, .init = sf_mem_init, .fini = sf_mem_fini) {
	void *x = sf_malloc(sizeof(double) * 8);
	void *y = sf_realloc(x, sizeof(int));

	cr_assert_not_null(y, "y is NULL!");

	sf_block *bp = (sf_block *)((char*)y - 2*sizeof(sf_header));
	cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((bp->header & BLOCK_SIZE_MASK) == 32, "Realloc'ed block size not what was expected!");

	// After realloc'ing x, we can return a block of size 48 to the freelist.
	// This block will go into the main freelist and be coalesced.
	assert_free_block_count(0, 1);
	assert_free_block_count(4016, 1);
}

//############################################
//STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
//DO NOT DELETE THESE COMMENTS
//############################################

Test(sf_memsuite_student, free_epilogue_check, .init = sf_mem_init, .fini = sf_mem_fini, .signal = SIGABRT) {
    	sf_epilogue *epilogue = (sf_epilogue *)(((size_t *)sf_mem_end()) - 1);    // Pointer to epilogue
	sf_free(&epilogue->header);
}

Test(sf_memsuite_student, free_prologue_check, .init = sf_mem_init, .fini = sf_mem_fini, .signal = SIGABRT) {
        sf_prologue *prologue = (sf_prologue *)(char *)sf_mem_start();            // Pointer to prologue
        sf_free(&prologue->header);
}

Test(sf_memsuite_student, free_prologue_negative_check, .init = sf_mem_init, .fini = sf_mem_fini, .signal = SIGABRT) {
        sf_prologue *prologue = (sf_prologue *)(char *)sf_mem_start();            // Pointer to prologue
	//size_t *free_ptr = (size_t *)((size_t *)(&prologue->header) - 1);
        sf_free(&prologue->footer);
}

Test(sf_memsuite_student, free_epilogue_two_negative_check, .init = sf_mem_init, .fini = sf_mem_fini, .signal = SIGABRT) {
	sf_epilogue *epilogue = (sf_epilogue *)(((size_t *)sf_mem_end()) - 1);    // Pointer to epilogue
        size_t *free_ptr = (size_t *)((size_t *)(&epilogue->header) + 1);
        sf_free(free_ptr);
}




Test(sf_memsuite_student, free_null_check, .init = sf_mem_init, .fini = sf_mem_fini, .signal = SIGABRT) {
        sf_free(NULL);
}

Test(sf_memsuite_student, coalesce_next_check, .init = sf_mem_init, .fini = sf_mem_fini) {
	void *u = sf_malloc(10);
	void *w = sf_malloc(10);
	sf_malloc(10);

	assert_free_block_count(0, 1);
	
	//printf("before middle freeing\n");
	sf_free(w);
	assert_free_block_count(0, 2);
	//printf("after middle assert freeing\n");

	sf_free(u);
	//printf("affter top freeing\n");
	assert_free_block_count(0, 2);
	//printf("affter top freeing assert\n");


	assert_free_block_count(64, 1);
	assert_free_block_count(3952, 1);

	// First block in list should be the most recently freed block.
	int i = 1;
	sf_block *bp = sf_free_list_heads[i].body.links.next;
	cr_assert_eq(bp, (char *)u - 2*sizeof(sf_header),
		     "Wrong first block in free list %d: (found=%p, exp=%p)",
                     i, bp, (char *)u - 2*sizeof(sf_header));

}


Test(sf_memsuite_student, coalesce_prev_next_check, .init = sf_mem_init, .fini = sf_mem_fini) {
        void *u = sf_malloc(10);
        void *w = sf_malloc(10);
        void *y = sf_malloc(10);
	sf_malloc(10);

        assert_free_block_count(0, 1);

        //printf("before top freeing\n");
        sf_free(u);
        assert_free_block_count(0, 2);
        //printf("after top assert freeing\n");


        sf_free(y);
        //printf("affter last buttom freeing\n");
        assert_free_block_count(0, 3);
        //printf("affter last buttom freeing assert\n");


	sf_free(w);
        //printf("affter middle freeing\n");
        assert_free_block_count(0, 2);
        //printf("affter middle freeing assert\n");

        assert_free_block_count(96, 1);
        assert_free_block_count(3920, 1);

        // First block in list should be the most recently freed block.
        int i = 2;
        sf_block *bp = sf_free_list_heads[i].body.links.next;
        cr_assert_eq(bp, (char *)u - 2*sizeof(sf_header),
                     "Wrong first block in free list %d: (found=%p, exp=%p)",
                     i, bp, (char *)u - 2*sizeof(sf_header));

}


Test(sf_memsuite_student, coalesce_prev_next_check_combine_one, .init = sf_mem_init, .fini = sf_mem_fini) {
        void *u = sf_malloc(10);
        void *w = sf_malloc(10);
        void *y = sf_malloc(10);

        assert_free_block_count(0, 1);

        //printf("before top freeing\n");
        sf_free(u);
        assert_free_block_count(0, 2);
        //printf("after top assert freeing\n");


        sf_free(y);
        //printf("affter last buttom freeing\n");
        assert_free_block_count(0, 2);
        //printf("affter last buttom freeing assert\n");


        sf_free(w);
        //printf("affter middle freeing\n");
        assert_free_block_count(0, 1);
        //printf("affter middle freeing assert\n");

        assert_free_block_count(4048, 1);

        // First block in list should be the most recently freed block.
        int i = 7;
        sf_block *bp = sf_free_list_heads[i].body.links.next;
        cr_assert_eq(bp, (char *)u - 2*sizeof(sf_header),
                     "Wrong first block in free list %d: (found=%p, exp=%p)",
                     i, bp, (char *)u - 2*sizeof(sf_header));

}

Test(sf_memsuite_student, malloc_after_alloc_pages, .init = sf_mem_init, .fini = sf_mem_fini) {
        sf_errno = 0;
	void *x = sf_malloc(4032);
        void *y = sf_malloc(4080);
	void *z = sf_malloc(4081);
	
	sf_free(z);
	cr_assert_not_null(x, "x is NULL!");
	cr_assert_not_null(y, "x is NULL!");
        assert_free_block_count(0, 1);
        cr_assert(sf_errno == 0, "sf_errno is not 0!");

	// First block in list should be the most recently freed block.
        int i = 8;
        sf_block *bp = sf_free_list_heads[i].body.links.next;
        cr_assert_eq(bp, (char *)z - 2*sizeof(sf_header),
                     "Wrong first block in free list %d: (found=%p, exp=%p)",
                     i, bp, (char *)z - 2*sizeof(sf_header));
}


Test(sf_memsuite_student, malloc_after_two_alloc_pages, .init = sf_mem_init, .fini = sf_mem_fini) {
        sf_errno = 0;
        void *x = sf_malloc(4032);
        void *y = sf_malloc(4081);
        void *z = sf_malloc(4080);
        
	cr_assert_not_null(x, "x is NULL!");
        cr_assert_not_null(y, "x is NULL!");
	cr_assert_not_null(z, "x is NULL!");
        assert_free_block_count(0, 1);
        cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

Test(sf_memsuite_student, malloc_after_free_pages, .init = sf_mem_init, .fini = sf_mem_fini) {
        sf_errno = 0;
        void *x = sf_malloc(4032);
        void *y = sf_malloc(4080);
        void *z = sf_malloc(4080);

        sf_free(z);
	void *a = sf_malloc(4080);
	cr_assert_not_null(a, "a is NULL!");
        cr_assert_not_null(x, "x is NULL!");
        cr_assert_not_null(y, "x is NULL!");
        assert_free_block_count(0, 0);
        cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

Test(sf_memsuite_student, unit_test_block_finder, .init = sf_mem_init, .fini = sf_mem_fini) {
	size_t required = 83;
        cr_assert(calculate_size_of_block(required) == 112, "Size not calculated correctly");
}


Test(sf_memsuite_student, unit_test_index_finder, .init = sf_mem_init, .fini = sf_mem_fini) {
	size_t required = 80;
        cr_assert(get_sizelist_index(required) == 2, "Size not calculated correctly");
}

Test(sf_memsuite_student, malloc_free_multiple, .init = sf_mem_init, .fini = sf_mem_fini) {
        sf_errno = 0;
        void *x = sf_malloc(402);
        void *y = sf_malloc(40);
        void *z = sf_malloc(80);
	void *b = sf_malloc(900);
	void *c = sf_malloc(100);
	void *d = sf_malloc(800);
	void *e = sf_malloc(500);
	void *f = sf_malloc(300);


        sf_free(x);
	sf_free(y);
	sf_free(z);
	sf_free(b);
	sf_free(c);
	sf_free(d);
	sf_free(e);
	sf_free(f);
	
        assert_free_block_count(0, 1);
        cr_assert(sf_errno == 0, "sf_errno is not 0!");
}


Test(sf_memsuite_student, realloc_multiple, .init = sf_mem_init, .fini = sf_mem_fini) {
        sf_errno = 0;
	
        void *x = sf_malloc(402);
	cr_assert(sf_errno == 0, "sf_errno is not 0! 1");
        void *y = sf_malloc(40);
        void *z = sf_malloc(80);
        void *b = sf_malloc(900);
        void *c = sf_malloc(100);
	cr_assert(sf_errno == 0, "sf_errno is not 0! 2");
        void *d = sf_malloc(80);
        void *e = sf_malloc(500);
        void *f = sf_malloc(300);
	cr_assert(sf_errno == 0, "sf_errno is not 0! 3");

	x = sf_realloc(x, 40);
        y = sf_realloc(y, 400);
	cr_assert(sf_errno == 0, "sf_errno is not 0! 4");
        z = sf_realloc(z, 80);
	
        b = sf_realloc(b, 940);
        c = sf_realloc(c, 10);
        d = sf_realloc(d, 5000);
        e = sf_realloc(e, 50);
        f = sf_realloc(f, 0);


        sf_free(x);
        sf_free(y);
        sf_free(z);
        sf_free(b);
        sf_free(c);
        sf_free(d);
        sf_free(e);

	cr_assert_null(f, "f is not NULL!");

        assert_free_block_count(0, 1);
        cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

Test(sf_memsuite_student, realloc_invalid_epilogue, .init = sf_mem_init, .fini = sf_mem_fini) {
	 sf_errno = 0;
        sf_epilogue *epilogue = (sf_epilogue *)(((size_t *)sf_mem_end()) - 1);    // Pointer to epilogue
        size_t *free_ptr = (size_t *)((size_t *)(&epilogue->header) + 1);
        free_ptr = sf_realloc(free_ptr, 20);

	cr_assert(sf_errno == EINVAL, "sf_errno is not EINVAL!");
}


Test(sf_memsuite_student, realloc_invalid_mem_null, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
        void *x = sf_malloc(402);	

        x = sf_realloc(x, PAGE_SZ << 2);

	cr_assert_null(x, "x is not NULL!");
}

Test(sf_memsuite_student, realloc_invalid_null, .init = sf_mem_init, .fini = sf_mem_fini) {
        sf_errno = 0;
        void *x = NULL;

        x = sf_realloc(x, 20);

        cr_assert_null(x, "x is not NULL!");
}

Test(sf_memsuite_student, realloc_invalid_rsize_zero, .init = sf_mem_init, .fini = sf_mem_fini) {
        sf_errno = 0;
        void *x = sf_malloc(402);

        x = sf_realloc(x, 0);
	
	assert_free_block_count(0, 1);
	assert_free_block_count(4048, 1);
        cr_assert_null(x, "x is not NULL!");
}

