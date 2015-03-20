.arm

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
