#ifndef _MEM_H_
#define _MEM_H_


#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <sys/mman.h>

#define HEAP_START ((void*)0x04040000)

struct block_search_result
{
	enum
	{
		BSR_FOUND_GOOD_BLOCK,
		BSR_REACHED_END_NOT_FOUND,
		BSR_CORRUPTED
	} type;
	struct block_header* block;
};

void* _malloc( size_t query );
void  _free( void* mem );
void* heap_init( size_t initial_size );
static bool block_is_big_enough(size_t query, struct block_header* block);
static size_t pages_count(size_t mem);
static size_t round_pages(size_t mem);
static void block_init(void* restrict addr, block_size block_sz, void* restrict next);
static size_t region_actual_size(size_t query);
static void* map_pages(void const* addr, size_t length, int additional_flags);
static struct region alloc_region(void const* addr, size_t query);
static void* block_after(struct block_header const* block);
static bool block_splittable(struct block_header* restrict block, size_t query);
static bool split_if_too_big(struct block_header* block, size_t query);
static bool blocks_continuous( struct block_header const* fst, struct block_header const* snd);
static bool mergeable(struct block_header const* restrict fst, struct block_header const* restrict snd);
static bool try_merge_with_next(struct block_header* block);
static struct block_search_result find_good_or_last(struct block_header* restrict block, size_t sz);
static struct block_search_result try_memalloc_existing(size_t query, struct block_header* block);
static struct block_header* grow_heap(struct block_header* restrict last, size_t query);
static struct block_header* memalloc(size_t query, struct block_header* heap_start);
static struct block_header* block_get_header(void* contents);


void* heap_init(size_t initial);

#define DEBUG_FIRST_BYTES 4

void debug_struct_info( FILE* f, void const* address );
void debug_heap( FILE* f,  void const* ptr );

#endif
