/**
 * @file endian.h
 * @brief Enhanced endianness conversion macros for STM32 (ARM Cortex-M).
 * @note Prevents conflicts with newlib's machine/endian.h and adds 64-bit support.
 */

#ifndef _ENDIAN_H_
#define _ENDIAN_H_

/* Only define if not already defined by the toolchain's sys/types.h or machine/endian.h */
#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN 1234
#endif

#ifndef BIG_ENDIAN
#define BIG_ENDIAN    4321
#endif

#ifndef BYTE_ORDER
#define BYTE_ORDER    LITTLE_ENDIAN
#endif

/* 16-bit Byte Swapping Macros using GCC Built-ins */
#ifndef htobe16
#define htobe16(x) __builtin_bswap16(x)
#define htole16(x) (x)
#define be16toh(x) __builtin_bswap16(x)
#define le16toh(x) (x)
#endif

/* 32-bit Byte Swapping Macros using GCC Built-ins */
#ifndef htobe32
#define htobe32(x) __builtin_bswap32(x)
#define htole32(x) (x)
#define be32toh(x) __builtin_bswap32(x)
#define le32toh(x) (x)
#endif

/* 64-bit Byte Swapping Macros using GCC Built-ins (Fixes csp_id.c warnings) */
#ifndef htobe64
#define htobe64(x) __builtin_bswap64(x)
#define htole64(x) (x)
#define be64toh(x) __builtin_bswap64(x)
#define le64toh(x) (x)
#endif

#endif /* _ENDIAN_H_ */
