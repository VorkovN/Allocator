#include <stdarg.h>

#define _DEFAULT_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

#include "../inc/mem_internals.h"
#include "../inc/mem.h"
#include "../inc/util.h"

void debug_block(struct block_header* b, const char* fmt, ...);

void debug(const char* fmt, ...);

extern inline block_size size_from_capacity(block_capacity cap);

extern inline block_capacity capacity_from_size(block_size sz);

//проверяет вместится ли блок
static bool block_is_big_enough(size_t query, struct block_header* block)
{
	return block->capacity.bytes >= query;
}

//пагинатор//
//смотрим на сколько частей разбить
static size_t pages_count(size_t mem)
{
	return mem / getpagesize() + ((mem % getpagesize()) > 0);
}

//округляем итоговый размер (>= начальный размер)
static size_t round_pages(size_t mem)
{
	return getpagesize() * pages_count(mem);
}

//инициализация свободного блока(наверно пригодится для расширения)
static void block_init(void* restrict addr, block_size block_sz, void* restrict next)
{
	*((struct block_header*)addr) = (struct block_header){
			.next = next,
			.capacity = capacity_from_size(block_sz),
			.is_free = true
	};
}

//подбор нужного размера для региона
static size_t region_actual_size(size_t query)
{
	return size_max(round_pages(query), REGION_MIN_SIZE);
}

//если мы выделяем память для NULL то мы лопухи
extern inline bool region_is_invalid(const struct region* r);

//https://wm-help.net/lib/b/book/1696396857/99
//возвращает указатель на выделенную память
static void* map_pages(void const* addr, size_t length, int additional_flags)
{
	return mmap((void*)addr, length, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | additional_flags, 0, 0);
}

/*  аллоцировать регион памяти и инициализировать его блоком */
static struct region alloc_region(void const* addr, size_t query)
{
	//todo
	query = region_actual_size(query);
	block_init(addr, (block_size){ query }, NULL);
	return (struct region){ .size = query, .addr = addr, .extends = true };
}

//получить указатель на следующий блок
static void* block_after(struct block_header const* block)
{
	return (void*)(block->contents + block->capacity.bytes);
}

//получить указатель на этот регион
void* heap_init(size_t initial)
{
	const struct region region = alloc_region(HEAP_START, initial);
	if (region_is_invalid(&region)) return NULL;

	return region.addr;
}

#define BLOCK_MIN_CAPACITY 24

/*  --- Разделение блоков (если найденный свободный блок слишком большой )--- */

//проверка на возможность разделения блока
static bool block_splittable(struct block_header* restrict block, size_t query)
{
	return block->is_free && query + offsetof(struct block_header, contents) + BLOCK_MIN_CAPACITY <= block->capacity.bytes;
}

//разбиение большого блока на два
static bool split_if_too_big(struct block_header* block, size_t query)
{
	//todo вроде норм
	if (block_splittable(block, query))
	{
		block_init((struct block_header*)(block->contents + query), (block_size){ block->capacity.bytes - query }, block->next);
		block->next = (struct block_header*)(block->contents + query);
		block->capacity.bytes = query;
		return true;
	}
	return false;
}


/*  --- Слияние соседних свободных блоков --- */

//проверка: идут ли блоки последовательно
static bool blocks_continuous( struct block_header const* fst, struct block_header const* snd)
{
	return (void*)snd == block_after(fst);
}

//проверка на возможность слияния блоков
static bool mergeable(struct block_header const* restrict fst, struct block_header const* restrict snd)
{
	return fst->is_free && snd->is_free && blocks_continuous(fst, snd);
}

//слияние блоков
static bool try_merge_with_next(struct block_header* block)
{
	//todo
	if (block->next == NULL)
		return false;

	struct block_header* block_next = block->next;


	if (mergeable(block, block_next))
	{
		block->capacity.bytes += size_from_capacity(block_next->capacity).bytes;
		block->next = block_next->next;
		return true;
	}
	return false;

}


/*  --- ... ecли размера кучи хватает --- */


static struct block_search_result find_good_or_last(struct block_header* restrict block, size_t sz)
{
	//todo
	sz = sz > BLOCK_MIN_CAPACITY ? sz : BLOCK_MIN_CAPACITY;

	struct block_search_result result;
	result.block = block;

	while (try_merge_with_next(block));

	if (block_is_big_enough(sz, block))
		result.type = BSR_FOUND_GOOD_BLOCK;
	else
		result.type = BSR_REACHED_END_NOT_FOUND;

	return result;
}

/*  Попробовать выделить память в куче начиная с блока `block` не пытаясь расширить кучу
 Можно переиспользовать как только кучу расширили. */
static struct block_search_result try_memalloc_existing(size_t query, struct block_header* block)
{
	if (!block->is_free)
		return (struct block_search_result){ .block = block, .type = BSR_CORRUPTED };

	if (find_good_or_last(block, query).type == BSR_FOUND_GOOD_BLOCK)
	{
		split_if_too_big(block, query);
		return (struct block_search_result){ .block = block, .type = BSR_FOUND_GOOD_BLOCK };
	}
	return (struct block_search_result){ .block = block, .type = BSR_REACHED_END_NOT_FOUND };
}


static struct block_header* grow_heap(struct block_header* restrict last, size_t query)
{
	//todo
	last->next = heap_init(query);
	return last->next;
}

/*  Реализует основную логику malloc и возвращает заголовок выделенного блока */
static struct block_header* memalloc(size_t query, struct block_header* heap_start)
{
	//todo
	query = query > BLOCK_MIN_CAPACITY ? query : BLOCK_MIN_CAPACITY;

	struct block_search_result bsr = try_memalloc_existing(query, heap_start);

	if (bsr.type == BSR_FOUND_GOOD_BLOCK)
	{
		bsr.block->is_free = false;
		return bsr.block;
	}

	bsr.block = grow_heap(bsr.block, query);
	bsr = try_memalloc_existing(query, bsr.block);
	bsr.block->is_free = false;

	return bsr.block;
}

void* _malloc(size_t query)
{
	struct block_header* const addr = memalloc(query, (struct block_header*)HEAP_START);
	if (addr) return addr->contents;
	else return NULL;
}

static struct block_header* block_get_header(void* contents)
{
	return (struct block_header*)(((uint8_t*)contents) - offsetof(struct block_header, contents));
}

void _free(void* mem)
{
	if (!mem) return;
	struct block_header* header = block_get_header(mem);
	header->is_free = true;
	while (try_merge_with_next(header));
}
