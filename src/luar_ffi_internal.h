/*
** luar_ffi_internal.h -- shared data model of the Luar FFI.
**
** The FFI is split into four implementation files:
**   luar_ffi_type.c   C type objects, interning, layout, conversions
**   luar_ffi_parse.c  lexer and recursive-descent cdef/type parser
**   luar_ffi.c        cdata values, metamethods, ffi.* functions
**   luar_ffi_call.c   ffi.C / ffi.load and libffi calls
**
** All C type objects are interned per Lua state.  CType memory is
** allocated from the state's tracked arena and released when the state
** is closed (the state userdata has a __gc).
**
** See Copyright Notice in lua.h and COPYRIGHT.
*/

#ifndef luar_ffi_internal_h
#define luar_ffi_internal_h

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <ffi.h>

#include "lua.h"
#include "lauxlib.h"

#include "luar_ffi.h"


/* Metatable names registered in the Lua registry. */
#define LUAR_FFI_CDATA_MT	"luar.ffi.cdata"
#define LUAR_FFI_CLIB_MT	"luar.ffi.clib"
#define LUAR_FFI_STATE_MT	"luar.ffi.state"

/* Sanity marker stored in every cdata userdata. */
#define LUAR_FFI_MAGIC		0x4C554652u	/* "LUFR" */

/* Uservalue slots of the FFI state userdata. */
#define FFI_UV_INTERN		1
#define FFI_UV_SYMS		2
#define FFI_UV_TYPEDEFS		3
#define FFI_UV_TAGS		4
#define FFI_UV_CONSTS		5
#define FFI_UV_LIB		6
#define FFI_UV_TLS		7
#define FFI_NUV			7

/* Unknown / not computable size. */
#define FFI_SIZE_UNKNOWN	((size_t)-1)

/* Maximum single allocation accepted by ffi.new (defensive limit). */
#define FFI_MAX_NEW		((size_t)1 << 30)


/* ------------------------------------------------------------------ */
/* Types                                                              */
/* ------------------------------------------------------------------ */

typedef struct FFIState FFIState;
typedef struct CType CType;
typedef struct CField CField;
typedef struct CData CData;

typedef enum {
  CT_VOID,
  CT_BOOL,
  CT_INT,
  CT_FLOAT,
  CT_POINTER,
  CT_ARRAY,
  CT_STRUCT,
  CT_UNION,
  CT_ENUM,
  CT_FUNC
} CTypeKind;

struct CField {
  char *name;		/* NULL for an anonymous struct/union member */
  CType *type;
  size_t offset;
  uint8_t bitwidth;	/* 0 for a normal field */
  uint8_t bitoffset;	/* bit position inside the first byte */
  uint8_t zero_width;	/* unnamed zero-width padding bitfield */
  uint8_t packed;	/* byte-granular bitfield storage */
};

typedef struct FCallCache FCallCache;

struct CType {
  CTypeKind kind;
  uint8_t is_unsigned;
  uint8_t vla;		/* array with variable/unknown length */
  uint8_t incomplete;	/* forward-declared struct/union */
  uint8_t vararg;	/* function with '...' */
  uint8_t pack;		/* 0 natural, 1 packed, 2/4/8 #pragma pack */
  uint8_t align_override;	/* __attribute__((aligned(n))) */
  uint16_t nargs;
  uint16_t argcap;
  uint16_t nfields;
  uint16_t fieldcap;
  size_t size;		/* FFI_SIZE_UNKNOWN if unknown */
  size_t align;
  size_t length;	/* fixed array length, 0 for VLA */
  CType *child;		/* pointee, array element, enum base */
  CType *ret;		/* function return type */
  CType **args;		/* function argument types */
  CField *fields;	/* struct/union fields */
  char *name;		/* primitive display name or struct/union/enum tag */
  char *id;		/* canonical identity for interning */
  char *repr;		/* cached human-readable representation */
  int metatype_ref;	/* registry ref to ffi.metatype table, or LUA_NOREF */
  FCallCache *call;	/* lazily built libffi call cache */
};

/* Cdata flags. */
#define CDATA_OWNED	0x01u	/* storage is embedded after the header */
#define CDATA_REF	0x02u	/* value points at external memory */
#define CDATA_CTYPE	0x04u	/* this object denotes a C type */
#define CDATA_FUNC	0x08u	/* value is a C function address */
#define CDATA_CALLBACK	0x10u	/* CDATA_FUNC created by ffi.callback */

typedef union CDataValue {
  int64_t i64;
  uint64_t u64;
  double d;
  float f;
  void *p;
  unsigned char bytes[16];
  long double ld;	/* force max_align_t-like alignment */
} CDataValue;

struct CData {
  CType *ct;
  FFIState *S;
  size_t len;		/* array length (VLA or fixed) for arrays */
  uint32_t flags;
  uint32_t magic;
  void *aux;		/* ffi.callback closure descriptor, or NULL */
  CDataValue v;
};

/* Round the cdata header up so embedded storage stays maximally aligned. */
#define CDATA_HDR_SIZE	((sizeof(CData) + 15u) & ~(size_t)15u)

typedef struct FAlloc {
  struct FAlloc *next;
} FAlloc;

struct FFIState {
  lua_State *L;
  FAlloc *allocs;	/* tracked allocations, freed on state close */
  void *macros;		/* preprocessor macros (luar_ffi_preproc.c) */
  unsigned long long anon;	/* anonymous struct/union/enum counter */
  int pack_align;		/* active #pragma pack alignment, 0 natural */

  /* Builtin types, initialized once per state. */
  CType *t_void, *t_bool;
  CType *t_char, *t_schar, *t_uchar;
  CType *t_short, *t_ushort;
  CType *t_int, *t_uint;
  CType *t_long, *t_ulong;
  CType *t_llong, *t_ullong;
  CType *t_float, *t_double;
  CType *t_voidptr, *t_charptr;
};


/* ------------------------------------------------------------------ */
/* Tracked allocation                                                 */
/* ------------------------------------------------------------------ */

#define FFI_FALLOC_HDR	((sizeof(FAlloc) + 15u) & ~(size_t)15u)

void *ffi_alloc (FFIState *S, size_t sz);
char *ffi_strdup (FFIState *S, const char *s);
char *ffi_asprintf (FFIState *S, const char *fmt, ...);

void ffi_state_free (FFIState *S);


/* ------------------------------------------------------------------ */
/* State / table access                                               */
/* ------------------------------------------------------------------ */

FFIState *ffi_getstate (lua_State *L);
void ffi_push_uv (lua_State *L, FFIState *S, int uv);

CType *ffi_table_get_type (lua_State *L, FFIState *S, int uv,
                           const char *name);
void ffi_table_set_type (lua_State *L, FFIState *S, int uv,
                         const char *name, CType *ct);
int ffi_table_get_int (lua_State *L, FFIState *S, int uv,
                       const char *name, int64_t *out);
void ffi_table_set_int (lua_State *L, FFIState *S, int uv,
                        const char *name, int64_t v);


/* ------------------------------------------------------------------ */
/* Type construction and queries                                     */
/* ------------------------------------------------------------------ */

CType *ffi_type_new (FFIState *S, CTypeKind kind);
CType *ffi_type_intern (lua_State *L, FFIState *S, CType *ct);
void ffi_push_ctype (lua_State *L, FFIState *S, CType *ct);

void ffi_type_init_builtins (lua_State *L, FFIState *S);
CType *ffi_type_ptr (lua_State *L, FFIState *S, CType *child);
CType *ffi_type_arr (lua_State *L, FFIState *S, CType *child,
                       size_t len, int vla);
CType *ffi_type_func (lua_State *L, FFIState *S, CType *ret,
                          CType **args, int nargs, int vararg);
CType *ffi_type_new_struct (lua_State *L, FFIState *S, int is_union,
                            const char *tag);
CType *ffi_type_new_enum (lua_State *L, FFIState *S, const char *tag);
void ffi_type_add_field (FFIState *S, CType *ct, const char *name,
                         CType *type);
void ffi_type_finish_struct (FFIState *S, CType *ct);
CField *ffi_type_find_field (CType *ct, const char *name,
                             size_t *offset, CType **fieldtype);

const char *ffi_type_repr (FFIState *S, CType *ct);
size_t ffi_type_size (CType *ct);
size_t ffi_type_align (CType *ct);

/* Scalar memory access (little-endian native loads; see doc/ffi.md). */
static inline uint64_t ffi_load_uint (const void *p, size_t sz) {
  uint64_t v = 0;
  memcpy(&v, p, sz);
  return v;
}

static inline int64_t ffi_load_int (const void *p, size_t sz) {
  uint64_t u = ffi_load_uint(p, sz);
  switch (sz) {
    case 1: return (int64_t)(int8_t)u;
    case 2: return (int64_t)(int16_t)u;
    case 4: return (int64_t)(int32_t)u;
    default: return (int64_t)u;
  }
}

static inline void ffi_store_uint (void *p, size_t sz, uint64_t v) {
  memcpy(p, &v, sz);
}

static inline void ffi_store_int (void *p, size_t sz, int64_t v) {
  ffi_store_uint(p, sz, (uint64_t)v);
}

static inline size_t ffi_cdata_size (CData *cd) {
  if (cd->ct->kind == CT_ARRAY && cd->ct->vla) {
    size_t esz = ffi_type_size(cd->ct->child);
    if (esz == FFI_SIZE_UNKNOWN || cd->len > FFI_SIZE_UNKNOWN / esz)
      return FFI_SIZE_UNKNOWN;
    return esz * cd->len;
  }
  return ffi_type_size(cd->ct);
}


/* ------------------------------------------------------------------ */
/* Cdata helpers (luar_ffi.c)                                         */
/* ------------------------------------------------------------------ */

int ffi_iscdata (lua_State *L, int idx);
CData *ffi_tocdata (lua_State *L, int idx);
CData *ffi_checkcdata (lua_State *L, int idx, const char *fname);
CType *ffi_checkctype (lua_State *L, int idx, const char *fname);

CData *ffi_push_cdata_object (lua_State *L, FFIState *S, CType *ct,
                              uint32_t flags, size_t payload);
CData *ffi_push_owned (lua_State *L, FFIState *S, CType *ct,
                       size_t payload);
CData *ffi_push_ref (lua_State *L, FFIState *S, CType *ct, void *p,
                     int owneridx, size_t len);
CData *ffi_push_func (lua_State *L, FFIState *S, CType *ct, void *p);
CData *ffi_push_scalar (lua_State *L, FFIState *S, CType *ct);
void ffi_push_value (lua_State *L, FFIState *S, CType *ct, void *p,
                     int owneridx);
void ffi_push_value_len (lua_State *L, FFIState *S, CType *ct, void *p,
                         int owneridx, size_t len);
int ffi_convert (lua_State *L, FFIState *S, CType *ct, void *dst,
                 int idx, const char *what);
uint64_t ffi_to_uint (lua_State *L, FFIState *S, int idx, int *ok);
int64_t ffi_to_int (lua_State *L, FFIState *S, int idx, int *ok);
double ffi_to_double (lua_State *L, FFIState *S, int idx, int *ok);
void *ffi_to_ptr (lua_State *L, FFIState *S, int idx, int *ok);
int ffi_do_new (lua_State *L, FFIState *S, CType *ct, int argstart);
int ffi_load (lua_State *L);

void ffi_libffi_init (lua_State *L);
void ffi_clib_initmt (lua_State *L);
void ffi_push_clib (lua_State *L, FFIState *S, void *handle, int is_default);
void ffi_push_default_clib (lua_State *L, FFIState *S);

int ffi_call_cdata (lua_State *L, FFIState *S, CData *fn, int argstart);

/* ffi.callback: create a C closure that calls a Lua function. */
int ffi_callback_new (lua_State *L, FFIState *S, CType *ft, int funcidx);
void ffi_callback_free (lua_State *L, CData *cd);
int ffi_trace_call0 (CData *fn, int *kind, int64_t *iv, double *dv);
int ffi_trace_call1i (CData *fn, int64_t arg, int64_t *out);
int ffi_trace_call1d_iarg (CData *fn, int64_t arg, double *out);
int ffi_trace_call1d (CData *fn, double arg, double *out);
int ffi_callback_free_mt (lua_State *L);
int ffi_callback_set_mt (lua_State *L);


/* ------------------------------------------------------------------ */
/* Parser                                                             */
/* ------------------------------------------------------------------ */

void ffi_parse_cdef (lua_State *L, FFIState *S, const char *src, size_t len);
char *ffi_preprocess (lua_State *L, FFIState *S, const char *src, size_t len,
                      size_t *outlen);
CType *ffi_parse_type (lua_State *L, FFIState *S, const char *src,
                       size_t len);


/* ------------------------------------------------------------------ */
/* Predicates                                                         */
/* ------------------------------------------------------------------ */

#define ffi_is_integer(ct) \
  ((ct)->kind == CT_INT || (ct)->kind == CT_BOOL || (ct)->kind == CT_ENUM)

#define ffi_is_numeric(ct) \
  ((ct)->kind == CT_INT || (ct)->kind == CT_BOOL || \
   (ct)->kind == CT_FLOAT || (ct)->kind == CT_ENUM)

#define ffi_is_aggregate(ct) \
  ((ct)->kind == CT_STRUCT || (ct)->kind == CT_UNION || \
   (ct)->kind == CT_ARRAY)

#define ffi_is_pointer(ct)	((ct)->kind == CT_POINTER)

#define ffi_is_func(ct)		((ct)->kind == CT_FUNC)

#define ffi_is_ptr_or_array(ct) \
  ((ct)->kind == CT_POINTER || (ct)->kind == CT_ARRAY)


#endif
