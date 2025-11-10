/*---------------------------------------------------------------------------*
  ARQ.c - ARAM Queue (Queued DMA Operations)
  
  Manages queued DMA transfers between ARAM and main RAM.
  
  On GC/Wii: Queues large transfers, breaks into chunks
  On PC: Executes immediately (simulated DMA is instant)
 *---------------------------------------------------------------------------*/

#include <dolphin/ar.h>
#include <dolphin/os.h>
#include <string.h>

/*---------------------------------------------------------------------------*
    Internal State
 *---------------------------------------------------------------------------*/

static BOOL s_arqInitialized = FALSE;
static u32 s_chunkSize = 4096;  // Default chunk size

// Priority queues (high and low)
static ARQRequest* s_queueHi = NULL;
static ARQRequest* s_queueLo = NULL;

/*---------------------------------------------------------------------------*
  Name:         ARQInit

  Description:  Initialize ARAM Queue subsystem.

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void ARQInit(void) {
    if (s_arqInitialized) {
        return;
    }
    
    s_queueHi = NULL;
    s_queueLo = NULL;
    s_chunkSize = 4096;
    s_arqInitialized = TRUE;
    
    OSReport("ARQ: Queue system initialized\n");
}

/*---------------------------------------------------------------------------*
  Name:         ARQReset

  Description:  Reset ARAM Queue subsystem.

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void ARQReset(void) {
    s_queueHi = NULL;
    s_queueLo = NULL;
    s_arqInitialized = FALSE;
}

/*---------------------------------------------------------------------------*
  Name:         ARQCheckInit

  Description:  Check if ARQ is initialized.

  Arguments:    None

  Returns:      TRUE if ARQInit() has been called
 *---------------------------------------------------------------------------*/
BOOL ARQCheckInit(void) {
    return s_arqInitialized;
}

/*---------------------------------------------------------------------------*
  Name:         ARQPostRequest

  Description:  Post a DMA request to the queue. On PC, executes immediately
                since simulated DMA is instant.
                
                On GC/Wii: Adds to queue, processes in chunks
                On PC: Executes DMA immediately, calls callback

  Arguments:    request   ARQ request structure
                owner     Owner ID
                type      DMA type (AR_MRAM_TO_ARAM or AR_ARAM_TO_MRAM)
                priority  Priority (0=high, 1=low)
                source    Source address
                dest      Destination address
                length    Transfer length
                callback  Completion callback

  Returns:      None
 *---------------------------------------------------------------------------*/
void ARQPostRequest(ARQRequest* request, u32 owner, u32 type, u32 priority,
                    u32 source, u32 dest, u32 length,
                    void (*callback)(ARQRequest*)) {
    if (!s_arqInitialized || !request) {
        return;
    }
    
    // Fill in request
    request->owner = owner;
    request->type = type;
    request->priority = priority;
    request->source = source;
    request->dest = dest;
    request->length = length;
    request->callback = callback;
    request->next = NULL;
    
    // On PC, execute immediately (no real queue needed)
    // DMA is instant memcpy
    ARStartDMA(type, source, dest, length);
    
    // Call callback
    if (callback) {
        callback(request);
    }
}

/*---------------------------------------------------------------------------*
  Name:         ARQRemoveRequest

  Description:  Remove a request from the queue.
                
                On PC: No-op since requests execute immediately

  Arguments:    request  Request to remove

  Returns:      None
 *---------------------------------------------------------------------------*/
void ARQRemoveRequest(ARQRequest* request) {
    (void)request;
    
    /* On PC, requests are not queued - they execute immediately.
     * This function exists for API compatibility.
     */
}

/*---------------------------------------------------------------------------*
  Name:         ARQRemoveOwnerRequest

  Description:  Remove all requests from a specific owner.
                
                On PC: No-op since requests execute immediately

  Arguments:    owner  Owner ID

  Returns:      None
 *---------------------------------------------------------------------------*/
void ARQRemoveOwnerRequest(u32 owner) {
    (void)owner;
    
    /* On PC, requests are not queued - they execute immediately. */
}

/*---------------------------------------------------------------------------*
  Name:         ARQFlushQueue

  Description:  Flush all pending requests.
                
                On PC: No-op since requests execute immediately

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void ARQFlushQueue(void) {
    /* On PC, no pending requests - all execute immediately. */
}

/*---------------------------------------------------------------------------*
  Name:         ARQSetChunkSize

  Description:  Set chunk size for breaking up large DMA transfers.
                
                On GC/Wii: Large transfers split into chunks to avoid blocking
                On PC: Stored but not used (transfers are instant)

  Arguments:    size  Chunk size in bytes

  Returns:      None
 *---------------------------------------------------------------------------*/
void ARQSetChunkSize(u32 size) {
    s_chunkSize = size;
}

/*---------------------------------------------------------------------------*
  Name:         ARQGetChunkSize

  Description:  Get current chunk size.

  Arguments:    None

  Returns:      Chunk size in bytes
 *---------------------------------------------------------------------------*/
u32 ARQGetChunkSize(void) {
    return s_chunkSize;
}

