#ifndef CEDA_DISASSEMBLER_H
#define CEDA_DISASSEMBLER_H

#include <stddef.h>
#include <stdint.h>

int disassemble(uint8_t *blob, int pc, char *buf, size_t buflen);

#endif
