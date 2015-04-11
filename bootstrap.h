#pragma once

#include <stdbool.h>

extern unsigned int *arm11_buffer;
extern unsigned int *arm9_payload;
extern unsigned int arm9_payload_size;

int do_gshax_copy(void *dst, void *src, unsigned int len, unsigned int check_val, int check_off);
bool doARM11Hax();

