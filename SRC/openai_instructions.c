/* M257 build entry: wrapper on first include, preserved M232 base on recursion. */
#ifndef OPENAI_M257_INSTR_ENTRY
#define OPENAI_M257_INSTR_ENTRY
#include "openai_instructions_m257.c"
#else
#include "openai_instructions_base.c"
#endif
