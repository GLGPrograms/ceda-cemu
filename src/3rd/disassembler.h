#ifndef CEDA_DISASSEMBLER_H
#define CEDA_DISASSEMBLER_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Disassemble a binary blob.
 *
 * The disassembly produced represents the decoding of the first opcode in the
 * blob. Output is a null-terminated C-string.
 *
 * @param blob Pointer to raw binary data to be disassembled.
 * @param pc Program counter where that data is located at.
 * @param buf Pointer to the destination buffer for the disassembled mnemonic.
 * @param buflen Size of the destination buffer.
 *
 * @return int Number of bytes disassembled from the blob.
 */
int disassemble(uint8_t *blob, int pc, char *buf, size_t buflen);

#endif
