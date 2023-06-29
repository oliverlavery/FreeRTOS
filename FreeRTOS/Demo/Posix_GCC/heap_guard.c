/*
 * Copyright 2018-2022 Amazon.com, Inc. or its affiliates. All rights reserved.
 *
 * AMAZON PROPRIETARY/CONFIDENTIAL
 *
 * You may not use this file except in compliance with the terms and
 * conditions set forth in the accompanying LICENSE.TXT file.
 *
 * THESE MATERIALS ARE PROVIDED ON AN "AS IS" BASIS. AMAZON SPECIFICALLY
 * DISCLAIMS, WITH RESPECT TO THESE MATERIALS, ALL WARRANTIES, EXPRESS,
 * IMPLIED, OR STATUTORY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

// #include <ace/ace.h>
// #include <ace/ace_log.h>
#include "FreeRTOS.h"
#include "heap_guard.h"
// #include <ace/ace_tools_handler.h>

#if portBYTE_ALIGNMENT < 4
#error "Cannot support <4B alignement"
#endif

#define HEAP_MAGIC 0x50414548 /* HEAP */
#define HEAP_GUARD_ENABLE_SIZE    \
    64 /* Min size of allocations \
           to enable check on */

// Since we're not actually using ACE...
#define ACE_STATUS_GENERAL_ERROR -1
#define ACE_STATUS_OK 0

typedef enum canary_state {
    /* 0: Uninitialized */
    CANARY_UNINITIALIZED = 0,
    /* 1: Initializing */
    CANARY_INITIALIZING  = 1,
    /* 2: Initialized */
    CANARY_INITIALIZED   = 2,
} canary_state_t;

static canary_state_t canaryState = CANARY_UNINITIALIZED;

static int heapGuardCanaryValue;


#define TAG "AceHeapGuard"



/*
 * This method will generate random number
 */

static uint32_t gen_random_num(void) {
    // This is not a CSRNG, it's the standard LPRNG, but it works for experimental purposes.
    // DO NOT use this in production code
    return rand();
}

static int32_t aceHeapGuard_getHead(void* ptr, struct malloc_header** head) {
    /*
     * Check the input pointer address to free - head
     * doesn't wrap to a negative/invalid pointer
     */
    if (!ptr || ((int)ptr <= (int)sizeof(struct malloc_header))) {
        *head = NULL;
        return ACE_STATUS_GENERAL_ERROR;
    }

    unsigned char* p = (unsigned char*)ptr;
    *head = (struct malloc_header*)(p - sizeof(struct malloc_header));
    return ACE_STATUS_OK;
}

void aceHeapGuard_preMallocHook(size_t size, size_t* req_size) {
    if (size >= HEAP_GUARD_ENABLE_SIZE) {
#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)~0)
#endif
        if (size <
                (SIZE_MAX - sizeof(struct malloc_header) - HEAP_GUARD_SIZE)) {
            *req_size = sizeof(struct malloc_header) + size + HEAP_GUARD_SIZE;
        } else {
            assert(0);
        }
    } else {
        *req_size = size;
    }
    return;
}

void aceHeapGuard_postMallocHook(void* ptr, size_t size, void** ret_ptr) {
    struct malloc_header* head;
    int* foot;

    /* self invoking */
    if (canaryState == CANARY_INITIALIZING) {
        return;
    }

    if (canaryState == CANARY_UNINITIALIZED) {
        canaryState = CANARY_INITIALIZING;
        uint32_t canary_rand = gen_random_num();
        heapGuardCanaryValue = ((int)ptr) ^ canary_rand;
        canaryState = CANARY_INITIALIZED;
    }

    /* Initialize the header */
    head = ptr;
    head->magic = HEAP_MAGIC;
    head->alloc_addr = (uint32_t)__builtin_return_address(0);
    head->footer = size;

    *ret_ptr = (unsigned char*)ptr + sizeof(struct malloc_header);
    foot = (int*)((unsigned char*)(*ret_ptr) + head->footer);

    /* Initialize the footer with the canary */
    for (int i = 0; i < HEAP_GUARD_NUM_CANARIES; i++) {
        *(foot + i) = heapGuardCanaryValue;
    }
    return;
}

void aceHeapGuard_freeHook(void* ptr, void** ret_ptr) {
    struct malloc_header* head;
    int* foot;

    if (ptr == NULL || aceHeapGuard_getHead(ptr, &head)) {
        return;
    }

    *ret_ptr = head;
    if (head->magic != HEAP_MAGIC) {
        *ret_ptr = ptr;
        return;
    }

    unsigned char* p = (unsigned char*)ptr;
    p += head->footer;
    foot = (int*)p;
    for (int i = 0; i < HEAP_GUARD_NUM_CANARIES; i++) {
        if (*(foot + i) != heapGuardCanaryValue) {
            // Call tool handler
            configASSERT("Heap corruption :(" && 0);
        }
    }
    return;
}

bool aceHeapGuard_isGuarded(void* ptr) {
    struct malloc_header* head;

    if (aceHeapGuard_getHead(ptr, &head)) {
        return false;
    }

    if (head->magic != HEAP_MAGIC) {
        return false;
    } else {
        return true;
    }
}

int32_t aceHeapGuard_getSize(void* ptr, size_t* size) {
    struct malloc_header* head;

    if (aceHeapGuard_getHead(ptr, &head)) {
        return ACE_STATUS_GENERAL_ERROR;
    }

    if (head->magic == HEAP_MAGIC) {
        *size = head->footer;
        return ACE_STATUS_OK;
    }

    return ACE_STATUS_GENERAL_ERROR;
}
