#pragma once

#define synci() __asm__ volatile( \
	"mcr p15, 0, r0, c7, c10, 5\n" \
	"mcr p15, 0, r0, c7, c5, 4\n" \
	::: "r0")

int svcBackdoor(int (*func)(void));

