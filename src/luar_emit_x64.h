/*
** This file has been pre-processed with DynASM.
** https://luajit.org/dynasm.html
** DynASM version 1.5.0, DynASM x64 version 1.5.0
** DO NOT EDIT! The original file is in "src/luar_emit_x64.dasc".
*/

#line 1 "src/luar_emit_x64.dasc"
/* DynASM x86-64 emitter for J2 numeric loops.
**
** Build (checked in as luar_emit_x64.h):
**   lua dynasm/dynasm.lua -D X64 src/luar_emit_x64.dasc \
**       > src/luar_emit_x64.h
**
** The generated function has the SysV shape int f(void *base), where base
** points at Lua stack register R0.  It runs the remaining iterations of a
** numeric for-loop directly on the 16-byte tagged TValue slots and returns
** 1 on success or 0 if a tag guard fails.
*/

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define DASM_CHECKS 1
#include "dasm_proto.h"
#include "dasm_x86.h"
#include "luar_emit.h"

//|.arch x64
#if DASM_VERSION != 10500
#error "Version mismatch between DynASM and included encoding engine"
#endif
#line 25 "src/luar_emit_x64.dasc"
//|.section code
#define DASM_SECTION_CODE	0
#define DASM_MAXSECTION		1
#line 26 "src/luar_emit_x64.dasc"
//|.actionlist luar_emit_actions
static const unsigned char luar_emit_actions[166] = {
  128,187,233,235,15,133,244,249,255,72,139,131,233,72,137,131,233,138,131,
  233,136,131,233,255,72,184,237,237,72,137,131,233,198,131,233,235,255,72,
  139,131,233,72,129,192,239,72,137,131,233,198,131,233,235,255,72,139,131,
  233,72,3,131,233,72,137,131,233,198,131,233,235,255,72,139,131,233,72,43,
  131,233,72,137,131,233,198,131,233,235,255,72,139,131,233,72,15,175,131,233,
  72,137,131,233,198,131,233,235,255,83,72,137,252,251,255,248,1,255,72,139,
  131,233,72,133,192,15,132,244,248,72,252,255,200,72,137,131,233,72,139,139,
  233,72,3,139,233,72,137,139,233,252,233,244,1,248,2,184,1,0,0,0,91,195,248,
  3,49,192,91,195,255
};

#line 27 "src/luar_emit_x64.dasc"

#define LUAR_SLOT(r)	(16 * (int)(r))
#define LUAR_TAG(r)	(16 * (int)(r) + 8)

static void emit_guards (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  unsigned char seen[256];
  int i;
  memset(seen, 0, sizeof(seen));
  for (i = 0; i < d->nops; i++) {
    const LuaR_Op *op = &d->ops[i];
    int regs[2];
    int n = 0, j;
    switch (op->kind) {
      case LUAR_OP_MOVE: regs[n++] = op->b; break;
      case LUAR_OP_ADDI: regs[n++] = op->b; break;
      case LUAR_OP_ADD:
      case LUAR_OP_SUB:
      case LUAR_OP_MUL: regs[n++] = op->b; regs[n++] = op->c; break;
      default: break;
    }
    for (j = 0; j < n; j++) {
      int r = regs[j];
      if (!seen[r]) {
        seen[r] = 1;
        //| cmp byte [rbx + ((int)LUAR_TAG(r))], ((int)d->tag_int)
        //| jne >3
        dasm_put(Dst, 0, ((int)LUAR_TAG(r)), ((int)d->tag_int));
#line 54 "src/luar_emit_x64.dasc"
      }
    }
  }
  for (i = 0; i < 3; i++) {
    int r = d->for_a + i;
    if (!seen[r]) {
      seen[r] = 1;
      //| cmp byte [rbx + ((int)LUAR_TAG(r))], ((int)d->tag_int)
      //| jne >3
      dasm_put(Dst, 0, ((int)LUAR_TAG(r)), ((int)d->tag_int));
#line 63 "src/luar_emit_x64.dasc"
    }
  }
}

static void emit_body (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  int i;
  for (i = 0; i < d->nops; i++) {
    const LuaR_Op *op = &d->ops[i];
    int a = op->a, b = op->b, c = op->c;
    switch (op->kind) {
      case LUAR_OP_MOVE:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
        //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
        //| mov al, byte [rbx + ((int)LUAR_TAG(b))]
        //| mov byte [rbx + ((int)LUAR_TAG(a))], al
        dasm_put(Dst, 9, ((int)LUAR_SLOT(b)), ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(b)), ((int)LUAR_TAG(a)));
#line 79 "src/luar_emit_x64.dasc"
        break;
      case LUAR_OP_LOADI:
        //| mov64 rax, ((int64_t)op->imm)
        //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
        dasm_put(Dst, 24, (unsigned int)(((int64_t)op->imm)), (unsigned int)((((int64_t)op->imm))>>32), ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 84 "src/luar_emit_x64.dasc"
        break;
      case LUAR_OP_ADDI:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
        //| add rax, ((int64_t)op->imm)
        //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
        dasm_put(Dst, 37, ((int)LUAR_SLOT(b)), ((int64_t)op->imm), ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 90 "src/luar_emit_x64.dasc"
        break;
      case LUAR_OP_ADD:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
        //| add rax, qword [rbx + ((int)LUAR_SLOT(c))]
        //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
        dasm_put(Dst, 54, ((int)LUAR_SLOT(b)), ((int)LUAR_SLOT(c)), ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 96 "src/luar_emit_x64.dasc"
        break;
      case LUAR_OP_SUB:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
        //| sub rax, qword [rbx + ((int)LUAR_SLOT(c))]
        //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
        dasm_put(Dst, 71, ((int)LUAR_SLOT(b)), ((int)LUAR_SLOT(c)), ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 102 "src/luar_emit_x64.dasc"
        break;
      case LUAR_OP_MUL:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
        //| imul rax, qword [rbx + ((int)LUAR_SLOT(c))]
        //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
        dasm_put(Dst, 88, ((int)LUAR_SLOT(b)), ((int)LUAR_SLOT(c)), ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 108 "src/luar_emit_x64.dasc"
        break;
      case LUAR_OP_MMBIN:
        break;  /* integer fast path proved by the guards */
      default:
        break;
    }
  }
}

static void emit_code (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  //| push rbx
  //| mov rbx, rdi
  dasm_put(Dst, 106);
#line 121 "src/luar_emit_x64.dasc"
  emit_guards(Dst, d);
  //|1:
  dasm_put(Dst, 112);
#line 123 "src/luar_emit_x64.dasc"
  emit_body(Dst, d);
  /* Exact FORLOOP semantics: while count > 0, decrement, advance the index
  ** and run the body again.  When count is already zero the body just ran
  ** for the final iteration and the index must not advance again. */
  //| mov rax, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| test rax, rax
  //| jz >2
  //| dec rax
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], rax
  //| mov rcx, qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))]
  //| add rcx, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], rcx
  //| jmp <1
  //|2:
  //| mov eax, 1
  //| pop rbx
  //| ret
  //|3:
  //| xor eax, eax
  //| pop rbx
  //| ret
  dasm_put(Dst, 115, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 144 "src/luar_emit_x64.dasc"
}

LuaR_LoopFn luar_emit_build (const LuaR_LoopDesc *d, size_t *code_size)
{
  dasm_State *Dst;
  size_t size = 0;
  void *mem;
  int status;
  if (d == NULL || code_size == NULL || d->ops == NULL || d->nops <= 0 ||
      d->for_a < 0 || d->for_a + 2 > 254)
    return NULL;
  dasm_init(&Dst, DASM_MAXSECTION);
  dasm_setupglobal(&Dst, NULL, 0);
  dasm_setup(&Dst, luar_emit_actions);
  emit_code(&Dst, d);
  if ((status = dasm_checkstep(&Dst, -1)) != 0) {
    dasm_free(&Dst);
    return NULL;
  }
  if ((status = dasm_link(&Dst, &size)) != 0) {
    dasm_free(&Dst);
    return NULL;
  }
  mem = mmap(NULL, size, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mem == MAP_FAILED) {
    dasm_free(&Dst);
    return NULL;
  }
  if ((status = dasm_encode(&Dst, mem)) != 0) {
    munmap(mem, size);
    dasm_free(&Dst);
    return NULL;
  }
  if (mprotect(mem, size, PROT_READ | PROT_EXEC) != 0) {
    munmap(mem, size);
    dasm_free(&Dst);
    return NULL;
  }
  dasm_free(&Dst);
  *code_size = size;
  return (LuaR_LoopFn)mem;
}
