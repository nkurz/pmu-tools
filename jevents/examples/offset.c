// Demonstration of ctx->offset jumping from 0 to 2^48 during execution
// cc -g -Wall -Wextra -Wconversion -std=c99 offset.c ../libjevents.a -o offset

//#define ASM_RDPMC  // compare with and without this line commented

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include "../rdpmc.h"

#define SAVED_MASK 0xffffffffffff
#define HW_INTERRUPTS 0x1cb
#define MAX_TO_SAVE 100L
#define DEFAULT_REPEAT 100000
#define LOOP_CYCLES 100000

#define COMPILER_NOINLINE __attribute__ ((noinline))
void COMPILER_NOINLINE timing_loop(uint64_t counter) {
    if (! counter) return;
    __asm volatile ("1:\n"                               
                    "dec %0\n"                           
                    "jnz 1b" :                           
                    /* read/write */ "+r" (counter) :    
                    /* no read only */ :                 
                    /* pretend to clobber */ "memory");
    return;
}

inline uint64_t asm_rdpmc(int ecx) {
    uint32_t eax, edx; 
    __asm volatile ("rdpmc\n" :
                    /* write */ "=a" (eax),
                    /* write */ "=d" (edx) :       
                    /* read */ "c" (ecx));
    return (uint64_t) edx << 32 | eax;
}


int main(int argc, char **argv) {
    uint64_t repeat = DEFAULT_REPEAT;
    if (argc > 1) repeat = strtoul(argv[1], NULL, 0);

    struct rdpmc_ctx ctx;
    if (rdpmc_open(HW_INTERRUPTS, &ctx) < 0) {
        exit(1);
    }
    ioctl(ctx.fd, PERF_EVENT_IOC_RESET, 0);
    
    uint64_t previous = rdpmc_read(&ctx);
    uint64_t saved[MAX_TO_SAVE];
    uint64_t total = 0;

#ifdef ASM_RDPMC
    int pmc = ctx.buf->index - 1;
#endif
    for (uint64_t i = 0; i < repeat; i++) {
        timing_loop(LOOP_CYCLES);
#ifdef ASM_RDPMC
        uint64_t current = asm_rdpmc(pmc);
#else
        uint64_t current = rdpmc_read(&ctx);
#endif
        if (current != previous) {
            if (total < MAX_TO_SAVE) saved[total] = current;
            previous = current;
            total++;
        }
    }
    
    uint64_t num_saved = total;
    if (num_saved > MAX_TO_SAVE) num_saved = MAX_TO_SAVE;

    printf("Repeat iterations: %ld\n", repeat);
    printf("Total interrupts: %ld\n", total);
    printf("saved[0..%ld] (as-is):\n", num_saved - 1);
    for (uint64_t i = 0; i < num_saved; i++) {
        printf("%ld ", saved[i]);
    }
    printf("\n");
    printf("saved[0..%ld] & %#lx:\n", num_saved - 1, SAVED_MASK);
    for (uint64_t i = 0; i < num_saved; i++) {
        printf("%ld ", saved[i] & SAVED_MASK);
    }
    printf("\n");
    
    rdpmc_close(&ctx);
    return 0;
}
