/*
** luar_ffi.c -- placeholder FFI library for builds without libffi.
**
** Builds with WITH_FFI=0 use this file: the 'ffi' module is not opened
** at all (luar.features.ffi is false), so this code exists only so that
** a C host calling luaopen_ffi() directly gets a clear error instead of
** a link failure.
**
** See Copyright Notice in lua.h and COPYRIGHT.
*/

#define LUA_LIB
#define LUAR_LIB

#include "lprefix.h"

#include <stddef.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"
#include "luar.h"
#include "lobject.h"


LUA_API int luar_has_ffi (void) {
  return 0;
}


LUALIB_API int luaopen_ffi (lua_State *L) {
  return luaL_error(L, "Luar: this build has no FFI support "
                       "(rebuild with WITH_FFI=1 and libffi)");
}


/* J4 FFI helpers: no FFI in this build. */
int luar_ffi_trace_kind (TValue *tv) { (void)tv; return -1; }
int64_t luar_ffi_trace_cget_int (TValue *cdv, TValue *kt, TValue *out) {
  (void)cdv; (void)kt; (void)out; return 0;
}
int64_t luar_ffi_trace_cget_double (TValue *cdv, TValue *kt, TValue *out) {
  (void)cdv; (void)kt; (void)out; return 0;
}
int64_t luar_ffi_trace_cset_int (TValue *cdv, TValue *kt, TValue *val) {
  (void)cdv; (void)kt; (void)val; return 0;
}
int64_t luar_ffi_trace_cset_double (TValue *cdv, TValue *kt, TValue *val) {
  (void)cdv; (void)kt; (void)val; return 0;
}

int luar_ffi_trace_field (TValue *cdv, const char *name, int *kind,
                          size_t *off, size_t *size) {
  (void)cdv; (void)name; (void)kind; (void)off; (void)size; return 0;
}
int64_t luar_ffi_trace_cget_field_int (TValue *cdv, size_t off, size_t sz,
                                       TValue *out) {
  (void)cdv; (void)off; (void)sz; (void)out; return 0;
}
int64_t luar_ffi_trace_cget_field_double (TValue *cdv, size_t off, size_t sz,
                                          TValue *out) {
  (void)cdv; (void)off; (void)sz; (void)out; return 0;
}

int64_t luar_ffi_trace_cset_field_int (TValue *cdv, size_t off, size_t sz,
                                       TValue *val) {
  (void)cdv; (void)off; (void)sz; (void)val; return 0;
}
int64_t luar_ffi_trace_cset_field_double (TValue *cdv, size_t off, size_t sz,
                                          TValue *val) {
  (void)cdv; (void)off; (void)sz; (void)val; return 0;
}

int64_t luar_ffi_trace_cset_field_int_const (TValue *cdv, size_t off,
                                             size_t sz, int64_t val) {
  (void)cdv; (void)off; (void)sz; (void)val; return 0;
}

int64_t luar_ffi_trace_cset_field_double_const (TValue *cdv, size_t off,
                                                size_t sz, double val) {
  (void)cdv; (void)off; (void)sz; (void)val; return 0;
}

int luar_ffi_trace_field2 (TValue *cdv, const char *outer, const char *inner,
                           int *kind, size_t *off, size_t *size) {
  (void)cdv; (void)outer; (void)inner; (void)kind; (void)off; (void)size;
  return 0;
}

int luar_ffi_trace_field3 (TValue *cdv, const char *n1, const char *n2,
                           const char *n3, int *kind, size_t *off,
                           size_t *size) {
  (void)cdv; (void)n1; (void)n2; (void)n3; (void)kind; (void)off;
  (void)size; return 0;
}

int luar_ffi_trace_ptr_field (TValue *cdv, const char *name, int *kind,
                              size_t *off, size_t *esz) {
  (void)cdv; (void)name; (void)kind; (void)off; (void)esz; return 0;
}
int64_t luar_ffi_trace_cget_ptr_int (TValue *cdv, size_t off, TValue *kt,
                                     TValue *out) {
  (void)cdv; (void)off; (void)kt; (void)out; return 0;
}
int64_t luar_ffi_trace_cget_ptr_double (TValue *cdv, size_t off, TValue *kt,
                                        TValue *out) {
  (void)cdv; (void)off; (void)kt; (void)out; return 0;
}

int64_t luar_ffi_trace_cset_ptr_int (TValue *cdv, size_t off, TValue *kt,
                                     TValue *val) {
  (void)cdv; (void)off; (void)kt; (void)val; return 0;
}
int64_t luar_ffi_trace_cset_ptr_double (TValue *cdv, size_t off, TValue *kt,
                                        TValue *val) {
  (void)cdv; (void)off; (void)kt; (void)val; return 0;
}

int64_t luar_ffi_trace_cget_ptr_i64 (TValue *cdv, size_t off, TValue *kt,
                                     TValue *out) {
  (void)cdv; (void)off; (void)kt; (void)out; return 0;
}
int64_t luar_ffi_trace_cset_ptr_i64 (TValue *cdv, size_t off, TValue *kt,
                                     TValue *val) {
  (void)cdv; (void)off; (void)kt; (void)val; return 0;
}

int luar_ffi_trace_field4 (TValue *cdv, const char *n1, const char *n2,
                           const char *n3, const char *n4, int *kind,
                           size_t *off, size_t *size) {
  (void)cdv; (void)n1; (void)n2; (void)n3; (void)n4; (void)kind;
  (void)off; (void)size; return 0;
}

int luar_ffi_trace_field5 (TValue *cdv, const char *n1, const char *n2,
                           const char *n3, const char *n4, const char *n5,
                           int *kind, size_t *off, size_t *size) {
  (void)cdv; (void)n1; (void)n2; (void)n3; (void)n4; (void)n5;
  (void)kind; (void)off; (void)size; return 0;
}

int luar_ffi_trace_fieldn (TValue *cdv, const char *const *names, int n,
                           int *kind, size_t *off, size_t *size) {
  (void)cdv; (void)names; (void)n; (void)kind; (void)off; (void)size;
  return 0;
}

int luar_ffi_trace_ccall0_kind (TValue *fn) { (void)fn; return 0; }
int64_t luar_ffi_trace_ccall0_double (TValue *fn, TValue *out) {
  (void)fn; (void)out;
  return 0;
}


int64_t luar_ffi_trace_ccall0_int (TValue *fn, TValue *out) {
  (void)fn; (void)out; return 0;
}
int luar_ffi_trace_ccall1_kind (TValue *fn) { (void)fn; return 0; }
int64_t luar_ffi_trace_ccall1i (TValue *fn, int64_t arg, int64_t *out) {
  (void)fn; (void)arg; (void)out; return 0;
}

int luar_ffi_trace_ccall1di_kind (TValue *fn) { (void)fn; return 0; }
int64_t luar_ffi_trace_ccall1di (TValue *fn, int64_t arg, TValue *out) {
  (void)fn; (void)arg; (void)out; return 0;
}
int luar_ffi_trace_ccall1dd_kind (TValue *fn) { (void)fn; return 0; }
int64_t luar_ffi_trace_ccall1dd (TValue *fn, double arg, TValue *out) {
  (void)fn; (void)arg; (void)out; return 0;
}
int luar_ffi_trace_ccall1ff_kind (TValue *fn) { (void)fn; return 0; }
int64_t luar_ffi_trace_ccall1ff (TValue *fn, double arg, TValue *out) {
  (void)fn; (void)arg; (void)out; return 0;
}
int luar_ffi_trace_ccall2dd_kind (TValue *fn) { (void)fn; return 0; }
int64_t luar_ffi_trace_ccall2dd (TValue *fn, double arg1, double arg2,
                                 TValue *out) {
  (void)fn; (void)arg1; (void)arg2; (void)out; return 0;
}

int luar_ffi_trace_ccall2id_kind (TValue *fn) { (void)fn; return 0; }
int64_t luar_ffi_trace_ccall2id (TValue *fn, int64_t a0, double a1,
                                 TValue *out) {
  (void)fn; (void)a0; (void)a1; (void)out; return 0;
}

int luar_ffi_trace_ccall2di_kind (TValue *fn) { (void)fn; return 0; }
int64_t luar_ffi_trace_ccall2di (TValue *fn, double a0, int64_t a1,
                                 TValue *out) {
  (void)fn; (void)a0; (void)a1; (void)out; return 0;
}

int luar_ffi_trace_ccall2ii_kind (TValue *fn) { (void)fn; return 0; }
int64_t luar_ffi_trace_ccall2ii (TValue *fn, int64_t a0, int64_t a1,
                                 TValue *out) {
  (void)fn; (void)a0; (void)a1; (void)out; return 0;
}
int luar_ffi_trace_ccall3dd_kind (TValue *fn) { (void)fn; return 0; }
int64_t luar_ffi_trace_ccall3dd (TValue *fn, double a0, double a1,
                                 double a2, TValue *out) {
  (void)fn; (void)a0; (void)a1; (void)a2; (void)out; return 0;
}

int luar_ffi_trace_ccall2df_kind (TValue *fn) { (void)fn; return 0; }
int64_t luar_ffi_trace_ccall2df (TValue *fn, double a0, double a1,
                                 TValue *out) {
  (void)fn; (void)a0; (void)a1; (void)out; return 0;
}

int luar_ffi_trace_ccall2fd_kind (TValue *fn) { (void)fn; return 0; }
int64_t luar_ffi_trace_ccall2fd (TValue *fn, double a0, double a1,
                                 TValue *out) {
  (void)fn; (void)a0; (void)a1; (void)out; return 0;
}

int luar_ffi_trace_ccall2if_kind (TValue *fn) { (void)fn; return 0; }
int64_t luar_ffi_trace_ccall2if (TValue *fn, int64_t a0, double a1,
                                 TValue *out) {
  (void)fn; (void)a0; (void)a1; (void)out; return 0;
}

int luar_ffi_trace_ccall2fi_kind (TValue *fn) { (void)fn; return 0; }
int64_t luar_ffi_trace_ccall2fi (TValue *fn, double a0, int64_t a1,
                                 TValue *out) {
  (void)fn; (void)a0; (void)a1; (void)out; return 0;
}

int luar_ffi_trace_ccall2ff_kind (TValue *fn) { (void)fn; return 0; }
int64_t luar_ffi_trace_ccall2ff (TValue *fn, double a0, double a1,
                                 TValue *out) {
  (void)fn; (void)a0; (void)a1; (void)out; return 0;
}
