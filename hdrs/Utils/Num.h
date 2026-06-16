#ifndef UTILS_NUM_H
#define UTILS_NUM_H

#ifdef __cplusplus
extern "C"{
#endif

#include <inttypes.h>
#include <limits.h>
#include <stdint.h>

typedef  int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef  uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef  intptr_t isize;
typedef uintptr_t usize;

typedef float  f32;
typedef double f64;

#define  I8_MIN ( (i8) INT8_MIN)
#define I16_MIN ((i16)INT16_MIN)
#define I32_MIN ((i32)INT32_MIN)
#define I64_MIN ((i64)INT64_MIN)
#define  I8_MAX ( (i8) INT8_MAX)
#define I16_MAX ((i16)INT16_MAX)
#define I32_MAX ((i32)INT32_MAX)
#define I64_MAX ((i64)INT64_MAX)

#define  U8_MAX ( (u8) UINT8_MAX)
#define U16_MAX ((u16)UINT16_MAX)
#define U32_MAX ((u32)UINT32_MAX)
#define U64_MAX ((u64)UINT64_MAX)

#define ISIZE_MIN ((isize)INTPTR_MIN)
#define ISIZE_MAX ((isize)INTPTR_MAX)

#define USIZE_MAX ((usize)UINTPTR_MAX)

#define  U8_LSBIT ( (u8)1ull)
#define U16_LSBIT ((u16)1ull)
#define U32_LSBIT ((u32)1ull)
#define U64_LSBIT ((u64)1ull)
#define  U8_MSBIT ( (u8)(1ull << (sizeof( u8) * CHAR_BIT - 1)))
#define U16_MSBIT ((u16)(1ull << (sizeof(u16) * CHAR_BIT - 1)))
#define U32_MSBIT ((u32)(1ull << (sizeof(u32) * CHAR_BIT - 1)))
#define U64_MSBIT ((u64)(1ull << (sizeof(u64) * CHAR_BIT - 1)))

#define USIZE_LSBIT ((usize)1ull)
#define USIZE_MSBIT ((usize)(1ull << (sizeof(usize) * CHAR_BIT - 1)))

#define  I8_PFMT "%" PRId8
#define I16_PFMT "%" PRId16
#define I32_PFMT "%" PRId32
#define I64_PFMT "%" PRId64

#define  U8_PFMT  "%" PRIu8
#define U16_PFMT  "%" PRIu16
#define U32_PFMT  "%" PRIu32
#define U64_PFMT  "%" PRIu64
#define  U8_PoFMT "%" PRIo8
#define U16_PoFMT "%" PRIo16
#define U32_PoFMT "%" PRIo32
#define U64_PoFMT "%" PRIo64
#define  U8_PxFMT "%" PRIx8
#define U16_PxFMT "%" PRIx16
#define U32_PxFMT "%" PRIx32
#define U64_PxFMT "%" PRIx64
#define  U8_PXFMT "%" PRIX8
#define U16_PXFMT "%" PRIX16
#define U32_PXFMT "%" PRIX32
#define U64_PXFMT "%" PRIX64

#define ISIZE_PFMT "%" PRIdPTR

#define USIZE_PFMT  "%" PRIuPTR
#define USIZE_PoFMT "%" PRIoPTR
#define USIZE_PxFMT "%" PRIxPTR
#define USIZE_PXFMT "%" PRIXPTR

#define  I8_SFMT "%" SCNd8
#define I16_SFMT "%" SCNd16
#define I32_SFMT "%" SCNd32
#define I64_SFMT "%" SCNd64

#define  U8_SFMT  "%" SCNu8
#define U16_SFMT  "%" SCNu16
#define U32_SFMT  "%" SCNu32
#define U64_SFMT  "%" SCNu64
#define  U8_SoFMT "%" SCNo8
#define U16_SoFMT "%" SCNo16
#define U32_SoFMT "%" SCNo32
#define U64_SoFMT "%" SCNo64
#define  U8_SxFMT "%" SCNx8
#define U16_SxFMT "%" SCNx16
#define U32_SxFMT "%" SCNx32
#define U64_SxFMT "%" SCNx64

#define ISIZE_SFMT "%" SCNdPTR

#define USIZE_SFMT  "%" SCNuPTR
#define USIZE_SoFMT "%" SCNoPTR
#define USIZE_SxFMT "%" SCNxPTR

#ifdef __cplusplus
}
#endif

#endif // UTILS_NUM_H
