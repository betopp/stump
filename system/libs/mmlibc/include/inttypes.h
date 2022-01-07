//mmlibc/include/inttypes.h
//Integer type definitions for MMK's libc
//Bryan E. Topp <betopp@betopp.com> 2020
#ifndef _INTTYPES_H
#define _INTTYPES_H

#include <stdint.h>
#include <mmbits/typedef_imaxdiv.h>
#include <mmbits/typedef_wchar.h>

#if !defined(__LP64__)
	#define _PRI_8(ext) ext
	#define _PRI_16(ext) ext
	#define _PRI_32(ext) ext
	#define _PRI_64(ext) "ll" ext
	#define _PRI_MAX(ext) "ll" ext
	#define _PRI_PTR(ext) ext
#else
	#define _PRI_8(ext) ext
	#define _PRI_16(ext) ext
	#define _PRI_32(ext) ext
	#define _PRI_64(ext) "l" ext
	#define _PRI_MAX(ext) "l" ext
	#define _PRI_PTR(ext) "l" ext
#endif

#define PRId8       _PRI_8  ("d")
#define PRId16      _PRI_16 ("d")
#define PRId32      _PRI_32 ("d")
#define PRId64      _PRI_64 ("d")
#define PRIdLEAST8  _PRI_8  ("d")
#define PRIdLEAST16 _PRI_16 ("d")
#define PRIdLEAST32 _PRI_32 ("d")
#define PRIdLEAST64 _PRI_64 ("d")
#define PRIdFAST8   _PRI_8  ("d")
#define PRIdFAST16  _PRI_16 ("d")
#define PRIdFAST32  _PRI_32 ("d")
#define PRIdFAST64  _PRI_64 ("d")
#define PRIdMAX     _PRI_MAX("d")
#define PRIdPTR     _PRI_PTR("d")

#define PRIi8       _PRI_8  ("i")
#define PRIi16      _PRI_16 ("i")
#define PRIi32      _PRI_32 ("i")
#define PRIi64      _PRI_64 ("i")
#define PRIiLEAST8  _PRI_8  ("i")
#define PRIiLEAST16 _PRI_16 ("i")
#define PRIiLEAST32 _PRI_32 ("i")
#define PRIiLEAST64 _PRI_64 ("i")
#define PRIiFAST8   _PRI_8  ("i")
#define PRIiFAST16  _PRI_16 ("i")
#define PRIiFAST32  _PRI_32 ("i")
#define PRIiFAST64  _PRI_64 ("i")
#define PRIiMAX     _PRI_MAX("i")
#define PRIiPTR     _PRI_PTR("i")

#define PRIo8       _PRI_8  ("o")
#define PRIo16      _PRI_16 ("o")
#define PRIo32      _PRI_32 ("o")
#define PRIo64      _PRI_64 ("o")
#define PRIoLEAST8  _PRI_8  ("o")
#define PRIoLEAST16 _PRI_16 ("o")
#define PRIoLEAST32 _PRI_32 ("o")
#define PRIoLEAST64 _PRI_64 ("o")
#define PRIoFAST8   _PRI_8  ("o")
#define PRIoFAST16  _PRI_16 ("o")
#define PRIoFAST32  _PRI_32 ("o")
#define PRIoFAST64  _PRI_64 ("o")
#define PRIoMAX     _PRI_MAX("o")
#define PRIoPTR     _PRI_PTR("o")

#define PRIu8       _PRI_8  ("u")
#define PRIu16      _PRI_16 ("u")
#define PRIu32      _PRI_32 ("u")
#define PRIu64      _PRI_64 ("u")
#define PRIuLEAST8  _PRI_8  ("u")
#define PRIuLEAST16 _PRI_16 ("u")
#define PRIuLEAST32 _PRI_32 ("u")
#define PRIuLEAST64 _PRI_64 ("u")
#define PRIuFAST8   _PRI_8  ("u")
#define PRIuFAST16  _PRI_16 ("u")
#define PRIuFAST32  _PRI_32 ("u")
#define PRIuFAST64  _PRI_64 ("u")
#define PRIuMAX     _PRI_MAX("u")
#define PRIuPTR     _PRI_PTR("u")

#define PRIx8       _PRI_8  ("x")
#define PRIx16      _PRI_16 ("x")
#define PRIx32      _PRI_32 ("x")
#define PRIx64      _PRI_64 ("x")
#define PRIxLEAST8  _PRI_8  ("x")
#define PRIxLEAST16 _PRI_16 ("x")
#define PRIxLEAST32 _PRI_32 ("x")
#define PRIxLEAST64 _PRI_64 ("x")
#define PRIxFAST8   _PRI_8  ("x")
#define PRIxFAST16  _PRI_16 ("x")
#define PRIxFAST32  _PRI_32 ("x")
#define PRIxFAST64  _PRI_64 ("x")
#define PRIxMAX     _PRI_MAX("x")
#define PRIxPTR     _PRI_PTR("x")

#define PRIX8       _PRI_8  ("X")
#define PRIX16      _PRI_16 ("X")
#define PRIX32      _PRI_32 ("X")
#define PRIX64      _PRI_64 ("X")
#define PRIXLEAST8  _PRI_8  ("X")
#define PRIXLEAST16 _PRI_16 ("X")
#define PRIXLEAST32 _PRI_32 ("X")
#define PRIXLEAST64 _PRI_64 ("X")
#define PRIXFAST8   _PRI_8  ("X")
#define PRIXFAST16  _PRI_16 ("X")
#define PRIXFAST32  _PRI_32 ("X")
#define PRIXFAST64  _PRI_64 ("X")
#define PRIXMAX     _PRI_MAX("X")
#define PRIXPTR     _PRI_PTR("X")

#define SCNd8       _PRI_8  ("d")
#define SCNd16      _PRI_16 ("d")
#define SCNd32      _PRI_32 ("d")
#define SCNd64      _PRI_64 ("d")
#define SCNdLEAST8  _PRI_8  ("d")
#define SCNdLEAST16 _PRI_16 ("d")
#define SCNdLEAST32 _PRI_32 ("d")
#define SCNdLEAST64 _PRI_64 ("d")
#define SCNdFAST8   _PRI_8  ("d")
#define SCNdFAST16  _PRI_16 ("d")
#define SCNdFAST32  _PRI_32 ("d")
#define SCNdFAST64  _PRI_64 ("d")
#define SCNdMAX     _PRI_MAX("d")
#define SCNdPTR     _PRI_PTR("d")

#define SCNi8       _PRI_8  ("i")
#define SCNi16      _PRI_16 ("i")
#define SCNi32      _PRI_32 ("i")
#define SCNi64      _PRI_64 ("i")
#define SCNiLEAST8  _PRI_8  ("i")
#define SCNiLEAST16 _PRI_16 ("i")
#define SCNiLEAST32 _PRI_32 ("i")
#define SCNiLEAST64 _PRI_64 ("i")
#define SCNiFAST8   _PRI_8  ("i")
#define SCNiFAST16  _PRI_16 ("i")
#define SCNiFAST32  _PRI_32 ("i")
#define SCNiFAST64  _PRI_64 ("i")
#define SCNiMAX     _PRI_MAX("i")
#define SCNiPTR     _PRI_PTR("i")

#define SCNo8       _PRI_8  ("o")
#define SCNo16      _PRI_16 ("o")
#define SCNo32      _PRI_32 ("o")
#define SCNo64      _PRI_64 ("o")
#define SCNoLEAST8  _PRI_8  ("o")
#define SCNoLEAST16 _PRI_16 ("o")
#define SCNoLEAST32 _PRI_32 ("o")
#define SCNoLEAST64 _PRI_64 ("o")
#define SCNoFAST8   _PRI_8  ("o")
#define SCNoFAST16  _PRI_16 ("o")
#define SCNoFAST32  _PRI_32 ("o")
#define SCNoFAST64  _PRI_64 ("o")
#define SCNoMAX     _PRI_MAX("o")
#define SCNoPTR     _PRI_PTR("o")

#define SCNu8       _PRI_8  ("u")
#define SCNu16      _PRI_16 ("u")
#define SCNu32      _PRI_32 ("u")
#define SCNu64      _PRI_64 ("u")
#define SCNuLEAST8  _PRI_8  ("u")
#define SCNuLEAST16 _PRI_16 ("u")
#define SCNuLEAST32 _PRI_32 ("u")
#define SCNuLEAST64 _PRI_64 ("u")
#define SCNuFAST8   _PRI_8  ("u")
#define SCNuFAST16  _PRI_16 ("u")
#define SCNuFAST32  _PRI_32 ("u")
#define SCNuFAST64  _PRI_64 ("u")
#define SCNuMAX     _PRI_MAX("u")
#define SCNuPTR     _PRI_PTR("u")

#define SCNx8       _PRI_8  ("x")
#define SCNx16      _PRI_16 ("x")
#define SCNx32      _PRI_32 ("x")
#define SCNx64      _PRI_64 ("x")
#define SCNxLEAST8  _PRI_8  ("x")
#define SCNxLEAST16 _PRI_16 ("x")
#define SCNxLEAST32 _PRI_32 ("x")
#define SCNxLEAST64 _PRI_64 ("x")
#define SCNxFAST8   _PRI_8  ("x")
#define SCNxFAST16  _PRI_16 ("x")
#define SCNxFAST32  _PRI_32 ("x")
#define SCNxFAST64  _PRI_64 ("x")
#define SCNxMAX     _PRI_MAX("x")
#define SCNxPTR     _PRI_PTR("x")

intmax_t imaxabs(intmax_t j);
imaxdiv_t imaxdiv(intmax_t numerator, intmax_t denominator);
intmax_t strtoimax(const char *nptr, char **endptr, int base);
uintmax_t strtoumax(const char *nptr, char **endptr, int base);
intmax_t wcstoimax(const wchar_t *nptr, wchar_t **endptr, int base);
uintmax_t wcstoumax(const wchar_t *nptr, wchar_t **endptr, int base);

#endif //_INTTYPES_H
