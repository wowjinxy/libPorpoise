#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#include <dolphin.h>
#include <demo.h>
#include <demo/DEMOWin.h>
#include <demo/DEMOAVX.h>



#define AVX_INTERNAL_NUM_FRAMES  10

static s16 __AVX_internal_buffer[AVX_FRAME_SIZE_WORDS*AVX_INTERNAL_NUM_FRAMES] ATTRIBUTE_ALIGN(32);

static void (*__AVX_save_isr)(void);

static u32  __AVX_num_frames;
static u32  __AVX_num_filled;
static u32  __AVX_curr_frame;

static u16 *__AVX_buffer;
static s16 *__AVX_left_buffer;
static s16 *__AVX_right_buffer;

static u32  __AVX_write_ptr    = 0;
static u32  __AVX_buffer_size  = 0;

static BOOL flag = FALSE;


static void __DEMOAVX_isr()
{

    u32 frame_address;

        if (__AVX_save_isr)
        {
            (*__AVX_save_isr)();
            frame_address = 0x80000000 | AIGetDMAStartAddr();
            ASSERTMSG(frame_address, "AVX: frame address is NULL!\n");

            DCInvalidateRange((void *)(frame_address), AVX_FRAME_SIZE_BYTES);

            memcpy((void *)(&__AVX_buffer[__AVX_curr_frame * AVX_FRAME_SIZE_WORDS]), (void *)(frame_address), AVX_FRAME_SIZE_BYTES);

            DCFlushRange((void *)(&__AVX_buffer[__AVX_curr_frame * AVX_FRAME_SIZE_WORDS]), AVX_FRAME_SIZE_BYTES);


            __AVX_curr_frame = (__AVX_curr_frame + 1) % __AVX_num_frames;

            __AVX_num_filled = (__AVX_num_filled + 1) % AVX_INTERNAL_NUM_FRAMES;

            if (__AVX_curr_frame > 4)
            {
                flag = TRUE;
            }
        }
}

u32 DEMOAVXGetNumFilled(void)
{

    u32  tmp;
    BOOL old;

        old = OSDisableInterrupts();

        tmp = __AVX_num_filled;
        __AVX_num_filled = 0;

        OSRestoreInterrupts(old);

        return(tmp);

}

u32 DEMOAVXGetFrameCounter(void)
{

    return(__AVX_curr_frame);

}

u32 DEMOAVXRefreshBuffer(u32 *start_index, u32 *end_index)
{

    u32  num_filled;
    u32  curr_frame;

    u32 i;
    u32 j;

        if (flag)
        {
            num_filled = DEMOAVXGetNumFilled();

            curr_frame = (__AVX_num_frames + DEMOAVXGetFrameCounter() - num_filled) % __AVX_num_frames;

            *start_index = __AVX_write_ptr;

            for (i=0; i<num_filled; i++)
            {
                DCInvalidateRange( (void *)(&__AVX_buffer[curr_frame * AVX_FRAME_SIZE_WORDS]), AVX_FRAME_SIZE_BYTES);

                for (j=0; j<AVX_FRAME_SIZE_WORDS; j+=2)
                {
                    __AVX_left_buffer [__AVX_write_ptr] = (s16)(__AVX_buffer[curr_frame * AVX_FRAME_SIZE_WORDS + j]);
                    __AVX_right_buffer[__AVX_write_ptr] = (s16)(__AVX_buffer[curr_frame * AVX_FRAME_SIZE_WORDS + j + 1]);

                    __AVX_write_ptr = (__AVX_write_ptr + 1) % __AVX_buffer_size;
                }

                curr_frame = (curr_frame + 1) % __AVX_num_frames;

            }

            *end_index = __AVX_write_ptr;



            return(num_filled*AVX_FRAME_SIZE_SAMPLES);
        }

        else
        {
            return(0);
        }
}

void DEMOAVXInit(s16 *left, s16 *right, u32 size)
{
    __AVX_left_buffer  = left;
    __AVX_right_buffer = right;

    __AVX_write_ptr = 0;

    __AVX_buffer_size = size;
    DEMOAVXAttach((void *)__AVX_internal_buffer, AVX_INTERNAL_NUM_FRAMES);

}

void DEMOAVXAttach(void *buffer, u32 num_frames)
{

    BOOL old;
    u32  i;

    __AVX_buffer     = (u16 *)buffer;
    __AVX_num_frames = num_frames;
    __AVX_num_filled = 0;
    __AVX_curr_frame = 0;
    
    for (i=0; i<(num_frames*AVX_FRAME_SIZE_WORDS); i++)
    {
        *(__AVX_buffer + i) = 0;
    }
    DCFlushRange(__AVX_buffer, num_frames*AVX_FRAME_SIZE_WORDS);

    old = OSDisableInterrupts();

    __AVX_save_isr = AIRegisterDMACallback(__DEMOAVX_isr);

    OSRestoreInterrupts(old);
}