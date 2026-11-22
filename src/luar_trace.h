/*
** luar_trace.h -- Luar J4 tracing-IR and exit-state model.
**
** This is the first layer of a tracing JIT, deliberately independent from
** the existing one-pass loop emitter.  Traces are recorded as a small,
** explicitly verified IR; exit snapshots carry the tags and payloads of
** live stack slots so a side trace can be linked from an interior PC.
*/
#ifndef luar_trace_h
#define luar_trace_h

#include <stddef.h>
#include <stdint.h>

#include "lobject.h"

#define LUAR_IR_MAX_INS		1024
#define LUAR_IR_MAX_EXITS	4
#define LUAR_IR_MAX_SLOTS	64
#define LUAR_IR_VERSION		1
#define LUAR_IR_MAX_DEPTH	4

typedef enum {
  LUAR_IR_KINT = 1,	/* value = signed integer constant */
  LUAR_IR_KFLOAT,	/* value = IEEE double constant */
  LUAR_IR_SLOAD,	/* a = destination slot, b = source slot */
  LUAR_IR_ULOAD,	/* a = dest, ival = LClosure*, nval = upvalue index */
  LUAR_IR_SSTORE,	/* a = slot, b = source slot */
  LUAR_IR_LOADI,	/* a = destination slot, ival = constant */
  LUAR_IR_LOADF,	/* a = destination slot, nval = float constant */
  LUAR_IR_KSTR,		/* a = destination slot, ival = TString pointer */
  LUAR_IR_KBOOL,	/* a = destination slot, nval = boolean */
  LUAR_IR_ADDI,		/* a = destination, b = source, ival = immediate */
  LUAR_IR_ADDK,
  LUAR_IR_SUBK,
  LUAR_IR_MULK,
  LUAR_IR_DIVK,
  LUAR_IR_MODK,
  LUAR_IR_IDIVK,
  LUAR_IR_GUARD,	/* a = slot, b = LUAR_IR_TAG_* expected */
  LUAR_IR_ADD,		/* a = dest, b/c = operands */
  LUAR_IR_SUB,
  LUAR_IR_MUL,
  LUAR_IR_DIV,
  LUAR_IR_TGET,
  LUAR_IR_CGETI,	/* FFI cdata integer array read */
  LUAR_IR_CGETF,	/* FFI cdata double array read */
  LUAR_IR_CSETI,	/* FFI cdata integer array write */
  LUAR_IR_CSETF,	/* FFI cdata float/double array write */
  LUAR_IR_CGETFIELD,	/* FFI cdata scalar struct field read */
  LUAR_IR_CSETFIELD,	/* FFI cdata scalar struct field write */
  LUAR_IR_CSETFIELDK,	/* FFI cdata scalar struct field write, integer K */
  LUAR_IR_CPGETI,	/* FFI cdata pointer-field integer element read */
  LUAR_IR_CPGETF,	/* FFI cdata pointer-field double element read */
  LUAR_IR_CPSETI,	/* FFI cdata pointer-field integer element write */
  LUAR_IR_CPSETF,	/* FFI cdata pointer-field double element write */
  LUAR_IR_CPGETI64,	/* FFI cdata pointer-field 8-byte int read */
  LUAR_IR_CPSETI64,	/* FFI cdata pointer-field 8-byte int write */
  LUAR_IR_CCALL0I,
  LUAR_IR_CCALL0D,	/* native zero-argument C call, integer result */		/* a = dest, b = table, c = key */
  LUAR_IR_CCALL1I,	/* native one-integer-argument C call, integer result */
  LUAR_IR_CCALL1DI,	/* native int arg/double return C call */
  LUAR_IR_CCALL1DD,	/* native double arg/double return C call */
  LUAR_IR_CCALL1FF,	/* native float arg/float return C call */
  LUAR_IR_CCALL2DD,	/* native two-double-arg/double-return C call */
  LUAR_IR_CCALL2FF,
  LUAR_IR_CCALL2FI,
  LUAR_IR_CCALL2IF,
  LUAR_IR_CCALL2FD,
  LUAR_IR_CCALL2DF,	/* native two-float32-arg/float32-return C call */
  LUAR_IR_CCALL2DI,
  LUAR_IR_CCALL2ID,	/* native (double,int) C call, double return */
  LUAR_IR_CCALL2II,	/* native (int,int) C call, integer return */
  LUAR_IR_TNEW,		/* allocate a table (sizes in ival/nval) */
  LUAR_IR_TSETI,	/* table[ival] = b, relocated base returned */
  LUAR_IR_TSETIF,	/* table[ival] = float slot b */
  LUAR_IR_TSETIS,	/* table[ival] = string slot b */
  LUAR_IR_TSETIB,	/* table[ival] = boolean slot b */
  LUAR_IR_TSETIT,	/* table[ival] = table slot b */
  LUAR_IR_TGETI,	/* a = table[ival], integer result */
  LUAR_IR_TGETK,	/* a = table[key string ptr in ival] */
  LUAR_IR_TGETKF,	/* a = table[key string ptr], float result */
  LUAR_IR_TGETKS,	/* a = table[key string ptr], string result */
  LUAR_IR_TGETKB,	/* a = table[key string ptr], boolean result */
  LUAR_IR_TGETKT,	/* a = table[key string ptr], table result */
  LUAR_IR_CONCAT2,	/* a = base[a] .. base[a+1] */
  LUAR_IR_CONCAT3,	/* a = base[a] .. base[a+1] .. base[a+2] */
  LUAR_IR_CONCATN,	/* a = base[a] .. ... base[a+ival-1], ival 2..32 */
  LUAR_IR_STRLEN,	/* a = #base[b], string */
  LUAR_IR_TLEN,		/* a = #base[b], table without __len */
  LUAR_IR_TSETK,	/* table[key string ptr] = b, relocated base */
  LUAR_IR_TSETKK,	/* table[key string ptr] = constant int (nval) */
  LUAR_IR_TSETKF,	/* table[key string ptr] = float slot b */
  LUAR_IR_TSETKKF,	/* table[key string ptr] = constant float (nval) */
  LUAR_IR_TSETKKS,	/* table[key] = constant string (nval pointer bits) */
  LUAR_IR_TSETKS,	/* table[key string ptr] = string slot b */
  LUAR_IR_TSETKT,	/* table[key string ptr] = table slot b */
  LUAR_IR_TSETKB,	/* table[key string ptr] = boolean (ival) */
  LUAR_IR_TSETKDB,	/* table[key string ptr] = boolean slot b */
  LUAR_IR_TSETUPKF,	/* closure upvalue table[key] = constant float */
  LUAR_IR_TSETUPKI,	/* closure upvalue table[key] = constant int */
  LUAR_IR_TSETUPKS,	/* closure upvalue table[key] = constant string */
  LUAR_IR_TSETUPKB,	/* closure upvalue table[key] = constant bool */
  LUAR_IR_TSETUPKT,	/* closure upvalue table[key] = table slot b */
  LUAR_IR_TSETUPKV,	/* closure upvalue table[key] = slot b (any non-nil) */
  LUAR_IR_TSETUPKN,	/* closure upvalue table[key] = nil (delete) */
  LUAR_IR_TSETDYN,	/* table[b] = c, dynamic string/int key */
  LUAR_IR_TSETDYN_KF,	/* table[b] = constant float (nval), dynamic key */
  LUAR_IR_TSETDYN_KS,	/* table[b] = constant string (nval ptr bits) */
  LUAR_IR_TSETDYN_KI,	/* table[b] = constant int (nval bits) */
  LUAR_IR_TSETDYN_KB,	/* table[b] = constant bool (ival) */
  LUAR_IR_CCALL3DD,
  LUAR_IR_CCALL3DDI,
  LUAR_IR_CCALL3DID,
  LUAR_IR_CCALL3IDD,	/* native three-double-arg/double-return C call */
  LUAR_IR_TGETF,	/* float table read */
  LUAR_IR_FADDI,	/* a = float base[b] + constant (nval) */
  LUAR_IR_FMULK,	/* float multiply by nval */
  LUAR_IR_FADD,		/* float add */
  LUAR_IR_FSUB,		/* float subtract */
  LUAR_IR_FDIVK,	/* float divide by constant */
  LUAR_IR_TSET,		/* a = table, b = key, c = value */
  LUAR_IR_CGUARD,	/* a = slot, ival = expected closure pointer */
  LUAR_IR_CALLEXIT,	/* terminal: resume interpreter at CALL pc */
  LUAR_IR_FORLOOP,	/* a = control base slot */
  LUAR_IR_EXIT,		/* a = exit index */
  LUAR_IR_RET
} LuaR_IRKind;

enum {
  LUAR_IR_TAG_INT = 1,
  LUAR_IR_TAG_FLOAT = 2,
  LUAR_IR_TAG_TABLE = 3,
  LUAR_IR_TAG_OTHER = 4,
  LUAR_IR_TAG_STRING = 5,
  LUAR_IR_TAG_USERDATA = 6
};

typedef struct LuaR_IRIns {
  uint8_t kind;
  uint8_t a, b, c;
  int32_t pc;		/* originating Proto instruction, -1 if synthetic */
  int64_t ival;
  double nval;
} LuaR_IRIns;

typedef struct LuaR_IRExitState {
  int32_t pc;			/* interpreter resume instruction */
  uint8_t nslots;
  uint8_t tags[LUAR_IR_MAX_SLOTS];
  uint64_t bits[LUAR_IR_MAX_SLOTS];
} LuaR_IRExitState;

typedef struct LuaR_TraceIR {
  Proto *proto;		/* informational; not owned */
  int32_t startpc;
  int32_t maxslots;
  int depth;		/* side-trace depth, 0 for a root trace */
  int side;		/* 1 = interior side segment */
  int partial;		/* 1 = trace prefix ending at a CALL */
  int nins;
  LuaR_IRIns ins[LUAR_IR_MAX_INS];
  int nexits;
  LuaR_IRExitState exits[LUAR_IR_MAX_EXITS];
} LuaR_TraceIR;

void luar_trace_ir_init (LuaR_TraceIR *t, Proto *proto, int32_t startpc,
                         int32_t maxslots, int depth);
int luar_trace_ir_emit (LuaR_TraceIR *t, const LuaR_IRIns *ins);
int luar_trace_ir_add_exit (LuaR_TraceIR *t, const LuaR_IRExitState *ex);
int luar_trace_ir_verify (const LuaR_TraceIR *t, char *err, size_t errlen);

/* Snapshot/restore live stack slots.  A snapshot is short-lived and must not
** be used across a GC safepoint; restoring into stack slots needs no barrier.
*/
int luar_trace_ir_snapshot (LuaR_IRExitState *ex, int32_t pc, TValue *base,
                            int nslots);
int luar_trace_ir_restore (const LuaR_IRExitState *ex, TValue *base);

int luar_trace_ir_analyze (LuaR_TraceIR *t, char *err, size_t errlen);
int luar_trace_record_suffix (Proto *p, int start, int backedge,
                              TValue *base, lua_State *L,
                              LuaR_TraceIR *t);
int luar_trace_record_call_prefix (Proto *p, int start, int backedge,
                                   TValue *base, lua_State *L,
                                   LuaR_TraceIR *t, int *callpc);
int luar_ffi_trace_kind (TValue *tv);
int64_t luar_ffi_trace_cget_int (TValue *cdv, TValue *kt, TValue *out);
int64_t luar_ffi_trace_cget_double (TValue *cdv, TValue *kt, TValue *out);
int64_t luar_ffi_trace_cset_int (TValue *cdv, TValue *kt, TValue *val);
int64_t luar_ffi_trace_cset_double (TValue *cdv, TValue *kt, TValue *val);
int luar_ffi_trace_field (TValue *cdv, const char *name, int *kind,
                          size_t *off, size_t *size);
int luar_ffi_trace_field2 (TValue *cdv, const char *outer, const char *inner,
                           int *kind, size_t *off, size_t *size);
int luar_ffi_trace_field3 (TValue *cdv, const char *n1, const char *n2,
                           const char *n3, int *kind, size_t *off,
                           size_t *size);
int luar_ffi_trace_field4 (TValue *cdv, const char *n1, const char *n2,
                           const char *n3, const char *n4, int *kind,
                           size_t *off, size_t *size);
int luar_ffi_trace_ccall0_kind (TValue *fn);
int64_t luar_ffi_trace_ccall0_double (TValue *fn, TValue *out);
int64_t luar_ffi_trace_ccall0_int (TValue *fn, TValue *out);
int luar_ffi_trace_ccall1_kind (TValue *fn);
int luar_ffi_trace_ccall1di_kind (TValue *fn);
int64_t luar_ffi_trace_ccall1di (TValue *fn, int64_t arg, TValue *out);
int luar_ffi_trace_ccall1dd_kind (TValue *fn);
int luar_ffi_trace_ccall1ff_kind (TValue *fn);
int64_t luar_ffi_trace_ccall1ff (TValue *fn, double arg, TValue *out);
int64_t luar_ffi_trace_ccall1dd (TValue *fn, double arg, TValue *out);
int luar_ffi_trace_ccall2dd_kind (TValue *fn);
int luar_ffi_trace_ccall3did_kind (TValue *fn);
int64_t luar_ffi_trace_ccall3did (TValue *fn, double arg1, int64_t arg2,
                                  double arg3, TValue *out);
int luar_ffi_trace_ccall3idd_kind (TValue *fn);
int64_t luar_ffi_trace_ccall3idd (TValue *fn, int64_t arg1, double arg2,
                                  double arg3, TValue *out);
int luar_ffi_trace_ccall3ddi_kind (TValue *fn);
int64_t luar_ffi_trace_ccall3ddi (TValue *fn, double arg1, double arg2,
                                  int64_t arg3, TValue *out);
int luar_ffi_trace_ccall3dd_kind (TValue *fn);
int64_t luar_ffi_trace_ccall3dd (TValue *fn, double arg1, double arg2,
                                 double arg3, TValue *out);
int luar_ffi_trace_ccall2id_kind (TValue *fn);
int64_t luar_ffi_trace_ccall2id (TValue *fn, int64_t arg1, double arg2,
                                 TValue *out);
int luar_ffi_trace_ccall2di_kind (TValue *fn);
int64_t luar_ffi_trace_ccall2di (TValue *fn, double arg1, int64_t arg2,
                                 TValue *out);
int luar_ffi_trace_ccall2ii_kind (TValue *fn);
int64_t luar_ffi_trace_ccall2ii (TValue *fn, int64_t arg1, int64_t arg2,
                                 TValue *out);
int64_t luar_ffi_trace_ccall2dd (TValue *fn, double arg1, double arg2,
                                 TValue *out);
int luar_ffi_trace_ccall2df_kind (TValue *fn);
int64_t luar_ffi_trace_ccall2df (TValue *fn, double arg1, double arg2,
                                 TValue *out);
int luar_ffi_trace_ccall2fd_kind (TValue *fn);
int64_t luar_ffi_trace_ccall2fd (TValue *fn, double arg1, double arg2,
                                 TValue *out);
int luar_ffi_trace_ccall2if_kind (TValue *fn);
int64_t luar_ffi_trace_ccall2if (TValue *fn, int64_t arg1, double arg2,
                                 TValue *out);
int luar_ffi_trace_ccall2fi_kind (TValue *fn);
int64_t luar_ffi_trace_ccall2fi (TValue *fn, double arg1, int64_t arg2,
                                 TValue *out);
int luar_ffi_trace_ccall2ff_kind (TValue *fn);
int64_t luar_ffi_trace_ccall2ff (TValue *fn, double arg1, double arg2,
                                 TValue *out);
int64_t luar_ffi_trace_ccall1i (TValue *fn, int64_t arg, int64_t *out);
int luar_ffi_trace_fieldn (TValue *cdv, const char *const *names, int n,
                           int *kind, size_t *off, size_t *size);
int luar_ffi_trace_field5 (TValue *cdv, const char *n1, const char *n2,
                           const char *n3, const char *n4, const char *n5,
                           int *kind, size_t *off, size_t *size);
int luar_ffi_trace_ptr_field (TValue *cdv, const char *name, int *kind,
                              size_t *off, size_t *esz);
int64_t luar_ffi_trace_cget_ptr_int (TValue *cdv, size_t off, TValue *kt,
                                     TValue *out);
int64_t luar_ffi_trace_cget_ptr_double (TValue *cdv, size_t off, TValue *kt,
                                        TValue *out);
int64_t luar_ffi_trace_cset_ptr_int (TValue *cdv, size_t off, TValue *kt,
                                     TValue *val);
int64_t luar_ffi_trace_cset_ptr_double (TValue *cdv, size_t off, TValue *kt,
                                        TValue *val);
int64_t luar_ffi_trace_cget_ptr_i64 (TValue *cdv, size_t off, TValue *kt,
                                     TValue *out);
int64_t luar_ffi_trace_cset_ptr_i64 (TValue *cdv, size_t off, TValue *kt,
                                     TValue *val);
int64_t luar_ffi_trace_cget_field_int (TValue *cdv, size_t off, size_t sz,
                                       TValue *out);
int64_t luar_ffi_trace_cget_field_double (TValue *cdv, size_t off, size_t sz,
                                          TValue *out);
int64_t luar_ffi_trace_cset_field_int (TValue *cdv, size_t off, size_t sz,
                                       TValue *val);
int64_t luar_ffi_trace_cset_field_double (TValue *cdv, size_t off, size_t sz,
                                          TValue *val);
int64_t luar_ffi_trace_cset_field_int_const (TValue *cdv, size_t off,
                                             size_t sz, int64_t val);
int64_t luar_ffi_trace_cset_field_double_const (TValue *cdv, size_t off,
                                                size_t sz, double val);
int64_t luar_trace_tget_int (TValue *tv, lua_Integer key, int64_t *out);
int64_t luar_trace_fsub (TValue *a, TValue *b, TValue *c);
int64_t luar_trace_fdivk (TValue *a, TValue *b, double k);
int64_t luar_trace_fadd (TValue *a, TValue *b, TValue *c);
int64_t luar_trace_faddk (TValue *out, TValue *src, double k);
int64_t luar_trace_fmulk (TValue *a, TValue *b, double k);
int luar_trace_table_elem_tag (TValue *tv, lua_Integer key);
int64_t luar_trace_tgetf_generic (TValue *tv, lua_Integer key, double *out);
int64_t luar_trace_tget_generic (TValue *tv, lua_Integer key, int64_t *out);
int64_t luar_trace_uload (lua_State *L, TValue *out, LClosure *expected,
                          int idx);
int64_t luar_trace_tset_int (TValue *tv, lua_Integer key, TValue *val);
int64_t luar_trace_tset_hash (TValue *tv, lua_Integer key, TValue *val);
int64_t luar_trace_tnew (lua_State *L, TValue *out, int asize, int hsize);
TValue *luar_trace_tset_insert (TValue *tv, lua_Integer key, TValue *val,
                                lua_State *L);
TValue *luar_trace_tseti (TValue *tv, lua_Integer key, TValue *val,
                          lua_State *L);
TValue *luar_trace_tsetif (TValue *tv, lua_Integer key, TValue *val,
                           lua_State *L);
TValue *luar_trace_tsetis (TValue *tv, lua_Integer key, TValue *val,
                           lua_State *L);
TValue *luar_trace_tsetib (TValue *tv, lua_Integer key, TValue *val,
                           lua_State *L);
TValue *luar_trace_tsetit (TValue *tv, lua_Integer key, TValue *val,
                           lua_State *L);
int64_t luar_trace_tgeti (TValue *tv, lua_Integer key, TValue *out);
TValue *luar_trace_tsetk (TValue *tv, TString *key, TValue *val,
                          lua_State *L);
TValue *luar_trace_tsetkk (TValue *tv, TString *key, lua_Integer val,
                           lua_State *L);
TValue *luar_trace_tsetkf (TValue *tv, TString *key, TValue *val,
                           lua_State *L);
TValue *luar_trace_tsetkkf (TValue *tv, TString *key, uint64_t bits,
                            lua_State *L);
TValue *luar_trace_tsetkks (TValue *tv, TString *key, uint64_t bits,
                            lua_State *L);
TValue *luar_trace_tsetks (TValue *tv, TString *key, TValue *val,
                           lua_State *L);
TValue *luar_trace_tsetkt (TValue *tv, TString *key, TValue *val,
                           lua_State *L);
TValue *luar_trace_tsetdynkb (TValue *tv, TValue *key, int b,
                                  lua_State *L);
TValue *luar_trace_tsetdynki (TValue *tv, TValue *key, int64_t v,
                                  lua_State *L);
TValue *luar_trace_tsetdynks (TValue *tv, TValue *key, TString *s,
                                  lua_State *L);
TValue *luar_trace_tsetdynkf (TValue *tv, TValue *key, double v,
                                 lua_State *L);
TValue *luar_trace_tsetdyn (TValue *tv, TValue *key, TValue *val,
                            lua_State *L);
int64_t luar_trace_tsetupki (int idx, TString *key, int64_t v,
                                lua_State *L);
int64_t luar_trace_tsetupks (int idx, TString *key, TString *v,
                                lua_State *L);
int64_t luar_trace_tsetupkb (int idx, TString *key, int b,
                                lua_State *L);
int64_t luar_trace_tsetupv (int idx, TString *key, TValue *val,
                                lua_State *L);
int64_t luar_trace_tsetupt (int idx, TString *key, TValue *val,
                                lua_State *L);
int64_t luar_trace_tsetupkn (int idx, TString *key, lua_State *L);
int64_t luar_trace_tsetupkf (int idx, TString *key, double v,
                                lua_State *L);
TValue *luar_trace_tsetkdb (TValue *tv, TString *key, TValue *val,
                            lua_State *L);
TValue *luar_trace_tsetkb (TValue *tv, TString *key, int b, lua_State *L);
int64_t luar_trace_tgetk (TValue *tv, TString *key, TValue *out);
int64_t luar_trace_tgetkf (TValue *tv, TString *key, TValue *out);
int64_t luar_trace_tgetkt (TValue *tv, TString *key, TValue *out);
int64_t luar_trace_tgetkb (TValue *tv, TString *key, TValue *out);
int64_t luar_trace_tgetks (TValue *tv, TString *key, TValue *out);
int64_t luar_trace_concat2 (TValue *base, int a, lua_State *L);
int64_t luar_trace_concat3 (TValue *base, int a, lua_State *L);
int64_t luar_trace_concatn (TValue *base, int a, int n, lua_State *L);
int64_t luar_trace_kbool (TValue *out, int b);
int64_t luar_trace_kstr (TValue *out, TString *s, lua_State *L);
int64_t luar_trace_strlen (TValue *v, TValue *out);
int64_t luar_trace_tlen (TValue *v, TValue *out, lua_State *L);
int luar_trace_ir_guards_match (const LuaR_TraceIR *t, TValue *base);
int luar_trace_ir_link (LuaR_TraceIR *t, int parent_depth, int root_start);
int luar_trace_record_loop (Proto *p, int start, int backedge, TValue *base,
                            lua_State *L, LuaR_TraceIR *t);
int luar_trace_ir_selftest (void);

#endif
