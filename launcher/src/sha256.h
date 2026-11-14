/* Launcher5 - empreinte SHA-256 (verification des mises a jour) - (c) 2026 cedsaill */
#ifndef L5_SHA256_H
#define L5_SHA256_H
#include <stddef.h>

/* empreinte de data en hexadecimal minuscule (65 octets avec le zero final) */
void sha256_hex(const unsigned char *data, size_t len, char out[65]);

#endif
