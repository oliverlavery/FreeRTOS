/*
 * Copyright 2018-2020 Amazon.com, Inc. or its affiliates. All rights reserved.
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
#include <string.h>

#include <FreeRTOS.h>

#include "heap_guard.h"

// extern void* __wrap_pvPortMalloc(size_t size);
 extern void* __real_pvPortMalloc(size_t size);
// extern void* pvPortCalloc(size_t nmemb, size_t size);
// extern void* __wrap_pvPortRealloc(void* pv, size_t size);
// extern void* __real_pvPortRealloc(void* pv, size_t size);
// extern void* pvPortRealloc(void* pv, size_t size);
// extern void __wrap_vPortFree(void* pv);
 extern void __real_vPortFree(void* pv);

// void* __wrap_malloc(size_t size) { return __wrap_pvPortMalloc(size); }

// void* __wrap__malloc_r(void* ptr, size_t size) { return __wrap_pvPortMalloc(size); }

// void* __wrap_calloc(size_t nmemb, size_t size) {
//     return pvPortCalloc(nmemb, size);
// }

// void __wrap_free(void* pv) { __wrap_vPortFree(pv); }

// void __wrap__free_r(void* ptr, void* pv) { __wrap_vPortFree(pv); }

#ifdef TRACK_WATERMARK
#define HEAP_FREE_WATERMARK_UNINITIALIZED ((size_t)-1)
#define ACE_HEAPGUARD 1
/* Keep track of system heap free low water mark */
static size_t xHeapFreeLowWaterMark = HEAP_FREE_WATERMARK_UNINITIALIZED;

/* Get system heap free low watermark */
size_t xGetHeapFreeLowWaterMark(bool reset) {
    size_t previous = xHeapFreeLowWaterMark;
    if (reset) {
        xHeapFreeLowWaterMark = xPortGetFreeHeapSize();
    }
    return previous;
}
#else

size_t xGetHeapFreeLowWaterMark(bool reset) { return 0; }
#endif

void* pvPortMalloc(size_t size) {
    size_t req_size = size;
    void* ret_ptr = NULL;
    aceHeapGuard_preMallocHook(size, &req_size);
    void* ptr = __real_pvPortMalloc(req_size);
    ret_ptr = ptr;
    if (ptr) {
#ifdef ACE_HEAPGUARD
        if (size != req_size) {
            aceHeapGuard_postMallocHook(ptr, size, &ret_ptr);
        }
#endif
    }
    return ret_ptr;
}

void vPortFree(void* pv) {
    if (pv == NULL)
        return;

    void* ret_ptr = pv;
#ifdef ACE_HEAPGUARD
    aceHeapGuard_freeHook(pv, &ret_ptr);
#endif
}

