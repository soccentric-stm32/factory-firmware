/**
 * @file    flatcc_port.c
 * @brief   Fixed-arena allocator for the flatcc runtime.
 * @details A small first-fit allocator with 16-byte blocks.  It is used only
 *          by the Comms task (builder/verifier), so no lock is needed.  The
 *          arena is part of .bss; malloc/free are never linked.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DRT-004 (no allocation after start-up), DCON-002
 */

#include <string.h>
#include "flatcc_port.h"

#ifndef APP_FLATCC_HEAP_SIZE
#define APP_FLATCC_HEAP_SIZE (16U * 1024U)
#endif

#define FLATCC_ALIGNMENT 16U

/** @brief Header in front of every payload in the arena. */
typedef struct flatcc_block {
    size_t size;                 /**< Payload size, 16-byte aligned. */
    struct flatcc_block *next;   /**< Next block, NULL at the arena end. */
    uint32_t free;               /**< 1 while the block is free. */
    uint32_t pad;                /**< Keeps the payload 16-byte aligned. */
} __attribute__((aligned(FLATCC_ALIGNMENT))) flatcc_block_t;

/** Backing store for the arena, aligned like the blocks. */
typedef union {
    long double align;
    void *pointer;
    unsigned char bytes[APP_FLATCC_HEAP_SIZE];
} __attribute__((aligned(FLATCC_ALIGNMENT))) flatcc_arena_t;

static flatcc_arena_t flatcc_arena;      /**< Static arena, .bss. */
static flatcc_block_t *flatcc_head;      /**< First block, NULL until used. */

/**
 * @brief Round a size up to the 16-byte block alignment.
 */
static size_t flatcc_align(size_t n)
{
    return (n + (FLATCC_ALIGNMENT - 1U)) & ~(size_t)(FLATCC_ALIGNMENT - 1U);
}

/** @brief Create the single free block that spans the arena, once. */
static void flatcc_heap_init(void)
{
    if (flatcc_head == NULL) {
        flatcc_head = (flatcc_block_t *)flatcc_arena.bytes;
        flatcc_head->size = APP_FLATCC_HEAP_SIZE - sizeof(flatcc_block_t);
        flatcc_head->next = NULL;
        flatcc_head->free = 1U;
    }
}

void *app_flatcc_alloc(size_t size)
{
    flatcc_heap_init();
    if (size == 0U) {
        return NULL;
    }
    size = flatcc_align(size);

    for (flatcc_block_t *block = flatcc_head; block != NULL;
         block = block->next) {
        if (block->free == 0U || block->size < size) {
            continue;
        }
        /* Split when the tail can hold a header and 16 payload bytes. */
        if ((block->size - size) >= (sizeof(flatcc_block_t) +
                                     FLATCC_ALIGNMENT)) {
            flatcc_block_t *tail = (flatcc_block_t *)((unsigned char *)block +
                                                     sizeof(flatcc_block_t) +
                                                     size);
            tail->size = block->size - size - sizeof(flatcc_block_t);
            tail->next = block->next;
            tail->free = 1U;
            block->next = tail;
            block->size = size;
        }
        block->free = 0U;
        return (unsigned char *)block + sizeof(flatcc_block_t);
    }
    return NULL;
}

void app_flatcc_free(void *ptr)
{
    if (ptr == NULL) {
        return;
    }
    flatcc_block_t *block = (flatcc_block_t *)((unsigned char *)ptr -
                                               sizeof(flatcc_block_t));
    block->free = 1U;

    /* Coalesce with a free predecessor, then with free followers. */
    flatcc_block_t *prev = flatcc_head;
    while (prev != NULL && prev->next != block) {
        prev = prev->next;
    }
    if (prev != NULL && prev->free != 0U) {
        prev->size += sizeof(flatcc_block_t) + block->size;
        prev->next = block->next;
        block = prev;
    }
    while (block->next != NULL && block->next->free != 0U) {
        flatcc_block_t *next = block->next;
        block->size += sizeof(flatcc_block_t) + next->size;
        block->next = next->next;
    }
}

void *app_flatcc_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) {
        return app_flatcc_alloc(size);
    }
    if (size == 0U) {
        app_flatcc_free(ptr);
        return NULL;
    }

    flatcc_block_t *block = (flatcc_block_t *)((unsigned char *)ptr -
                                               sizeof(flatcc_block_t));
    if (block->size >= flatcc_align(size)) {
        return ptr;
    }

    void *new_ptr = app_flatcc_alloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }
    memcpy(new_ptr, ptr, block->size);
    app_flatcc_free(ptr);
    return new_ptr;
}

void *app_flatcc_calloc(size_t count, size_t size)
{
    if (count != 0U && size > (SIZE_MAX / count)) {
        return NULL;
    }
    size_t total = count * size;
    void *ptr = app_flatcc_alloc(total);
    if (ptr != NULL) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *app_flatcc_aligned_alloc(size_t alignment, size_t size)
{
    (void)alignment;   /* the arena always returns 16-byte aligned blocks */
    return app_flatcc_alloc(size);
}

void app_flatcc_aligned_free(void *ptr)
{
    app_flatcc_free(ptr);
}
