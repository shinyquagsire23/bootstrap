.arm

.global InvalidateEntireInstructionCache
.type InvalidateEntireInstructionCache, %function
InvalidateEntireInstructionCache:
	mov     R0, #0
	mcr     p15, 0, r0,c7,c5, 0
	mcr     p15, 0, r0,c7,c5, 4
	mcr     p15, 0, r0,c7,c5, 6
	mcr     p15, 0, r0,c7,c10, 4
	bx lr

.global InvalidateEntireDataCache
.type InvalidateEntireDataCache, %function
InvalidateEntireDataCache:
	mov r0, #0
	mcr     p15, 0, r0,c7,c14, 0
	mcr     p15, 0, r0,c7,c10, 4
	bx lr

.global svcBackdoor
.type svcBackdoor, %function
svcBackdoor:
   svc 0x7B
   bx lr

.global memcpy_asm
.type memcpy_asm, %function
memcpy_asm:
	ADD     R2, R1, R2
memcpy_loop:
	LDMIA   R1!, {R3}
	STMIA   R0!, {R3}
	CMP     R1, R2
	BCC     memcpy_loop
	BX      LR
