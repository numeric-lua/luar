/* Shared contract between the bytecode decoder (demo.c) and the
** DynASM-generated native code builder (demo.dasc -> demo_trace.c).
**
** This is a proof-of-concept only: the descriptor captures exactly the
** one loop shape the demo accepts, and the native function implements
** that shape with no guards.
*/
#ifndef TRACE_H
#define TRACE_H

#include <stddef.h>
#include <stdint.h>

enum {
  TRACE_ADD_REG = 1,  /* acc = acc + loop-variable register */
  TRACE_ADD_IMM = 2   /* acc = acc + immediate (OP_ADDI form)  */
};

typedef struct TraceDesc {
  int64_t init;        /* R[A]   before FORPREP                      */
  int64_t step;        /* R[A+2] before FORPREP                      */
  int64_t acc_init;    /* accumulator initial value                  */
  int64_t add_imm;     /* immediate operand for TRACE_ADD_IMM        */
  int op;              /* TRACE_ADD_REG or TRACE_ADD_IMM             */

  /* Lua register numbers decoded from the Proto.  The generated code
  ** has a fixed physical-register shape, but these select which decoded
  ** values are used and are validated before emission.
  */
  int acc_reg;         /* accumulator slot                           */
  int for_reg;         /* FORPREP/FORLOOP A                          */
  int init_reg;        /* for_reg + 0                                */
  int limit_reg;       /* for_reg + 1                                */
  int step_reg;        /* for_reg + 2 (also the visible loop var)    */
  int var_reg;         /* register read by the ADD body              */

  const char *limit_name;  /* global name supplying the dynamic limit */
} TraceDesc;

typedef int64_t (*trace_sum_fn)(int64_t n);

/* Assemble the native loop, returning executable memory of length
** *code_size, or NULL on failure.  Uses DynASM x64.
*/
trace_sum_fn trace_build(const TraceDesc *desc, size_t *code_size);

#endif
