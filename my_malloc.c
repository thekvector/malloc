/*
 * CS 2110 Spring 2018
 * Author: Hisham Kodvavi
 */

/* we need this for uintptr_t */
#include <stdint.h>
/* we need this for memcpy/memset */
#include <string.h>
/* we need this to print out stuff*/
#include <stdio.h>
/* we need this for my_sbrk */
#include "my_sbrk.h"
/* we need this for the metadata_t struct and my_malloc_err enum definitions */
#include "my_malloc.h"

/* Our freelist structure - our freelist is represented as two doubly linked lists
 * the address_list orders the free blocks in ascending address
 * the size_list orders the free blocks by size
 */

metadata_t *address_list;
metadata_t *size_list;

static void *find_smallest_fit(size_t size);
static void addToSize(metadata_t *block);
static void addToAddress(metadata_t *block);
// static void *removeAddress(metadata_t *block);
static void removeSize(metadata_t *block);
/* Set on every invocation of my_malloc()/my_free()/my_realloc()/
 * my_calloc() to indicate success or the type of failure. See
 * the definition of the my_malloc_err enum in my_malloc.h for details.
 * Similar to errno(3).
 */
enum my_malloc_err my_malloc_errno;

static void *find_smallest_fit(size_t size) {
	metadata_t* curr  = size_list;
	while (curr != NULL && curr -> size < size) {
		curr = curr -> next_size;
	}
	return curr;
}

static void addToSize(metadata_t *block) {
	metadata_t* curr = size_list;
	if (curr -> next_size == NULL) {
		if (block -> size <= curr -> size) {
			block -> prev_size = NULL;
			block -> next_size = curr;
			curr -> prev_size = block;
			size_list = block;
		} else {
			curr -> next_size = block;
			block -> prev_size = curr;
			block -> next_size = NULL;
		}
	}
	metadata_t* prev = NULL;
	while (curr != NULL && block -> size > curr -> size) {
		prev = curr;
		curr = curr -> next_size;
	}
	if (curr == NULL) {
		if (block -> size <= prev -> size) {
			metadata_t *tempPrev = prev -> prev_size;
			tempPrev -> next_size = block;
			block -> prev_size = tempPrev;
			block -> next_size = prev;
			prev -> prev_size = block;
		} else {
			block -> next_size = NULL;
			prev -> next_size = block;
			block -> prev_size = prev;
		}
	} else {
		if (prev == NULL) { //block should be smallest thing in size list
			if (curr -> size < block -> size) {
				metadata_t *currNext = curr -> next_size;
				currNext -> prev_size = block;
				block -> prev_size = curr;
				curr -> next_size = block;
				block -> next_size = currNext;
			} else {
				curr -> prev_size = block;
				block -> prev_size = NULL;
				block -> next_size = curr;
				size_list = block;
			}
		} else {
			prev -> next_size = block;
			block -> prev_size = prev;
			block -> next_size = curr;
			curr -> prev_size = block;
		}
	}
}

static void addToAddress(metadata_t *block) {
	 metadata_t *curr = address_list;
	 metadata_t *prev = NULL;
	 while (curr != NULL && ((uintptr_t) block > (uintptr_t) curr)) {
	 	prev = curr;
	 	curr = curr -> next_addr;
	 }
	 if (prev == NULL) {
	 	block -> prev_addr = NULL;
	 	block -> next_addr = curr;
	 	address_list = block;
	 } else if (((char*) prev + prev -> size) == (char*) block) {
	 	prev -> size += block -> size;
	 	block = prev;
	 } else {
	 	prev -> next_addr = block;
	 	block -> prev_addr = prev;
	 	block -> next_addr = curr;
	 	curr -> prev_addr = block;
	 }
	 if ((metadata_t*) ((char*) block + block -> size) == curr) {
	 	block -> next_addr = curr -> next_addr;
	 	block -> size += curr -> size;
	 	removeSize(block);
	 	addToSize(block);
	 }
}

	

static void removeSize(metadata_t *block) {
	metadata_t *curr = size_list;
	metadata_t *prev = NULL;
	while (curr -> next_size != NULL && block -> size < curr -> size) {
		prev = curr;
		curr = curr -> next_size;
	}
	if (prev == NULL) {
		size_list = curr -> next_size;
		curr -> prev_size = NULL;
	} else if (curr -> next_size == NULL) {
		prev -> next_size = NULL;
	} else {
		prev -> next_size = curr -> next_size;
		metadata_t *next = curr -> next_size;
		next -> prev_size = prev;
	}
}

/* MALLOC
 * See my_malloc.h for documentation
 */
void *my_malloc(size_t size) {
	if (size == 0) {
		my_malloc_errno = NO_ERROR;
		return NULL;
	}
	size_t totalSize = size + TOTAL_METADATA_SIZE;
	if (totalSize > SBRK_SIZE) {
		my_malloc_errno = SINGLE_REQUEST_TOO_LARGE;
		return NULL;
	}
	metadata_t *block = find_smallest_fit(totalSize);
	while (!block) {
		block = (metadata_t*)my_sbrk(SBRK_SIZE);
		if (!block) {
			my_malloc_errno = OUT_OF_MEMORY;
			return NULL;
		} else {
			block -> size = SBRK_SIZE;
			block -> canary = ((uintptr_t)block ^ CANARY_MAGIC_NUMBER) + 1;
			*(unsigned long*)((uint8_t*)block + block -> size - sizeof(unsigned long)) = ((uintptr_t)block ^ CANARY_MAGIC_NUMBER) + 1;
			if (!size_list) {
				size_list = block;
				block -> prev_size = NULL;
				block -> next_size = NULL;
			} else {
				addToSize(block);
			}
			if (!address_list) {
				address_list = block;
				block -> prev_addr = NULL;
				block -> next_addr = NULL;
			} else {
				addToAddress(block);
			}
		}
		block = find_smallest_fit(totalSize);
	}

	if (block -> size == totalSize) {
		metadata_t *prevSize = block -> prev_size;
		metadata_t *nextSize = block -> next_size;
		if (!prevSize && !nextSize) {
			size_list = NULL;
			address_list = NULL;
		}
		if (!prevSize) {
			size_list = block -> next_size;
			nextSize -> prev_size = NULL;
		} else if (!nextSize) {
			prevSize -> next_size = NULL;
			prevSize = block -> prev_size;
		} else {
			prevSize = block -> prev_size;
			nextSize = block -> next_size;
			prevSize -> next_size = nextSize;
			nextSize -> prev_size = prevSize;
		}
		metadata_t *prevAddr = block -> prev_addr;
		metadata_t *nextAddr = block -> next_addr;
		if (!prevAddr && !nextAddr) {
			nextAddr = block -> next_addr;
			address_list = nextAddr;
			nextAddr -> prev_addr = NULL;
		} else if (!prevAddr) {
			address_list = block -> next_addr;
			nextAddr -> prev_addr = NULL;
		} else if (!nextAddr) {
			prevAddr -> next_addr = NULL;
			prevAddr = block -> prev_addr;
		} else {
			prevAddr = block -> prev_addr;
			nextAddr = block -> next_addr;
			prevAddr -> next_addr = nextAddr;
			nextAddr -> prev_addr = prevAddr;
		}
		block -> canary = ((uintptr_t)block ^ CANARY_MAGIC_NUMBER) + 1;
		*(unsigned long*)((uint8_t*)block + block -> size - sizeof(unsigned long)) = ((uintptr_t)block ^ CANARY_MAGIC_NUMBER) + 1;
		block -> next_addr = NULL;
		block -> prev_addr = NULL;
		block -> next_size = NULL;
		block -> prev_size = NULL;
		my_malloc_errno = NO_ERROR;
		return block + 1;
	}
	metadata_t *removed = (metadata_t*) ((uint8_t*) block + block -> size - totalSize);
	removed -> size = totalSize;
	removed -> prev_addr = NULL;
	removed -> next_addr = NULL;
	removed -> next_size = NULL;
	removed -> prev_size = NULL;
	removed -> canary = ((uintptr_t)removed ^ CANARY_MAGIC_NUMBER) + 1;
	*(unsigned long*)((uint8_t*)removed + removed -> size - sizeof(unsigned long)) = ((uintptr_t)removed ^ CANARY_MAGIC_NUMBER) + 1;
	unsigned long leftoverSize = block -> size - removed -> size;
	block -> size = leftoverSize;
	metadata_t *prevSize = block -> prev_size;
	metadata_t *nextSize = block -> next_size;
	if ((!prevSize && !nextSize) || !prevSize) {
		my_malloc_errno = NO_ERROR;
		return removed + 1;
	} else if (!nextSize) {
		prevSize -> next_size = NULL;
	} else {
		prevSize -> next_size = nextSize;
		nextSize -> prev_size = prevSize;
	}
	addToSize(block);
	my_malloc_errno = NO_ERROR;
	return removed + 1;
}

/* REALLOC
 * See my_malloc.h for documentation
 */
void *my_realloc(void *ptr, size_t size) {
	if (!ptr) {
		return my_malloc(size);
	}
	if (size == 0) {
		my_free(ptr);
		return NULL;
	}
	metadata_t *blockPtr = (metadata_t *) ((char*) ptr - sizeof(metadata_t)*sizeof(char));
	unsigned long realCanary = ((uintptr_t)blockPtr ^ CANARY_MAGIC_NUMBER) + 1;
	unsigned long* backCanary = (unsigned long*)((uint8_t*)blockPtr + blockPtr -> size - sizeof(unsigned long));
	if (realCanary != blockPtr -> canary || realCanary != *backCanary) {
		my_malloc_errno = CANARY_CORRUPTED;
		return NULL;
	}
	void *reallocPtr = my_malloc(size);
	memcpy(reallocPtr, ptr, size);
	my_free(ptr);
    return reallocPtr;
}

/* CALLOC
 * See my_malloc.h for documentation
 */
void *my_calloc(size_t nmemb, size_t size) {
	size_t totalSize = nmemb * size;
	void *callocPtr = my_malloc(totalSize);
	if (callocPtr) {
		memset(callocPtr, 0, totalSize);
	}
    return callocPtr;
}

/* FREE
 * See my_malloc.h for documentation
 */
void my_free(void *ptr) {
	if (ptr == NULL) {
		my_malloc_errno = NO_ERROR;
		return;
	}
	metadata_t *blockPtr = (metadata_t *) ((char*) ptr - sizeof(metadata_t)*sizeof(char));
	unsigned long realCanary = ((uintptr_t)blockPtr ^ CANARY_MAGIC_NUMBER) + 1;
	unsigned long* backCanary = (unsigned long*)((uint8_t*)blockPtr + blockPtr -> size - sizeof(unsigned long));
	if (realCanary != blockPtr -> canary || realCanary != *backCanary) {
		my_malloc_errno = CANARY_CORRUPTED;
		return;
	}
	if (address_list == NULL) {
		address_list = blockPtr;
		size_list = blockPtr;
		my_malloc_errno = NO_ERROR;
		return;
	}
	addToAddress(blockPtr);
	addToSize(blockPtr);
	my_malloc_errno = NO_ERROR;
}