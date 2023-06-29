/*
 * Copyright 2018-2021 Amazon.com, Inc. or its affiliates. All rights reserved.
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

#ifndef ACE_HEAP_GUARD_H
#define ACE_HEAP_GUARD_H

#include <stdint.h>
#include <stdbool.h>
// #include <ace/ace.h>

#define HEAP_GUARD_SIZE (4)
#define HEAP_GUARD_NUM_CANARIES (HEAP_GUARD_SIZE / (int)sizeof(int))

struct malloc_header {
    uint32_t magic;
    // XXX portability fix, uint32_t for below is wrong on 64 bit architectures
    void *alloc_addr;
    size_t footer;
    /*
     * Since data follows the header in actual Heap allocation, keep the size
     * of struct as a mutiple of 4 bytes.
     */
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup ACE_HEAP_GUARD_API
 * @{
 */

/**
 * @brief Callback function type
 * @param[in] ctx Context included during the callback
 *
 * @return ACE_STATUS_OK on success or ACE_STATUS_GENERAL_ERROR on failure
 */
typedef int32_t (*aceHeapGuard_cb)(void* ctx);

/**
 * @brief Register a callback for heap guard. If registered with a non NULL
 * value, the function will be called when a heap over-run is detected.
 * @param[in] cb Callback to be called
 * @param[in] ctx Context to be included during the callback
 *
 * @return ACE_STATUS_OK on success or ACE_STATUS_GENERAL_ERROR on failure
 */
int32_t aceHeapGuard_register_cb(aceHeapGuard_cb cb, void* ctx);

/**
 * @brief Callback function type
 * @param[in] size Requested allocation size
 * @param[out] req_size Pointer to location with updated size with actual
 * allocation required including heap guard overheads
 *
 */
void aceHeapGuard_preMallocHook(size_t size, size_t* req_size);

/**
 * @brief Callback function type
 * @param[in] ptr Pointer to actual allocation on heap
 * @param[in] size Requested allocation size
 * @param[out] ret_ptr Pointer to location with updated pointer to first byte
 * after heap guard header
 *
 */
void aceHeapGuard_postMallocHook(void* ptr, size_t size, void** ret_ptr);

/**
 * @brief Callback function type
 * @param[in] ptr Pointer to first byte after heap guard header
 * @param[out] ret_ptr Pointer to location with updated pointer to actual
 * allocation on heap
 *
 */
void aceHeapGuard_freeHook(void* ptr, void** ret_ptr);

/**
 * @brief Callback function type
 * @param[in] ptr Pointer to data buffer
 *
 * @return true if ACE Heap Guard present, else false
 */
bool aceHeapGuard_isGuarded(void* ptr);

/**
 * @brief Callback function type
 * @param[in] ptr Pointer to data buffer
 * @param[out] size Requested allocation size
 *
 * @return ACE_STATUS_OK on success or ACE_STATUS_GENERAL_ERROR on failure
 */
int32_t aceHeapGuard_getSize(void* ptr, size_t* size);

/** @} */

#ifdef __cplusplus
}
#endif

#endif  //  ACE_HEAP_GUARD_H
