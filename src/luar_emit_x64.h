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
#include <stdio.h>
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
#line 26 "src/luar_emit_x64.dasc"
//|.section code
#define DASM_SECTION_CODE	0
#define DASM_MAXSECTION		1
#line 27 "src/luar_emit_x64.dasc"
//|.actionlist luar_emit_actions
static const unsigned char luar_emit_actions[3463] = {
  128,187,233,235,15,133,244,249,255,72,139,131,233,252,242,72,15,42,192,255,
  252,242,15,16,131,233,255,252,242,15,17,131,233,198,131,233,235,255,72,139,
  131,233,72,137,131,233,138,131,233,136,131,233,255,72,184,237,237,72,137,
  131,233,198,131,233,235,255,72,129,131,233,239,255,72,139,131,233,72,129,
  192,239,72,137,131,233,198,131,233,235,255,72,184,237,237,102,72,15,110,192,
  255,252,242,15,88,131,233,252,242,15,17,131,233,255,252,242,15,89,131,233,
  252,242,15,17,131,233,255,252,242,15,16,139,233,255,252,242,15,92,200,255,
  252,242,15,94,200,255,252,242,15,17,139,233,255,72,184,237,237,102,72,15,
  110,200,255,252,242,15,88,193,255,252,242,15,92,193,255,252,242,15,89,193,
  255,252,242,15,94,193,255,72,139,131,233,255,72,139,131,233,252,242,72,15,
  42,200,255,72,139,131,233,72,1,131,233,255,72,139,131,233,72,41,131,233,255,
  72,139,131,233,72,15,175,131,233,72,137,131,233,255,72,3,131,233,255,72,43,
  131,233,255,72,15,175,131,233,255,72,185,237,237,72,57,200,255,72,59,131,
  233,255,15,140,245,255,15,141,245,255,15,142,245,255,15,143,245,255,15,132,
  245,255,15,133,245,255,65,187,237,72,139,131,233,72,185,237,237,72,57,200,
  15,143,244,249,72,185,237,237,72,57,200,15,140,244,249,255,65,187,237,252,
  233,244,249,255,72,184,237,237,252,242,72,15,42,200,255,102,15,46,193,255,
  15,138,245,255,15,130,245,255,15,131,245,255,15,134,245,255,15,135,245,255,
  72,139,131,233,72,131,252,248,1,15,140,244,249,76,57,252,240,15,135,244,249,
  72,252,255,200,72,193,224,4,76,1,232,128,184,233,235,15,133,244,249,72,139,
  8,72,137,139,233,198,131,233,235,255,72,139,131,233,72,131,252,248,1,15,140,
  244,249,76,57,252,240,15,135,244,249,72,252,255,200,72,193,224,4,76,1,232,
  128,184,233,235,15,133,244,249,72,139,8,255,72,139,131,233,72,1,200,72,137,
  131,233,198,131,233,235,255,252,242,72,15,42,193,252,242,15,88,131,233,252,
  242,15,17,131,233,198,131,233,235,255,72,139,131,233,72,131,252,248,1,15,
  140,244,249,76,57,252,240,15,135,244,249,72,252,255,200,72,193,224,4,76,1,
  232,255,72,139,139,233,72,137,8,198,128,233,235,255,252,242,15,16,131,233,
  252,242,15,17,0,198,128,233,235,255,83,72,137,252,251,255,65,84,65,85,65,
  86,255,65,187,237,255,76,139,163,233,73,139,132,253,36,233,72,133,192,15,
  133,244,249,77,139,172,253,36,233,69,139,180,253,36,233,255,252,233,245,255,
  249,184,1,0,0,0,255,65,94,65,93,65,92,255,91,195,255,249,72,139,131,233,72,
  133,192,15,132,244,248,72,252,255,200,72,137,131,233,72,139,139,233,72,3,
  139,233,72,137,139,233,252,233,245,249,248,2,184,1,0,0,0,255,248,3,68,137,
  216,131,192,2,252,247,216,255,83,72,137,252,251,65,84,65,85,65,86,255,65,
  87,255,76,139,163,233,76,139,171,233,76,139,179,233,255,252,242,15,16,131,
  233,72,184,237,237,102,72,15,110,200,255,76,139,187,233,73,187,237,237,255,
  248,1,255,252,242,73,15,42,213,252,242,15,89,209,252,242,15,88,194,255,76,
  137,252,233,73,15,175,203,73,1,207,255,77,133,228,15,132,244,248,73,252,255,
  204,77,1,252,245,252,233,244,1,248,2,76,137,163,233,76,137,171,233,255,76,
  137,187,233,65,95,255,184,1,0,0,0,65,94,65,93,65,92,91,195,248,3,255,65,95,
  49,192,65,94,65,93,65,92,91,195,255,83,72,137,252,251,65,84,65,85,65,86,65,
  87,76,139,163,233,76,139,171,233,76,139,179,233,255,76,139,187,233,73,139,
  135,233,72,133,192,15,133,244,249,77,139,151,233,69,139,159,233,248,1,73,
  131,252,253,1,15,140,244,249,77,57,221,15,135,244,249,76,137,232,72,252,255,
  200,72,193,224,4,76,1,208,76,137,40,198,128,233,235,77,133,228,15,132,244,
  248,73,252,255,204,77,1,252,245,252,233,244,1,248,2,76,137,163,233,255,76,
  137,171,233,184,1,0,0,0,65,95,65,94,65,93,65,92,91,195,248,3,76,137,163,233,
  76,137,171,233,49,192,65,95,65,94,65,93,65,92,91,195,255,76,139,187,233,255,
  252,242,73,15,42,205,252,242,15,88,193,255,77,1,252,239,255,76,137,187,233,
  255,83,72,137,252,251,65,84,65,85,65,86,65,87,76,139,163,233,76,139,171,233,
  76,139,179,233,76,139,187,233,248,1,76,137,232,255,73,1,199,77,133,228,15,
  132,244,248,73,252,255,204,77,1,252,245,252,233,244,1,248,2,76,137,163,233,
  76,137,171,233,76,137,187,233,184,1,0,0,0,65,95,65,94,65,93,65,92,91,195,
  248,3,49,192,65,95,65,94,65,93,65,92,91,195,255,83,72,137,252,251,65,84,65,
  85,65,86,65,87,76,139,163,233,76,139,171,233,76,139,179,233,76,139,187,233,
  248,1,77,1,252,239,77,133,228,15,132,244,248,73,252,255,204,77,1,252,245,
  252,233,244,1,248,2,76,137,163,233,76,137,171,233,76,137,187,233,184,1,0,
  0,0,65,95,65,94,65,93,65,92,91,195,248,3,49,192,65,95,65,94,65,93,65,92,91,
  195,255,76,139,131,233,73,139,128,233,72,133,192,15,133,244,249,77,139,136,
  233,69,139,144,233,255,73,131,252,254,1,15,133,244,249,73,131,252,253,1,15,
  140,244,249,76,137,232,76,1,224,76,57,208,15,135,244,249,76,137,252,233,72,
  252,255,201,72,193,225,4,76,1,201,76,137,226,72,252,255,194,248,4,128,185,
  233,235,15,133,244,249,72,131,193,16,72,252,255,202,15,133,244,4,77,137,252,
  235,73,252,255,203,73,193,227,4,77,1,203,248,1,255,77,3,59,255,252,242,73,
  15,42,11,252,242,15,88,193,255,73,131,195,16,77,133,228,15,132,244,248,73,
  252,255,204,73,129,197,239,252,233,244,1,248,2,76,137,163,233,76,137,171,
  233,255,184,1,0,0,0,65,95,65,94,65,93,65,92,91,195,248,3,76,137,163,233,76,
  137,171,233,255,252,242,15,16,131,233,72,184,237,237,102,72,15,110,208,255,
  76,139,163,233,255,76,139,171,233,255,73,190,237,237,255,248,1,77,57,252,
  245,255,15,140,244,248,255,15,141,244,248,255,15,142,244,248,255,15,143,244,
  248,255,252,242,73,15,42,205,255,252,242,15,89,201,252,242,15,88,193,255,
  252,242,15,89,202,252,242,15,88,193,255,77,15,175,229,255,77,41,252,236,255,
  76,137,252,233,73,15,175,205,73,1,204,255,77,1,252,236,255,73,129,197,239,
  252,233,244,1,248,2,255,76,137,171,233,184,1,0,0,0,65,94,65,93,65,92,91,195,
  248,3,49,192,65,94,65,93,65,92,91,195,255,252,242,73,15,42,205,252,242,15,
  89,201,252,242,15,88,193,255,76,137,252,233,73,15,175,205,73,1,207,255,77,
  15,175,252,253,255,252,242,73,15,42,205,252,242,15,92,193,255,77,41,252,239,
  255,83,72,137,252,251,65,85,65,86,65,87,76,139,171,233,255,248,1,73,129,197,
  239,255,77,57,252,238,255,15,140,244,1,255,15,141,244,1,255,15,142,244,1,
  255,15,143,244,1,255,15,132,244,1,255,15,133,244,1,255,184,1,0,0,0,65,95,
  65,94,65,93,91,195,248,3,248,2,49,192,65,95,65,94,65,93,91,195,255,184,237,
  91,195,248,3,49,192,91,195,255,72,139,131,233,72,133,192,15,132,244,248,72,
  252,255,200,72,137,131,233,72,139,139,233,72,3,139,233,72,137,139,233,252,
  233,244,1,248,2,184,1,0,0,0,91,195,248,3,49,192,91,195,255,65,84,65,87,83,
  72,131,252,236,16,72,137,252,251,73,137,252,244,65,191,0,1,0,0,65,187,237,
  248,1,255,65,187,237,128,187,233,235,15,133,244,248,255,72,141,187,233,190,
  237,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,72,141,
  187,233,72,190,237,237,76,137,226,72,184,237,237,252,255,208,65,187,237,133,
  192,15,132,244,248,255,76,137,231,72,141,179,233,72,186,237,237,185,237,72,
  184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,72,141,187,
  233,72,141,179,233,72,141,147,233,72,184,237,237,252,255,208,65,187,237,133,
  192,15,132,244,248,255,72,141,187,233,72,141,179,233,72,184,237,237,252,255,
  208,65,187,237,133,192,15,132,244,248,255,72,141,187,233,72,139,179,233,72,
  141,147,233,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,
  255,72,141,187,233,252,242,15,16,131,233,72,141,179,233,72,184,237,237,252,
  255,208,65,187,237,133,192,15,132,244,248,255,76,137,231,72,141,179,233,186,
  237,185,237,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,
  255,72,141,187,233,72,141,179,233,76,137,226,72,184,237,237,252,255,208,65,
  187,237,133,192,15,132,244,248,255,72,137,223,190,237,186,237,76,137,225,
  72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,72,137,223,
  190,237,76,137,226,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,
  248,255,72,141,187,233,72,190,237,237,72,141,147,233,72,184,237,237,252,255,
  208,65,187,237,133,192,15,132,244,248,255,72,141,187,233,72,141,179,233,186,
  237,76,137,225,72,184,237,237,252,255,208,65,187,237,72,133,192,15,132,244,
  248,72,137,195,255,72,141,187,233,72,141,179,233,72,186,237,237,76,137,225,
  72,184,237,237,252,255,208,65,187,237,72,133,192,15,132,244,248,72,137,195,
  255,72,141,187,233,72,141,179,233,72,184,237,237,102,72,15,110,192,76,137,
  226,72,184,237,237,252,255,208,65,187,237,72,133,192,15,132,244,248,72,137,
  195,255,72,141,187,233,72,141,179,233,72,141,147,233,76,137,225,72,184,237,
  237,252,255,208,65,187,237,72,133,192,15,132,244,248,72,137,195,255,191,237,
  72,190,237,237,76,137,226,72,184,237,237,252,255,208,65,187,237,133,192,15,
  132,244,248,255,191,237,72,190,237,237,72,141,147,233,76,137,225,72,184,237,
  237,252,255,208,65,187,237,133,192,15,132,244,248,255,191,237,72,190,237,
  237,186,237,76,137,225,72,184,237,237,252,255,208,65,187,237,133,192,15,132,
  244,248,255,191,237,72,190,237,237,72,186,237,237,76,137,225,72,184,237,237,
  252,255,208,65,187,237,133,192,15,132,244,248,255,191,237,72,190,237,237,
  72,184,237,237,102,72,15,110,192,76,137,226,72,184,237,237,252,255,208,65,
  187,237,133,192,15,132,244,248,255,72,141,187,233,72,190,237,237,72,141,147,
  233,76,137,225,72,184,237,237,252,255,208,65,187,237,72,133,192,15,132,244,
  248,72,137,195,255,72,141,187,233,72,190,237,237,186,237,76,137,225,72,184,
  237,237,252,255,208,65,187,237,72,133,192,15,132,244,248,72,137,195,255,72,
  141,187,233,72,190,237,237,72,186,237,237,76,137,225,72,184,237,237,252,255,
  208,65,187,237,72,133,192,15,132,244,248,72,137,195,255,72,141,187,233,72,
  199,198,237,72,141,147,233,72,184,237,237,252,255,208,65,187,237,133,192,
  15,132,244,248,255,72,141,187,233,72,199,198,237,72,141,147,233,76,137,225,
  72,184,237,237,252,255,208,65,187,237,72,133,192,15,132,244,248,72,137,195,
  255,72,141,187,233,139,179,233,139,147,233,72,141,139,233,72,184,237,237,
  252,255,208,65,187,237,133,192,15,132,244,248,255,72,141,187,233,72,139,179,
  233,252,242,15,16,131,233,72,141,147,233,72,184,237,237,252,255,208,65,187,
  237,133,192,15,132,244,248,255,72,141,187,233,252,243,15,126,131,233,72,139,
  179,233,72,141,147,233,72,184,237,237,252,255,208,65,187,237,133,192,15,132,
  244,248,255,72,141,187,233,252,242,15,16,131,233,252,242,15,16,139,233,72,
  141,179,233,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,
  255,72,141,187,233,252,242,15,16,131,233,72,139,179,233,72,141,147,233,72,
  184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,72,141,187,
  233,252,243,15,126,131,233,252,243,15,126,139,233,72,141,179,233,72,184,237,
  237,252,255,208,65,187,237,133,192,15,132,244,248,255,72,141,187,233,72,139,
  179,233,252,242,15,16,131,233,252,242,15,16,139,233,72,141,147,233,72,184,
  237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,72,141,187,233,
  252,242,15,16,131,233,72,139,179,233,252,242,15,16,139,233,72,141,147,233,
  72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,72,141,187,
  233,252,242,15,16,131,233,252,242,15,16,139,233,72,139,179,233,72,141,147,
  233,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,72,141,
  187,233,252,242,15,16,131,233,252,242,15,16,139,233,252,242,15,16,147,233,
  72,141,179,233,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,
  255,72,141,187,233,72,139,179,233,72,141,147,233,72,184,237,237,252,255,208,
  65,187,237,133,192,15,132,244,248,198,131,233,235,255,72,141,187,233,72,199,
  198,237,72,141,147,233,72,141,139,233,72,184,237,237,252,255,208,65,187,237,
  133,192,15,132,244,248,255,72,141,187,233,72,199,198,237,72,199,194,237,255,
  72,199,193,237,255,72,141,187,233,72,199,198,237,72,199,194,237,72,141,139,
  233,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,72,141,
  187,233,72,141,179,233,72,186,237,237,72,184,237,237,252,255,208,65,187,237,
  133,192,15,132,244,248,255,72,141,187,233,72,139,179,233,72,141,147,233,255,
  72,184,237,237,255,72,141,187,233,72,139,179,233,72,141,147,233,76,137,225,
  72,184,237,237,252,255,208,65,187,237,72,133,192,15,132,244,248,72,137,195,
  255,72,129,192,239,255,72,129,232,239,255,72,105,192,239,255,65,187,237,72,
  139,131,233,72,185,237,237,72,57,200,15,133,244,254,255,72,141,187,233,72,
  141,179,233,72,184,237,237,102,72,15,110,192,72,184,237,237,252,255,208,65,
  187,237,133,192,15,132,244,248,255,65,187,237,252,233,244,254,255,72,139,
  131,233,72,133,192,15,132,244,251,72,252,255,200,72,137,131,233,72,139,139,
  233,72,3,139,233,72,137,139,233,49,192,72,131,196,16,91,65,95,65,92,195,248,
  5,184,1,0,0,0,72,131,196,16,91,65,95,65,92,195,255,72,139,131,233,72,133,
  192,15,132,244,249,72,252,255,200,72,137,131,233,72,139,139,233,72,3,139,
  233,72,137,139,233,65,252,255,207,15,132,244,252,252,233,244,1,255,248,3,
  184,1,0,0,0,72,131,196,16,91,65,95,65,92,195,248,2,255,68,137,216,252,247,
  216,131,232,2,255,72,131,196,16,91,65,95,65,92,195,248,6,184,2,0,0,0,72,131,
  196,16,91,65,95,65,92,195,248,8,68,137,216,252,247,216,131,232,2,72,131,196,
  16,91,65,95,65,92,195,255,72,185,237,237,72,1,200,255,72,185,237,237,72,41,
  200,255,72,185,237,237,72,15,175,193,255
};

#line 28 "src/luar_emit_x64.dasc"

#define LUAR_SLOT(r)	(16 * (int)(r))
#define LUAR_TAG(r)	(16 * (int)(r) + 8)

static void emit_guards (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  int i;
  for (i = 0; i < d->nguards; i++) {
    int r = d->guard_reg[i];
    int tag = (d->guard_type[i] == LUAR_T_FLOAT) ? d->tag_flt :
              (d->guard_type[i] == LUAR_T_TABLE) ? d->tag_table :
              d->tag_int;
    //| cmp byte [rbx + ((int)LUAR_TAG(r))], ((int)tag)
    //| jne >3
    dasm_put(Dst, 0, ((int)LUAR_TAG(r)), ((int)tag));
#line 42 "src/luar_emit_x64.dasc"
  }
}

static void load_double (dasm_State **Dst, int reg, int type)
{
  if (type == LUAR_T_INT) {
    //| mov rax, qword [rbx + ((int)LUAR_SLOT(reg))]
    //| cvtsi2sd xmm0, rax
    dasm_put(Dst, 9, ((int)LUAR_SLOT(reg)));
#line 50 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(reg))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(reg)));
#line 53 "src/luar_emit_x64.dasc"
  }
}

static void store_double (dasm_State **Dst, const LuaR_LoopDesc *d, int reg)
{
  //| movsd qword [rbx + ((int)LUAR_SLOT(reg))], xmm0
  //| mov byte [rbx + ((int)LUAR_TAG(reg))], ((int)d->tag_flt)
  dasm_put(Dst, 27, ((int)LUAR_SLOT(reg)), ((int)LUAR_TAG(reg)), ((int)d->tag_flt));
#line 60 "src/luar_emit_x64.dasc"
}

/* Full-width immediate helpers (defined at the end of this file). */
static void emit_add_imm (dasm_State **Dst, int64_t imm);
static void emit_sub_imm (dasm_State **Dst, int64_t imm);
static void emit_mul_imm (dasm_State **Dst, int64_t imm);

static void emit_body (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  int i;
  for (i = 0; i < d->nops; i++) {
    const LuaR_Op *op = &d->ops[i];
    int a = op->a, b = op->b;
    switch (op->kind) {
      case LUAR_OP_MOVE:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
        //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
        //| mov al, byte [rbx + ((int)LUAR_TAG(b))]
        //| mov byte [rbx + ((int)LUAR_TAG(a))], al
        dasm_put(Dst, 38, ((int)LUAR_SLOT(b)), ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(b)), ((int)LUAR_TAG(a)));
#line 79 "src/luar_emit_x64.dasc"
        break;
      case LUAR_OP_LOADI:
        //| mov64 rax, ((int64_t)op->imm)
        //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
        dasm_put(Dst, 53, (unsigned int)(((int64_t)op->imm)), (unsigned int)((((int64_t)op->imm))>>32), ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 84 "src/luar_emit_x64.dasc"
        break;
      case LUAR_OP_LOADF:
      case LUAR_OP_LOADK: {
        int64_t bits;
        if (op->kind == LUAR_OP_LOADF || op->ta == LUAR_T_FLOAT) {
          memcpy(&bits, &op->num, sizeof(bits));
          //| mov64 rax, bits
          //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
          //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_flt)
          dasm_put(Dst, 53, (unsigned int)(bits), (unsigned int)((bits)>>32), ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_flt));
#line 93 "src/luar_emit_x64.dasc"
        }
        else {
          //| mov64 rax, op->imm
          //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
          //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
          dasm_put(Dst, 53, (unsigned int)(op->imm), (unsigned int)((op->imm)>>32), ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 98 "src/luar_emit_x64.dasc"
        }
        break;
      }
      case LUAR_OP_ADDI:
        if (a == b) {
          //| add qword [rbx + ((int)LUAR_SLOT(a))], ((int64_t)op->imm)
          dasm_put(Dst, 66, ((int)LUAR_SLOT(a)), ((int64_t)op->imm));
#line 104 "src/luar_emit_x64.dasc"
        }
        else {
          //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
          //| add rax, ((int64_t)op->imm)
          //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
          //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
          dasm_put(Dst, 72, ((int)LUAR_SLOT(b)), ((int64_t)op->imm), ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 110 "src/luar_emit_x64.dasc"
        }
        break;
      case LUAR_OP_ADDK:
      case LUAR_OP_SUBK:
      case LUAR_OP_MULK:
      case LUAR_OP_DIVK: {
        int64_t bits;
        int isdiv = op->kind == LUAR_OP_DIVK;
        memcpy(&bits, &op->num, sizeof(bits));
        if (a == b && op->tb == LUAR_T_FLOAT &&
            (isdiv || op->ta == LUAR_T_FLOAT)) {
          //| mov64 rax, bits
          //| movq xmm0, rax
          dasm_put(Dst, 89, (unsigned int)(bits), (unsigned int)((bits)>>32));
#line 123 "src/luar_emit_x64.dasc"
          if (op->kind == LUAR_OP_ADDK) {
            //| addsd xmm0, qword [rbx + ((int)LUAR_SLOT(a))]
            //| movsd qword [rbx + ((int)LUAR_SLOT(a))], xmm0
            dasm_put(Dst, 99, ((int)LUAR_SLOT(a)), ((int)LUAR_SLOT(a)));
#line 126 "src/luar_emit_x64.dasc"
          }
          else if (op->kind == LUAR_OP_MULK) {
            //| mulsd xmm0, qword [rbx + ((int)LUAR_SLOT(a))]
            //| movsd qword [rbx + ((int)LUAR_SLOT(a))], xmm0
            dasm_put(Dst, 112, ((int)LUAR_SLOT(a)), ((int)LUAR_SLOT(a)));
#line 130 "src/luar_emit_x64.dasc"
          }
          else {
            //| movsd xmm1, qword [rbx + ((int)LUAR_SLOT(a))]
            dasm_put(Dst, 125, ((int)LUAR_SLOT(a)));
#line 133 "src/luar_emit_x64.dasc"
            if (op->kind == LUAR_OP_SUBK) {
              //| subsd xmm1, xmm0
              dasm_put(Dst, 132);
#line 135 "src/luar_emit_x64.dasc"
            }
            else {
              //| divsd xmm1, xmm0
              dasm_put(Dst, 138);
#line 138 "src/luar_emit_x64.dasc"
            }
            //| movsd qword [rbx + ((int)LUAR_SLOT(a))], xmm1
            dasm_put(Dst, 144, ((int)LUAR_SLOT(a)));
#line 140 "src/luar_emit_x64.dasc"
          }
          break;
        }
        load_double(Dst, b, op->tb);
        if (isdiv || op->ta == LUAR_T_FLOAT) {
          //| mov64 rax, bits
          //| movq xmm1, rax
          dasm_put(Dst, 151, (unsigned int)(bits), (unsigned int)((bits)>>32));
#line 147 "src/luar_emit_x64.dasc"
          if (op->kind == LUAR_OP_ADDK) {
            //| addsd xmm0, xmm1
            dasm_put(Dst, 161);
#line 149 "src/luar_emit_x64.dasc"
          }
          if (op->kind == LUAR_OP_SUBK) {
            //| subsd xmm0, xmm1
            dasm_put(Dst, 167);
#line 152 "src/luar_emit_x64.dasc"
          }
          if (op->kind == LUAR_OP_MULK) {
            //| mulsd xmm0, xmm1
            dasm_put(Dst, 173);
#line 155 "src/luar_emit_x64.dasc"
          }
          if (isdiv) {
            //| divsd xmm0, xmm1
            dasm_put(Dst, 179);
#line 158 "src/luar_emit_x64.dasc"
          }
          store_double(Dst, d, a);
        }
        else {
          //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
          dasm_put(Dst, 185, ((int)LUAR_SLOT(b)));
#line 163 "src/luar_emit_x64.dasc"
          /* integer constant: op->imm holds the value */
          if (op->kind == LUAR_OP_ADDK) {
            emit_add_imm(Dst, op->imm);
          }
          if (op->kind == LUAR_OP_SUBK) {
            emit_sub_imm(Dst, op->imm);
          }
          if (op->kind == LUAR_OP_MULK) {
            emit_mul_imm(Dst, op->imm);
          }
          //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
          //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
          dasm_put(Dst, 57, ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 175 "src/luar_emit_x64.dasc"
        }
        break;
      }
      case LUAR_OP_ADD:
      case LUAR_OP_SUB:
      case LUAR_OP_MUL:
      case LUAR_OP_DIV: {
        int isdiv = op->kind == LUAR_OP_DIV;
        int flt = isdiv || op->ta == LUAR_T_FLOAT || op->tb == LUAR_T_FLOAT;
        if (flt && a == b && op->tb == LUAR_T_FLOAT) {
          /* In-place float accumulator: tag stays float. */
          load_double(Dst, op->c, op->tc);
          if (op->kind == LUAR_OP_ADD) {
            //| addsd xmm0, qword [rbx + ((int)LUAR_SLOT(a))]
            //| movsd qword [rbx + ((int)LUAR_SLOT(a))], xmm0
            dasm_put(Dst, 99, ((int)LUAR_SLOT(a)), ((int)LUAR_SLOT(a)));
#line 190 "src/luar_emit_x64.dasc"
          }
          else if (op->kind == LUAR_OP_MUL) {
            //| mulsd xmm0, qword [rbx + ((int)LUAR_SLOT(a))]
            //| movsd qword [rbx + ((int)LUAR_SLOT(a))], xmm0
            dasm_put(Dst, 112, ((int)LUAR_SLOT(a)), ((int)LUAR_SLOT(a)));
#line 194 "src/luar_emit_x64.dasc"
          }
          else {
            //| movsd xmm1, qword [rbx + ((int)LUAR_SLOT(a))]
            dasm_put(Dst, 125, ((int)LUAR_SLOT(a)));
#line 197 "src/luar_emit_x64.dasc"
            if (op->kind == LUAR_OP_SUB) {
              //| subsd xmm1, xmm0
              dasm_put(Dst, 132);
#line 199 "src/luar_emit_x64.dasc"
            }
            else {
              //| divsd xmm1, xmm0
              dasm_put(Dst, 138);
#line 202 "src/luar_emit_x64.dasc"
            }
            //| movsd qword [rbx + ((int)LUAR_SLOT(a))], xmm1
            dasm_put(Dst, 144, ((int)LUAR_SLOT(a)));
#line 204 "src/luar_emit_x64.dasc"
          }
        }
        else if (flt) {
          load_double(Dst, b, op->tb);
          if (op->tc == LUAR_T_INT) {
            //| mov rax, qword [rbx + ((int)LUAR_SLOT(op->c))]
            //| cvtsi2sd xmm1, rax
            dasm_put(Dst, 190, ((int)LUAR_SLOT(op->c)));
#line 211 "src/luar_emit_x64.dasc"
          }
          else {
            //| movsd xmm1, qword [rbx + ((int)LUAR_SLOT(op->c))]
            dasm_put(Dst, 125, ((int)LUAR_SLOT(op->c)));
#line 214 "src/luar_emit_x64.dasc"
          }
          if (op->kind == LUAR_OP_ADD) {
            //| addsd xmm0, xmm1
            dasm_put(Dst, 161);
#line 217 "src/luar_emit_x64.dasc"
          }
          if (op->kind == LUAR_OP_SUB) {
            //| subsd xmm0, xmm1
            dasm_put(Dst, 167);
#line 220 "src/luar_emit_x64.dasc"
          }
          if (op->kind == LUAR_OP_MUL) {
            //| mulsd xmm0, xmm1
            dasm_put(Dst, 173);
#line 223 "src/luar_emit_x64.dasc"
          }
          if (isdiv) {
            //| divsd xmm0, xmm1
            dasm_put(Dst, 179);
#line 226 "src/luar_emit_x64.dasc"
          }
          store_double(Dst, d, a);
        }
        else if (a == b) {
          /* In-place integer accumulator: the tag is already correct. */
          if (op->kind == LUAR_OP_ADD) {
            //| mov rax, qword [rbx + ((int)LUAR_SLOT(op->c))]
            //| add qword [rbx + ((int)LUAR_SLOT(a))], rax
            dasm_put(Dst, 201, ((int)LUAR_SLOT(op->c)), ((int)LUAR_SLOT(a)));
#line 234 "src/luar_emit_x64.dasc"
          }
          else if (op->kind == LUAR_OP_SUB) {
            //| mov rax, qword [rbx + ((int)LUAR_SLOT(op->c))]
            //| sub qword [rbx + ((int)LUAR_SLOT(a))], rax
            dasm_put(Dst, 210, ((int)LUAR_SLOT(op->c)), ((int)LUAR_SLOT(a)));
#line 238 "src/luar_emit_x64.dasc"
          }
          else {
            //| mov rax, qword [rbx + ((int)LUAR_SLOT(op->c))]
            //| imul rax, qword [rbx + ((int)LUAR_SLOT(a))]
            //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
            dasm_put(Dst, 219, ((int)LUAR_SLOT(op->c)), ((int)LUAR_SLOT(a)), ((int)LUAR_SLOT(a)));
#line 243 "src/luar_emit_x64.dasc"
          }
        }
        else {
          //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
          dasm_put(Dst, 185, ((int)LUAR_SLOT(b)));
#line 247 "src/luar_emit_x64.dasc"
          if (op->kind == LUAR_OP_ADD) {
            //| add rax, qword [rbx + ((int)LUAR_SLOT(op->c))]
            dasm_put(Dst, 233, ((int)LUAR_SLOT(op->c)));
#line 249 "src/luar_emit_x64.dasc"
          }
          if (op->kind == LUAR_OP_SUB) {
            //| sub rax, qword [rbx + ((int)LUAR_SLOT(op->c))]
            dasm_put(Dst, 238, ((int)LUAR_SLOT(op->c)));
#line 252 "src/luar_emit_x64.dasc"
          }
          if (op->kind == LUAR_OP_MUL) {
            //| imul rax, qword [rbx + ((int)LUAR_SLOT(op->c))]
            dasm_put(Dst, 243, ((int)LUAR_SLOT(op->c)));
#line 255 "src/luar_emit_x64.dasc"
          }
          //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
          //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
          dasm_put(Dst, 57, ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 258 "src/luar_emit_x64.dasc"
        }
        break;
      }
      case LUAR_OP_MMBIN:
        break;  /* numeric fast path proved by the guards */
      default:
        break;
    }
  }
}

static void emit_int_one (dasm_State **Dst, const LuaR_LoopDesc *d,
                          const LuaR_Op *op)
{
  LuaR_LoopDesc one = *d;
  one.ops = op;
  one.nops = 1;
  emit_body(Dst, &one);
}

static void emit_cmp (dasm_State **Dst, const LuaR_LoopDesc *d,
                      const LuaR_Op *op, int fallthrough)
{
  int a = op->a, b = op->b;
  int k = op->k;
  int both_int = (op->ta == LUAR_T_INT && op->tb == LUAR_T_INT);
  (void)d;
  if (both_int) {
    //| mov rax, qword [rbx + ((int)LUAR_SLOT(a))]
    dasm_put(Dst, 185, ((int)LUAR_SLOT(a)));
#line 287 "src/luar_emit_x64.dasc"
    if (b == 0xFF) {
      //| mov64 rcx, op->imm
      //| cmp rax, rcx
      dasm_put(Dst, 249, (unsigned int)(op->imm), (unsigned int)((op->imm)>>32));
#line 290 "src/luar_emit_x64.dasc"
    }
    else {
      //| cmp rax, qword [rbx + ((int)LUAR_SLOT(b))]
      dasm_put(Dst, 257, ((int)LUAR_SLOT(b)));
#line 293 "src/luar_emit_x64.dasc"
    }
    switch (op->kind) {
      case LUAR_OP_LT:
        if (k) {
          //| jl =>((int)op->target)
          dasm_put(Dst, 262, ((int)op->target));
#line 298 "src/luar_emit_x64.dasc"
        }
        else {
          //| jge =>((int)op->target)
          dasm_put(Dst, 266, ((int)op->target));
#line 301 "src/luar_emit_x64.dasc"
        }
        break;
      case LUAR_OP_LE:
        if (k) {
          //| jle =>((int)op->target)
          dasm_put(Dst, 270, ((int)op->target));
#line 306 "src/luar_emit_x64.dasc"
        }
        else {
          //| jg =>((int)op->target)
          dasm_put(Dst, 274, ((int)op->target));
#line 309 "src/luar_emit_x64.dasc"
        }
        break;
      case LUAR_OP_GT:
        if (k) {
          //| jg =>((int)op->target)
          dasm_put(Dst, 274, ((int)op->target));
#line 314 "src/luar_emit_x64.dasc"
        }
        else {
          //| jle =>((int)op->target)
          dasm_put(Dst, 270, ((int)op->target));
#line 317 "src/luar_emit_x64.dasc"
        }
        break;
      case LUAR_OP_GE:
        if (k) {
          //| jge =>((int)op->target)
          dasm_put(Dst, 266, ((int)op->target));
#line 322 "src/luar_emit_x64.dasc"
        }
        else {
          //| jl =>((int)op->target)
          dasm_put(Dst, 262, ((int)op->target));
#line 325 "src/luar_emit_x64.dasc"
        }
        break;
      case LUAR_OP_EQ:
        if (k) {
          //| je =>((int)op->target)
          dasm_put(Dst, 278, ((int)op->target));
#line 330 "src/luar_emit_x64.dasc"
        }
        else {
          //| jne =>((int)op->target)
          dasm_put(Dst, 282, ((int)op->target));
#line 333 "src/luar_emit_x64.dasc"
        }
        break;
      default: break;
    }
    return;
  }

  /* Mixed integer/float comparisons are exact in Lua: a large integer
  ** may not be representable as a double.  If either integer operand is
  ** outside [-2^53, 2^53], exit to the interpreter at this instruction
  ** instead of comparing rounded doubles.  Earlier body operations are
  ** already applied to the stack slots, so resuming here is exact. */
  if (op->ta == LUAR_T_INT) {
    //| mov r11d, ((int)op->pc)
    //| mov rax, qword [rbx + ((int)LUAR_SLOT(a))]
    //| mov64 rcx, 9007199254740992
    //| cmp rax, rcx
    //| jg >3
    //| mov64 rcx, -9007199254740992
    //| cmp rax, rcx
    //| jl >3
    dasm_put(Dst, 286, ((int)op->pc), ((int)LUAR_SLOT(a)), (unsigned int)(9007199254740992), (unsigned int)((9007199254740992)>>32), (unsigned int)(-9007199254740992), (unsigned int)((-9007199254740992)>>32));
#line 354 "src/luar_emit_x64.dasc"
  }
  if (op->tb == LUAR_T_INT && b == 0xFF) {
    if (op->imm > 9007199254740992LL ||
        op->imm < -9007199254740992LL) {
      //| mov r11d, ((int)op->pc)
      //| jmp >3
      dasm_put(Dst, 316, ((int)op->pc));
#line 360 "src/luar_emit_x64.dasc"
    }
  }
  else if (op->tb == LUAR_T_INT) {
    //| mov r11d, ((int)op->pc)
    //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
    //| mov64 rcx, 9007199254740992
    //| cmp rax, rcx
    //| jg >3
    //| mov64 rcx, -9007199254740992
    //| cmp rax, rcx
    //| jl >3
    dasm_put(Dst, 286, ((int)op->pc), ((int)LUAR_SLOT(b)), (unsigned int)(9007199254740992), (unsigned int)((9007199254740992)>>32), (unsigned int)(-9007199254740992), (unsigned int)((-9007199254740992)>>32));
#line 371 "src/luar_emit_x64.dasc"
  }

  /* At least one operand is a double: convert both and use ucomisd. */
  load_double(Dst, a, op->ta);
  if (b == 0xFF) {
    if (op->tb == LUAR_T_INT) {
      //| mov64 rax, op->imm
      //| cvtsi2sd xmm1, rax
      dasm_put(Dst, 324, (unsigned int)(op->imm), (unsigned int)((op->imm)>>32));
#line 379 "src/luar_emit_x64.dasc"
    }
    else {
      int64_t bits;
      memcpy(&bits, &op->num, sizeof(bits));
      //| mov64 rax, bits
      //| movq xmm1, rax
      dasm_put(Dst, 151, (unsigned int)(bits), (unsigned int)((bits)>>32));
#line 385 "src/luar_emit_x64.dasc"
    }
  }
  else if (op->tb == LUAR_T_INT) {
    //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
    //| cvtsi2sd xmm1, rax
    dasm_put(Dst, 190, ((int)LUAR_SLOT(b)));
#line 390 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd xmm1, qword [rbx + ((int)LUAR_SLOT(b))]
    dasm_put(Dst, 125, ((int)LUAR_SLOT(b)));
#line 393 "src/luar_emit_x64.dasc"
  }
  //| ucomisd xmm0, xmm1
  dasm_put(Dst, 335);
#line 395 "src/luar_emit_x64.dasc"
  /* Unordered (NaN) comparisons are false, as in Lua. */
  if (k) {
    //| jp =>((int)fallthrough)
    dasm_put(Dst, 340, ((int)fallthrough));
#line 398 "src/luar_emit_x64.dasc"
  }
  else {
    //| jp =>((int)op->target)
    dasm_put(Dst, 340, ((int)op->target));
#line 401 "src/luar_emit_x64.dasc"
  }
  switch (op->kind) {
    case LUAR_OP_LT:
      if (k) {
          //| jb =>((int)op->target)
          dasm_put(Dst, 344, ((int)op->target));
#line 406 "src/luar_emit_x64.dasc"
        }
        else {
          //| jae =>((int)op->target)
          dasm_put(Dst, 348, ((int)op->target));
#line 409 "src/luar_emit_x64.dasc"
        }
      break;
    case LUAR_OP_LE:
      if (k) {
          //| jbe =>((int)op->target)
          dasm_put(Dst, 352, ((int)op->target));
#line 414 "src/luar_emit_x64.dasc"
        }
        else {
          //| ja =>((int)op->target)
          dasm_put(Dst, 356, ((int)op->target));
#line 417 "src/luar_emit_x64.dasc"
        }
      break;
    case LUAR_OP_GT:
      if (k) {
          //| ja =>((int)op->target)
          dasm_put(Dst, 356, ((int)op->target));
#line 422 "src/luar_emit_x64.dasc"
        }
        else {
          //| jbe =>((int)op->target)
          dasm_put(Dst, 352, ((int)op->target));
#line 425 "src/luar_emit_x64.dasc"
        }
      break;
    case LUAR_OP_GE:
      if (k) {
          //| jae =>((int)op->target)
          dasm_put(Dst, 348, ((int)op->target));
#line 430 "src/luar_emit_x64.dasc"
        }
        else {
          //| jb =>((int)op->target)
          dasm_put(Dst, 344, ((int)op->target));
#line 433 "src/luar_emit_x64.dasc"
        }
      break;
    case LUAR_OP_EQ:
      if (k) {
          //| je =>((int)op->target)
          dasm_put(Dst, 278, ((int)op->target));
#line 438 "src/luar_emit_x64.dasc"
        }
        else {
          //| jne =>((int)op->target)
          dasm_put(Dst, 282, ((int)op->target));
#line 441 "src/luar_emit_x64.dasc"
        }
      break;
    default: break;
  }
}

static void emit_table_get (dasm_State **Dst, const LuaR_LoopDesc *d,
                            const LuaR_Op *op)
{
  //| mov rax, qword [rbx + ((int)LUAR_SLOT(op->c))]
  //| cmp rax, 1
  //| jl >3
  //| cmp rax, r14
  //| ja >3
  //| dec rax
  //| shl rax, 4
  //| add rax, r13
  //| cmp byte [rax + 8], ((int)d->tag_int)
  //| jne >3
  //| mov rcx, qword [rax]
  //| mov qword [rbx + ((int)LUAR_SLOT(op->a))], rcx
  //| mov byte [rbx + ((int)LUAR_TAG(op->a))], ((int)d->tag_int)
  dasm_put(Dst, 360, ((int)LUAR_SLOT(op->c)), 8, ((int)d->tag_int), ((int)LUAR_SLOT(op->a)), ((int)LUAR_TAG(op->a)), ((int)d->tag_int));
#line 463 "src/luar_emit_x64.dasc"
}

static void emit_table_add (dasm_State **Dst, const LuaR_LoopDesc *d,
                            const LuaR_Op *op)
{
  //| mov rax, qword [rbx + ((int)LUAR_SLOT(op->c))]
  //| cmp rax, 1
  //| jl >3
  //| cmp rax, r14
  //| ja >3
  //| dec rax
  //| shl rax, 4
  //| add rax, r13
  //| cmp byte [rax + 8], ((int)d->tag_int)
  //| jne >3
  //| mov rcx, qword [rax]
  dasm_put(Dst, 412, ((int)LUAR_SLOT(op->c)), 8, ((int)d->tag_int));
#line 479 "src/luar_emit_x64.dasc"
  if (op->ta == LUAR_T_INT) {
    //| mov rax, qword [rbx + ((int)LUAR_SLOT(op->a))]
    //| add rax, rcx
    //| mov qword [rbx + ((int)LUAR_SLOT(op->a))], rax
    //| mov byte [rbx + ((int)LUAR_TAG(op->a))], ((int)d->tag_int)
    dasm_put(Dst, 456, ((int)LUAR_SLOT(op->a)), ((int)LUAR_SLOT(op->a)), ((int)LUAR_TAG(op->a)), ((int)d->tag_int));
#line 484 "src/luar_emit_x64.dasc"
  }
  else {
    //| cvtsi2sd xmm0, rcx
    //| addsd xmm0, qword [rbx + ((int)LUAR_SLOT(op->a))]
    //| movsd qword [rbx + ((int)LUAR_SLOT(op->a))], xmm0
    //| mov byte [rbx + ((int)LUAR_TAG(op->a))], ((int)d->tag_flt)
    dasm_put(Dst, 472, ((int)LUAR_SLOT(op->a)), ((int)LUAR_SLOT(op->a)), ((int)LUAR_TAG(op->a)), ((int)d->tag_flt));
#line 490 "src/luar_emit_x64.dasc"
  }
}

static void emit_table_set (dasm_State **Dst, const LuaR_LoopDesc *d,
                            const LuaR_Op *op)
{
  //| mov rax, qword [rbx + ((int)LUAR_SLOT(op->b))]
  //| cmp rax, 1
  //| jl >3
  //| cmp rax, r14
  //| ja >3
  //| dec rax
  //| shl rax, 4
  //| add rax, r13
  dasm_put(Dst, 495, ((int)LUAR_SLOT(op->b)));
#line 504 "src/luar_emit_x64.dasc"
  if (op->ta == LUAR_T_INT) {
    //| mov rcx, qword [rbx + ((int)LUAR_SLOT(op->c))]
    //| mov qword [rax], rcx
    //| mov byte [rax + 8], ((int)d->tag_int)
    dasm_put(Dst, 528, ((int)LUAR_SLOT(op->c)), 8, ((int)d->tag_int));
#line 508 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(op->c))]
    //| movsd qword [rax], xmm0
    //| mov byte [rax + 8], ((int)d->tag_flt)
    dasm_put(Dst, 540, ((int)LUAR_SLOT(op->c)), 8, ((int)d->tag_flt));
#line 513 "src/luar_emit_x64.dasc"
  }
}

static void emit_branch_code (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  int i;
  //| push rbx
  //| mov rbx, rdi
  dasm_put(Dst, 556);
#line 521 "src/luar_emit_x64.dasc"
  if (d->has_table) {
    //| push r12
    //| push r13
    //| push r14
    dasm_put(Dst, 562);
#line 525 "src/luar_emit_x64.dasc"
  }
  //| mov r11d, ((int)d->entry_pc)
  dasm_put(Dst, 569, ((int)d->entry_pc));
#line 527 "src/luar_emit_x64.dasc"
  emit_guards(Dst, d);
  if (d->has_table) {
    //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->table_reg))]
    //| mov rax, qword [r12 + ((int)d->table_metatable_off)]
    //| test rax, rax
    //| jnz >3
    //| mov r13, qword [r12 + ((int)d->table_array_off)]
    //| mov r14d, dword [r12 + ((int)d->table_asize_off)]
    dasm_put(Dst, 573, ((int)LUAR_SLOT(d->table_reg)), ((int)d->table_metatable_off), ((int)d->table_array_off), ((int)d->table_asize_off));
#line 535 "src/luar_emit_x64.dasc"
  }
  for (i = 0; i < d->nops; i++) {
    const LuaR_Op *op = &d->ops[i];
    //|=>((int)i):
    dasm_put(Dst, 7, ((int)i));
#line 539 "src/luar_emit_x64.dasc"
    if (op->kind == LUAR_OP_GETTABLE || op->kind == LUAR_OP_TABLE_ADD ||
        op->kind == LUAR_OP_SETTABLE)
      //| mov r11d, ((int)op->pc)
      dasm_put(Dst, 569, ((int)op->pc));
#line 542 "src/luar_emit_x64.dasc"
    switch (op->kind) {
      case LUAR_OP_LT:
      case LUAR_OP_LE:
      case LUAR_OP_GT:
      case LUAR_OP_GE:
      case LUAR_OP_EQ:
        emit_cmp(Dst, d, op, i + 2);
        /* false/true fall-through skips the paired JMP; repeat loops
        ** exit at the descriptor's end label instead. */
        if (d->repeat && i == d->nops - 1)
          //| jmp =>((int)(d->nops + 1))
          dasm_put(Dst, 603, ((int)(d->nops + 1)));
#line 553 "src/luar_emit_x64.dasc"
        else
          //| jmp =>((int)(i + 2))
          dasm_put(Dst, 603, ((int)(i + 2)));
#line 555 "src/luar_emit_x64.dasc"
        break;
      case LUAR_OP_JMP:
        //| jmp =>((int)op->target)
        dasm_put(Dst, 603, ((int)op->target));
#line 558 "src/luar_emit_x64.dasc"
        break;
      case LUAR_OP_MMBIN:
      case LUAR_OP_NOP:
        break;
      case LUAR_OP_GETTABLE:
        emit_table_get(Dst, d, op);
        break;
      case LUAR_OP_TABLE_ADD:
        emit_table_add(Dst, d, op);
        break;
      case LUAR_OP_SETTABLE:
        emit_table_set(Dst, d, op);
        break;
      default:
        emit_int_one(Dst, d, op);
        break;
    }
  }
  if (d->is_while) {
    if (!d->repeat)
      //| jmp =>0
      dasm_put(Dst, 603, 0);
#line 579 "src/luar_emit_x64.dasc"
    if (d->repeat)
      //|=>((int)(d->nops + 1)):
      dasm_put(Dst, 7, ((int)(d->nops + 1)));
#line 581 "src/luar_emit_x64.dasc"
    else
      //|=>((int)d->nops):
    //| mov eax, 1
    dasm_put(Dst, 607, ((int)d->nops));
#line 584 "src/luar_emit_x64.dasc"
    if (d->has_table) {
      //| pop r14
      //| pop r13
      //| pop r12
      dasm_put(Dst, 614);
#line 588 "src/luar_emit_x64.dasc"
    }
    //| pop rbx
    //| ret
    dasm_put(Dst, 621);
#line 591 "src/luar_emit_x64.dasc"
  }
  else {
    //|=>((int)d->nops):
    //| mov rax, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
    //| test rax, rax
    //| jz >2
    //| dec rax
    //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], rax
    //| mov rcx, qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))]
    //| add rcx, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
    //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], rcx
    //| jmp =>0
    //|=>((int)(d->nops + 1)):
    //|2:
    //| mov eax, 1
    dasm_put(Dst, 624, ((int)d->nops), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)), ((int)LUAR_SLOT(d->for_a + 2)), 0, ((int)(d->nops + 1)));
#line 606 "src/luar_emit_x64.dasc"
    if (d->has_table) {
      //| pop r14
      //| pop r13
      //| pop r12
      dasm_put(Dst, 614);
#line 610 "src/luar_emit_x64.dasc"
    }
    //| pop rbx
    //| ret
    dasm_put(Dst, 621);
#line 613 "src/luar_emit_x64.dasc"
  }
  //|3:
  //| mov eax, r11d
  //| add eax, 2
  //| neg eax
  dasm_put(Dst, 668);
#line 618 "src/luar_emit_x64.dasc"
  if (d->has_table) {
    //| pop r14
    //| pop r13
    //| pop r12
    dasm_put(Dst, 614);
#line 622 "src/luar_emit_x64.dasc"
  }
  //| pop rbx
  //| ret
  dasm_put(Dst, 621);
#line 625 "src/luar_emit_x64.dasc"
}

static void emit_fast_float (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  int64_t bits;
  memcpy(&bits, &d->ff_const, sizeof(bits));
  //| push rbx
  //| mov rbx, rdi
  //| push r12
  //| push r13
  //| push r14
  dasm_put(Dst, 680);
#line 636 "src/luar_emit_x64.dasc"
  if (!d->ff_is_float)
    //| push r15
    dasm_put(Dst, 692);
#line 638 "src/luar_emit_x64.dasc"
  emit_guards(Dst, d);
  //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->ff_idx))]
  //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  dasm_put(Dst, 695, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->ff_idx)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 642 "src/luar_emit_x64.dasc"
  if (d->ff_is_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->ff_acc))]
    //| mov64 rax, bits
    //| movq xmm1, rax
    dasm_put(Dst, 708, ((int)LUAR_SLOT(d->ff_acc)), (unsigned int)(bits), (unsigned int)((bits)>>32));
#line 646 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->ff_acc))]
    //| mov64 r11, d->ff_iconst
    dasm_put(Dst, 724, ((int)LUAR_SLOT(d->ff_acc)), (unsigned int)(d->ff_iconst), (unsigned int)((d->ff_iconst)>>32));
#line 650 "src/luar_emit_x64.dasc"
  }
  //|1:
  dasm_put(Dst, 733);
#line 652 "src/luar_emit_x64.dasc"
  if (d->ff_is_float) {
    //| cvtsi2sd xmm2, r13
    //| mulsd xmm2, xmm1
    //| addsd xmm0, xmm2
    dasm_put(Dst, 736);
#line 656 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov rcx, r13
    //| imul rcx, r11
    //| add r15, rcx
    dasm_put(Dst, 753);
#line 661 "src/luar_emit_x64.dasc"
  }
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 765, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 670 "src/luar_emit_x64.dasc"
  if (d->ff_is_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->ff_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->ff_acc)));
#line 672 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->ff_acc))], r15
    //| pop r15
    dasm_put(Dst, 795, ((int)LUAR_SLOT(d->ff_acc)));
#line 676 "src/luar_emit_x64.dasc"
  }
  //| mov eax, 1
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  //|3:
  dasm_put(Dst, 802);
#line 684 "src/luar_emit_x64.dasc"
  if (!d->ff_is_float)
    //| pop r15
  //| xor eax, eax
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 818);
#line 692 "src/luar_emit_x64.dasc"
}

static void emit_fast_fill (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  //| push rbx
  //| mov rbx, rdi
  //| push r12
  //| push r13
  //| push r14
  //| push r15
  //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))]
  //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  dasm_put(Dst, 831, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 705 "src/luar_emit_x64.dasc"
  emit_guards(Dst, d);
  //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->ff_table))]
  //| mov rax, qword [r15 + ((int)d->table_metatable_off)]
  //| test rax, rax
  //| jnz >3
  //| mov r10, qword [r15 + ((int)d->table_array_off)]
  //| mov r11d, dword [r15 + ((int)d->table_asize_off)]
  //|1:
  //| cmp r13, 1
  //| jl >3
  //| cmp r13, r11
  //| ja >3
  //| mov rax, r13
  //| dec rax
  //| shl rax, 4
  //| add rax, r10
  //| mov qword [rax], r13
  //| mov byte [rax + 8], ((int)d->tag_int)
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 857, ((int)LUAR_SLOT(d->ff_table)), ((int)d->table_metatable_off), ((int)d->table_array_off), ((int)d->table_asize_off), 8, ((int)d->tag_int), ((int)LUAR_SLOT(d->for_a)));
#line 731 "src/luar_emit_x64.dasc"
  //| mov eax, 1
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  //|3:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  //| xor eax, eax
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 945, ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 748 "src/luar_emit_x64.dasc"
}

static void emit_fast_int (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  //| push rbx
  //| mov rbx, rdi
  //| push r12
  //| push r13
  //| push r14
  //| push r15
  //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))]
  //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  dasm_put(Dst, 831, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 761 "src/luar_emit_x64.dasc"
  if (d->fi_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fi_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->fi_acc)));
#line 763 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fi_acc))]
    dasm_put(Dst, 987, ((int)LUAR_SLOT(d->fi_acc)));
#line 766 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  dasm_put(Dst, 733);
#line 769 "src/luar_emit_x64.dasc"
  if (d->fi_float) {
    //| cvtsi2sd xmm1, r13
    //| addsd xmm0, xmm1
    dasm_put(Dst, 992);
#line 772 "src/luar_emit_x64.dasc"
  }
  else {
    //| add r15, r13
    dasm_put(Dst, 1004);
#line 775 "src/luar_emit_x64.dasc"
  }
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 765, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 784 "src/luar_emit_x64.dasc"
  if (d->fi_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fi_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fi_acc)));
#line 786 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fi_acc))], r15
    dasm_put(Dst, 1009, ((int)LUAR_SLOT(d->fi_acc)));
#line 789 "src/luar_emit_x64.dasc"
  }
  //| mov eax, 1
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  //|3:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  //| xor eax, eax
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 949, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 807 "src/luar_emit_x64.dasc"
}

static void emit_fast_meta (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  //| push rbx
  //| mov rbx, rdi
  //| push r12
  //| push r13
  //| push r14
  //| push r15
  //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))]
  //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fm_acc))]
  //|1:
  //| mov rax, r13
  dasm_put(Dst, 1014, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)), ((int)LUAR_SLOT(d->fm_acc)));
#line 823 "src/luar_emit_x64.dasc"
  emit_mul_imm(Dst, d->fm_mult);
  //| add r15, rax
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  //| mov qword [rbx + ((int)LUAR_SLOT(d->fm_acc))], r15
  //| mov eax, 1
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  //|3:
  //| xor eax, eax
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 1049, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->fm_acc)));
#line 849 "src/luar_emit_x64.dasc"
}


static void emit_fast_closure (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  //| push rbx
  //| mov rbx, rdi
  //| push r12
  //| push r13
  //| push r14
  //| push r15
  //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))]
  //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fc_upval_slot))]
  //|1:
  //| add r15, r13
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  //| mov qword [rbx + ((int)LUAR_SLOT(d->fc_upval_slot))], r15
  //| mov eax, 1
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  //|3:
  //| xor eax, eax
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 1115, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)), ((int)LUAR_SLOT(d->fc_upval_slot)), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->fc_upval_slot)));
#line 890 "src/luar_emit_x64.dasc"
}


static void emit_fast_tsum (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  int isf = (d->ops[0].ta == LUAR_T_FLOAT);
  //| push rbx
  //| mov rbx, rdi
  //| push r12
  //| push r13
  //| push r14
  //| push r15
  //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->ft_idx))]
  //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  dasm_put(Dst, 831, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->ft_idx)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 905 "src/luar_emit_x64.dasc"
  if (!isf) {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->ft_acc))]
    dasm_put(Dst, 987, ((int)LUAR_SLOT(d->ft_acc)));
#line 907 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->ft_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->ft_acc)));
#line 910 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //| mov r8, qword [rbx + ((int)LUAR_SLOT(d->ft_table))]
  //| mov rax, qword [r8 + ((int)d->table_metatable_off)]
  //| test rax, rax
  //| jnz >3
  //| mov r9, qword [r8 + ((int)d->table_array_off)]
  //| mov r10d, dword [r8 + ((int)d->table_asize_off)]
  dasm_put(Dst, 1213, ((int)LUAR_SLOT(d->ft_table)), ((int)d->table_metatable_off), ((int)d->table_array_off), ((int)d->table_asize_off));
#line 918 "src/luar_emit_x64.dasc"
  /* This fast path only handles step 1 and pre-validates the whole range. */
  //| cmp r14, 1
  //| jne >3
  //| cmp r13, 1
  //| jl >3
  //| mov rax, r13
  //| add rax, r12
  //| cmp rax, r10
  //| ja >3
  //| mov rcx, r13
  //| dec rcx
  //| shl rcx, 4
  //| add rcx, r9
  //| mov rdx, r12
  //| inc rdx
  //|4:
  //| cmp byte [rcx + 8], ((int)d->tag_int)
  //| jne >3
  //| add rcx, 16
  //| dec rdx
  //| jnz <4
  //| mov r11, r13
  //| dec r11
  //| shl r11, 4
  //| add r11, r9
  //|1:
  dasm_put(Dst, 1237, 8, ((int)d->tag_int));
#line 944 "src/luar_emit_x64.dasc"
  if (!isf) {
    //| add r15, qword [r11]
    dasm_put(Dst, 1330);
#line 946 "src/luar_emit_x64.dasc"
  }
  else {
    //| cvtsi2sd xmm1, qword [r11]
    //| addsd xmm0, xmm1
    dasm_put(Dst, 1334);
#line 950 "src/luar_emit_x64.dasc"
  }
  //| add r11, 16
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, d->fw_step
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 1346, d->fw_step, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 960 "src/luar_emit_x64.dasc"
  if (!isf) {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->ft_acc))], r15
    dasm_put(Dst, 1009, ((int)LUAR_SLOT(d->ft_acc)));
#line 962 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->ft_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->ft_acc)));
#line 965 "src/luar_emit_x64.dasc"
  }
  //| mov eax, 1
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  //|3:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 1380, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 976 "src/luar_emit_x64.dasc"
  if (!isf) {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->ft_acc))], r15
    dasm_put(Dst, 1009, ((int)LUAR_SLOT(d->ft_acc)));
#line 978 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->ft_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->ft_acc)));
#line 981 "src/luar_emit_x64.dasc"
  }
  //| xor eax, eax
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 974);
#line 989 "src/luar_emit_x64.dasc"
}

static void emit_fast_while (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  int is_lt = (d->fw_kind == LUAR_OP_LT);
  //| push rbx
  //| mov rbx, rdi
  //| push r12
  //| push r13
  //| push r14
  dasm_put(Dst, 680);
#line 999 "src/luar_emit_x64.dasc"
  if (d->fw_float) {
    union { double d; uint64_t u; } cv;
    cv.d = d->fw_const;
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fw_acc))]
    //| mov64 rax, cv.u
    //| movq xmm2, rax
    dasm_put(Dst, 1406, ((int)LUAR_SLOT(d->fw_acc)), (unsigned int)(cv.u), (unsigned int)((cv.u)>>32));
#line 1005 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->fw_acc))]
    dasm_put(Dst, 1422, ((int)LUAR_SLOT(d->fw_acc)));
#line 1008 "src/luar_emit_x64.dasc"
  }
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->fw_idx))]
  dasm_put(Dst, 1427, ((int)LUAR_SLOT(d->fw_idx)));
#line 1010 "src/luar_emit_x64.dasc"
  if (d->fw_limit == 0xFF) {
    //| mov64 r14, d->fw_imm
    dasm_put(Dst, 1432, (unsigned int)(d->fw_imm), (unsigned int)((d->fw_imm)>>32));
#line 1012 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->fw_limit))]
    dasm_put(Dst, 703, ((int)LUAR_SLOT(d->fw_limit)));
#line 1015 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  //| cmp r13, r14
  dasm_put(Dst, 1437);
#line 1019 "src/luar_emit_x64.dasc"
  if (is_lt) {
    if (d->fw_k) {
      //| jl >2
      dasm_put(Dst, 1444);
#line 1022 "src/luar_emit_x64.dasc"
    }
    else {
      //| jge >2
      dasm_put(Dst, 1449);
#line 1025 "src/luar_emit_x64.dasc"
    }
  }
  else {
    if (d->fw_k) {
      //| jle >2
      dasm_put(Dst, 1454);
#line 1030 "src/luar_emit_x64.dasc"
    }
    else {
      //| jg >2
      dasm_put(Dst, 1459);
#line 1033 "src/luar_emit_x64.dasc"
    }
  }
  if (d->fw_float) {
    //| cvtsi2sd xmm1, r13
    dasm_put(Dst, 1464);
#line 1037 "src/luar_emit_x64.dasc"
    if (d->fw_prod) {
      //| mulsd xmm0, xmm1
      dasm_put(Dst, 173);
#line 1039 "src/luar_emit_x64.dasc"
    }
    else if (d->fw_sub) {
      //| subsd xmm0, xmm1
      dasm_put(Dst, 167);
#line 1042 "src/luar_emit_x64.dasc"
    }
    else if (d->fw_div) {
      //| divsd xmm0, xmm1
      dasm_put(Dst, 179);
#line 1045 "src/luar_emit_x64.dasc"
    }
    else if (d->fw_square) {
      //| mulsd xmm1, xmm1
      //| addsd xmm0, xmm1
      dasm_put(Dst, 1471);
#line 1049 "src/luar_emit_x64.dasc"
    }
    else if (d->fw_linear) {
      //| mulsd xmm1, xmm2
      //| addsd xmm0, xmm1
      dasm_put(Dst, 1482);
#line 1053 "src/luar_emit_x64.dasc"
    }
    else {
      //| addsd xmm0, xmm1
      dasm_put(Dst, 161);
#line 1056 "src/luar_emit_x64.dasc"
    }
  }
  else if (d->fw_prod) {
    //| imul r12, r13
    dasm_put(Dst, 1493);
#line 1060 "src/luar_emit_x64.dasc"
  }
  else if (d->fw_sub) {
    //| sub r12, r13
    dasm_put(Dst, 1498);
#line 1063 "src/luar_emit_x64.dasc"
  }
  else if (d->fw_square) {
    //| mov rcx, r13
    //| imul rcx, r13
    //| add r12, rcx
    dasm_put(Dst, 1503);
#line 1068 "src/luar_emit_x64.dasc"
  }
  else {
    //| add r12, r13
    dasm_put(Dst, 1515);
#line 1071 "src/luar_emit_x64.dasc"
  }
  //| add r13, d->fw_step
  //| jmp <1
  //|2:
  dasm_put(Dst, 1520, d->fw_step);
#line 1075 "src/luar_emit_x64.dasc"
  if (d->fw_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fw_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fw_acc)));
#line 1077 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fw_acc))], r12
    dasm_put(Dst, 940, ((int)LUAR_SLOT(d->fw_acc)));
#line 1080 "src/luar_emit_x64.dasc"
  }
  //| mov qword [rbx + ((int)LUAR_SLOT(d->fw_idx))], r13
  //| mov eax, 1
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  //|3:
  //| xor eax, eax
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 1531, ((int)LUAR_SLOT(d->fw_idx)));
#line 1095 "src/luar_emit_x64.dasc"
}

static void emit_fast_sq (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  //| push rbx
  //| mov rbx, rdi
  //| push r12
  //| push r13
  //| push r14
  //| push r15
  //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))]
  //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  dasm_put(Dst, 831, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 1108 "src/luar_emit_x64.dasc"
  if (d->fs_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fs_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->fs_acc)));
#line 1110 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fs_acc))]
    dasm_put(Dst, 987, ((int)LUAR_SLOT(d->fs_acc)));
#line 1113 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  dasm_put(Dst, 733);
#line 1116 "src/luar_emit_x64.dasc"
  if (d->fs_float) {
    //| cvtsi2sd xmm1, r13
    //| mulsd xmm1, xmm1
    //| addsd xmm0, xmm1
    dasm_put(Dst, 1561);
#line 1120 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov rcx, r13
    //| imul rcx, r13
    //| add r15, rcx
    dasm_put(Dst, 1578);
#line 1125 "src/luar_emit_x64.dasc"
  }
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 765, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 1134 "src/luar_emit_x64.dasc"
  if (d->fs_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fs_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fs_acc)));
#line 1136 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fs_acc))], r15
    dasm_put(Dst, 1009, ((int)LUAR_SLOT(d->fs_acc)));
#line 1139 "src/luar_emit_x64.dasc"
  }
  //| mov eax, 1
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  //|3:
  //| xor eax, eax
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 1085);
#line 1155 "src/luar_emit_x64.dasc"
}

static void emit_fast_prod (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  //| push rbx
  //| mov rbx, rdi
  //| push r12
  //| push r13
  //| push r14
  //| push r15
  //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))]
  //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  dasm_put(Dst, 831, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 1168 "src/luar_emit_x64.dasc"
  if (d->fp_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fp_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->fp_acc)));
#line 1170 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fp_acc))]
    dasm_put(Dst, 987, ((int)LUAR_SLOT(d->fp_acc)));
#line 1173 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  dasm_put(Dst, 733);
#line 1176 "src/luar_emit_x64.dasc"
  if (d->fp_float) {
    //| cvtsi2sd xmm1, r13
    dasm_put(Dst, 1464);
#line 1178 "src/luar_emit_x64.dasc"
    if (d->fp_div)
      //| divsd xmm0, xmm1
      dasm_put(Dst, 179);
#line 1180 "src/luar_emit_x64.dasc"
    else
      //| mulsd xmm0, xmm1
      dasm_put(Dst, 173);
#line 1182 "src/luar_emit_x64.dasc"
  }
  else {
    //| imul r15, r13
    dasm_put(Dst, 1590);
#line 1185 "src/luar_emit_x64.dasc"
  }
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 765, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 1194 "src/luar_emit_x64.dasc"
  if (d->fp_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fp_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fp_acc)));
#line 1196 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fp_acc))], r15
    dasm_put(Dst, 1009, ((int)LUAR_SLOT(d->fp_acc)));
#line 1199 "src/luar_emit_x64.dasc"
  }
  //| mov eax, 1
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  //|3:
  //| xor eax, eax
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 1085);
#line 1215 "src/luar_emit_x64.dasc"
}

static void emit_fast_sub (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  //| push rbx
  //| mov rbx, rdi
  //| push r12
  //| push r13
  //| push r14
  //| push r15
  //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))]
  //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  dasm_put(Dst, 831, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 1228 "src/luar_emit_x64.dasc"
  if (d->fsb_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fsb_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->fsb_acc)));
#line 1230 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fsb_acc))]
    dasm_put(Dst, 987, ((int)LUAR_SLOT(d->fsb_acc)));
#line 1233 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  dasm_put(Dst, 733);
#line 1236 "src/luar_emit_x64.dasc"
  if (d->fsb_float) {
    //| cvtsi2sd xmm1, r13
    //| subsd xmm0, xmm1
    dasm_put(Dst, 1596);
#line 1239 "src/luar_emit_x64.dasc"
  }
  else {
    //| sub r15, r13
    dasm_put(Dst, 1608);
#line 1242 "src/luar_emit_x64.dasc"
  }
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 765, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 1251 "src/luar_emit_x64.dasc"
  if (d->fsb_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fsb_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fsb_acc)));
#line 1253 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fsb_acc))], r15
    dasm_put(Dst, 1009, ((int)LUAR_SLOT(d->fsb_acc)));
#line 1256 "src/luar_emit_x64.dasc"
  }
  //| mov eax, 1
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  //|3:
  //| xor eax, eax
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 1085);
#line 1272 "src/luar_emit_x64.dasc"
}

static void emit_fast_repeat (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  int kind = d->fr_kind, k = d->fr_k;
  //| push rbx
  //| mov rbx, rdi
  //| push r13
  //| push r14
  //| push r15
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->fr_idx))]
  dasm_put(Dst, 1613, ((int)LUAR_SLOT(d->fr_idx)));
#line 1283 "src/luar_emit_x64.dasc"
  if (d->fr_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fr_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->fr_acc)));
#line 1285 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fr_acc))]
    dasm_put(Dst, 987, ((int)LUAR_SLOT(d->fr_acc)));
#line 1288 "src/luar_emit_x64.dasc"
  }
  if (d->fr_limit != 0xFF) {
    //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->fr_limit))]
    dasm_put(Dst, 703, ((int)LUAR_SLOT(d->fr_limit)));
#line 1291 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov64 r14, d->fr_immediate
    dasm_put(Dst, 1432, (unsigned int)(d->fr_immediate), (unsigned int)((d->fr_immediate)>>32));
#line 1294 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  //| add r13, d->fr_step
  dasm_put(Dst, 1629, d->fr_step);
#line 1298 "src/luar_emit_x64.dasc"
  if (d->fr_float) {
    //| cvtsi2sd xmm1, r13
    dasm_put(Dst, 1464);
#line 1300 "src/luar_emit_x64.dasc"
    if (d->fr_op == 1) {
      //| subsd xmm0, xmm1
      dasm_put(Dst, 167);
#line 1302 "src/luar_emit_x64.dasc"
    }
    else if (d->fr_op == 2) {
      //| mulsd xmm0, xmm1
      dasm_put(Dst, 173);
#line 1305 "src/luar_emit_x64.dasc"
    }
    else if (d->fr_op == 3) {
      //| mulsd xmm1, xmm1
      //| addsd xmm0, xmm1
      dasm_put(Dst, 1471);
#line 1309 "src/luar_emit_x64.dasc"
    }
    else if (d->fr_op == 4) {
      //| divsd xmm0, xmm1
      dasm_put(Dst, 179);
#line 1312 "src/luar_emit_x64.dasc"
    }
    else {
      //| addsd xmm0, xmm1
      dasm_put(Dst, 161);
#line 1315 "src/luar_emit_x64.dasc"
    }
  }
  else if (d->fr_op == 1) {
    //| sub r15, r13
    dasm_put(Dst, 1608);
#line 1319 "src/luar_emit_x64.dasc"
  }
  else if (d->fr_op == 2) {
    //| imul r15, r13
    dasm_put(Dst, 1590);
#line 1322 "src/luar_emit_x64.dasc"
  }
  else if (d->fr_op == 3) {
    //| mov rcx, r13
    //| imul rcx, r13
    //| add r15, rcx
    dasm_put(Dst, 1578);
#line 1327 "src/luar_emit_x64.dasc"
  }
  else {
    //| add r15, r13
    dasm_put(Dst, 1004);
#line 1330 "src/luar_emit_x64.dasc"
  }
  if (d->fr_idx_left) {
    //| cmp r13, r14
    dasm_put(Dst, 1439);
#line 1333 "src/luar_emit_x64.dasc"
  }
  else {
    //| cmp r14, r13
    dasm_put(Dst, 1636);
#line 1336 "src/luar_emit_x64.dasc"
  }
  if (kind == LUAR_OP_LT) {
    if (k) {
      //| jl <1
      dasm_put(Dst, 1641);
#line 1340 "src/luar_emit_x64.dasc"
    }
    else {
      //| jge <1
      dasm_put(Dst, 1646);
#line 1343 "src/luar_emit_x64.dasc"
    }
  }
  else if (kind == LUAR_OP_LE) {
    if (k) {
      //| jle <1
      dasm_put(Dst, 1651);
#line 1348 "src/luar_emit_x64.dasc"
    }
    else {
      //| jg <1
      dasm_put(Dst, 1656);
#line 1351 "src/luar_emit_x64.dasc"
    }
  }
  else if (kind == LUAR_OP_GT) {
    if (k) {
      //| jg <1
      dasm_put(Dst, 1656);
#line 1356 "src/luar_emit_x64.dasc"
    }
    else {
      //| jle <1
      dasm_put(Dst, 1651);
#line 1359 "src/luar_emit_x64.dasc"
    }
  }
  else if (kind == LUAR_OP_GE) {
    if (k) {
      //| jge <1
      dasm_put(Dst, 1646);
#line 1364 "src/luar_emit_x64.dasc"
    }
    else {
      //| jl <1
      dasm_put(Dst, 1641);
#line 1367 "src/luar_emit_x64.dasc"
    }
  }
  else {
    if (k) {
      //| je <1
      dasm_put(Dst, 1661);
#line 1372 "src/luar_emit_x64.dasc"
    }
    else {
      //| jne <1
      dasm_put(Dst, 1666);
#line 1375 "src/luar_emit_x64.dasc"
    }
  }
  //| mov qword [rbx + ((int)LUAR_SLOT(d->fr_idx))], r13
  dasm_put(Dst, 790, ((int)LUAR_SLOT(d->fr_idx)));
#line 1378 "src/luar_emit_x64.dasc"
  if (d->fr_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fr_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fr_acc)));
#line 1380 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fr_acc))], r15
    dasm_put(Dst, 1009, ((int)LUAR_SLOT(d->fr_acc)));
#line 1383 "src/luar_emit_x64.dasc"
  }
  //| mov eax, 1
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop rbx
  //| ret
  //|3:
  //|2:
  //| xor eax, eax
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop rbx
  //| ret
  dasm_put(Dst, 1671);
#line 1398 "src/luar_emit_x64.dasc"
}

static void emit_partial_code (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  //| push rbx
  //| mov rbx, rdi
  dasm_put(Dst, 556);
#line 1404 "src/luar_emit_x64.dasc"
  emit_guards(Dst, d);
  emit_body(Dst, d);
  //| mov eax, -((int)d->exit_pc + 2)
  //| pop rbx
  //| ret
  //|3:
  //| xor eax, eax
  //| pop rbx
  //| ret
  dasm_put(Dst, 1699, -((int)d->exit_pc + 2));
#line 1413 "src/luar_emit_x64.dasc"
}

static void emit_code (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  if (d->partial) {
    emit_partial_code(Dst, d);
    return;
  }
  if (d->fast_repeat) {
    emit_fast_repeat(Dst, d);
    return;
  }
  if (d->fast_sub) {
    emit_fast_sub(Dst, d);
    return;
  }
  if (d->fast_prod) {
    emit_fast_prod(Dst, d);
    return;
  }
  if (d->fast_sq) {
    emit_fast_sq(Dst, d);
    return;
  }
  if (d->fast_while) {
    emit_fast_while(Dst, d);
    return;
  }
  if (d->fast_tsum) {
    emit_fast_tsum(Dst, d);
    return;
  }
  if (d->fast_meta) {
    emit_fast_meta(Dst, d);
    return;
  }
  if (d->fast_closure) {
    emit_fast_closure(Dst, d);
    return;
  }
  if (d->fast_int) {
    emit_fast_int(Dst, d);
    return;
  }
  if (d->fast_fill) {
    emit_fast_fill(Dst, d);
    return;
  }
  if (d->fast_float) {
    emit_fast_float(Dst, d);
    return;
  }
  if (d->has_branch) {
    emit_branch_code(Dst, d);
    return;
  }
  //| push rbx
  //| mov rbx, rdi
  dasm_put(Dst, 556);
#line 1471 "src/luar_emit_x64.dasc"
  emit_guards(Dst, d);
  //|1:
  dasm_put(Dst, 733);
#line 1473 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1710, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 1494 "src/luar_emit_x64.dasc"
}

static int emit_trace_ir_func (dasm_State **Dst, const LuaR_TraceIR *t) {
  int i, forslot = -1;
  int side = t->side;
  //| push r12
  //| push r15
  //| push rbx
  //| sub rsp, 16
  //| mov rbx, rdi
  //| mov r12, rsi
  //| mov r15d, 256
  //| mov r11d, ((int)t->startpc)
  //|1:
  dasm_put(Dst, 1761, ((int)t->startpc));
#line 1508 "src/luar_emit_x64.dasc"
  for (i = 0; i < t->nins; i++) {
    const LuaR_IRIns *in = &t->ins[i];
    switch (in->kind) {
      case LUAR_IR_GUARD: {
        int tag = (in->b == LUAR_IR_TAG_INT) ? LUA_VNUMINT :
                  (in->b == LUAR_IR_TAG_FLOAT) ? LUA_VNUMFLT :
                  (in->b == LUAR_IR_TAG_TABLE) ? ctb(LUA_VTABLE) :
                  (in->b == LUAR_IR_TAG_STRING) ? ctb(LUA_VSHRSTR) :
                  (in->b == LUAR_IR_TAG_USERDATA) ? ctb(LUA_VUSERDATA) : 0;
        if (tag == 0)
          return 0;
        //| mov r11d, ((int)in->pc)
        //| cmp byte [rbx + ((int)LUAR_TAG(in->a))], tag
        //| jne >2
        dasm_put(Dst, 1791, ((int)in->pc), ((int)LUAR_TAG(in->a)), tag);
#line 1522 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_LOADI:
        //| mov64 rax, in->ival
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMINT
        dasm_put(Dst, 53, (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 1528 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_KBOOL:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov esi, ((int)in->nval)
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_kbool
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1803, ((int)LUAR_SLOT(in->a)), ((int)in->nval), (unsigned int)((int64_t)(intptr_t)&luar_trace_kbool), (unsigned int)(((int64_t)(intptr_t)&luar_trace_kbool)>>32), ((int)in->pc));
#line 1537 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_KSTR:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rsi, in->ival
        //| mov rdx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_kstr
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1826, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_kstr), (unsigned int)(((int64_t)(intptr_t)&luar_trace_kstr)>>32), ((int)in->pc));
#line 1547 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_LOADF: {
        int64_t bits;
        memcpy(&bits, &in->nval, sizeof(bits));
        //| mov64 rax, bits
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMFLT
        dasm_put(Dst, 53, (unsigned int)(bits), (unsigned int)((bits)>>32), ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->a)), LUA_VNUMFLT);
#line 1554 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_ADDI:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(in->b))]
        //| add rax, in->ival
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMINT
        dasm_put(Dst, 72, ((int)LUAR_SLOT(in->b)), in->ival, ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 1561 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_SLOAD:
      case LUAR_IR_SSTORE:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov al, byte [rbx + ((int)LUAR_TAG(in->b))]
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], al
        dasm_put(Dst, 38, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->b)), ((int)LUAR_TAG(in->a)));
#line 1568 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_ULOAD:
        //| mov rdi, r12
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rdx, in->ival
        //| mov ecx, ((int)in->nval)
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_uload
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1854, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)in->nval), (unsigned int)((int64_t)(intptr_t)&luar_trace_uload), (unsigned int)(((int64_t)(intptr_t)&luar_trace_uload)>>32), ((int)in->pc));
#line 1579 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_ADD:
      case LUAR_IR_SUB:
      case LUAR_IR_MUL:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(in->b))]
        dasm_put(Dst, 185, ((int)LUAR_SLOT(in->b)));
#line 1584 "src/luar_emit_x64.dasc"
        if (in->kind == LUAR_IR_ADD) {
          //| add rax, qword [rbx + ((int)LUAR_SLOT(in->c))]
          dasm_put(Dst, 233, ((int)LUAR_SLOT(in->c)));
#line 1586 "src/luar_emit_x64.dasc"
        }
        else if (in->kind == LUAR_IR_SUB) {
          //| sub rax, qword [rbx + ((int)LUAR_SLOT(in->c))]
          dasm_put(Dst, 238, ((int)LUAR_SLOT(in->c)));
#line 1589 "src/luar_emit_x64.dasc"
        }
        else {
          //| imul rax, qword [rbx + ((int)LUAR_SLOT(in->c))]
          dasm_put(Dst, 243, ((int)LUAR_SLOT(in->c)));
#line 1592 "src/luar_emit_x64.dasc"
        }
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMINT
        dasm_put(Dst, 57, ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 1595 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CGETF:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->c))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_cget_double
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1884, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_cget_double), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_cget_double)>>32), ((int)in->pc));
#line 1605 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CPSETI:
      case LUAR_IR_CPSETF:
      case LUAR_IR_CCALL0D:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall0_double
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1913, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall0_double), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall0_double)>>32), ((int)in->pc));
#line 1616 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL0I:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall0_int
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1913, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall0_int), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall0_int)>>32), ((int)in->pc));
#line 1625 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL1DI:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rsi, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall1di
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1938, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall1di), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall1di)>>32), ((int)in->pc));
#line 1635 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL1DD:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall1dd
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1967, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall1dd), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall1dd)>>32), ((int)in->pc));
#line 1645 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL1FF:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall1ff
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1967, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall1ff), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall1ff)>>32), ((int)in->pc));
#line 1655 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TNEW:
        //| mov rdi, r12
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov edx, ((int)in->ival)
        //| mov ecx, ((int)in->nval)
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tnew
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1998, ((int)LUAR_SLOT(in->a)), ((int)in->ival), ((int)in->nval), (unsigned int)((int64_t)(intptr_t)&luar_trace_tnew), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tnew)>>32), ((int)in->pc));
#line 1666 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TLEN:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov rdx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tlen
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2026, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tlen), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tlen)>>32), ((int)in->pc));
#line 1676 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_STRLEN:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_strlen
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1913, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_strlen), (unsigned int)(((int64_t)(intptr_t)&luar_trace_strlen)>>32), ((int)in->pc));
#line 1685 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CONCATN:
        //| mov rdi, rbx
        //| mov esi, ((int)in->a)
        //| mov edx, ((int)in->ival)
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_concatn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2054, ((int)in->a), ((int)in->ival), (unsigned int)((int64_t)(intptr_t)&luar_trace_concatn), (unsigned int)(((int64_t)(intptr_t)&luar_trace_concatn)>>32), ((int)in->pc));
#line 1696 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CONCAT3:
        //| mov rdi, rbx
        //| mov esi, ((int)in->a)
        //| mov rdx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_concat3
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2081, ((int)in->a), (unsigned int)((int64_t)(intptr_t)&luar_trace_concat3), (unsigned int)(((int64_t)(intptr_t)&luar_trace_concat3)>>32), ((int)in->pc));
#line 1706 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CONCAT2:
        //| mov rdi, rbx
        //| mov esi, ((int)in->a)
        //| mov rdx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_concat2
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2081, ((int)in->a), (unsigned int)((int64_t)(intptr_t)&luar_trace_concat2), (unsigned int)(((int64_t)(intptr_t)&luar_trace_concat2)>>32), ((int)in->pc));
#line 1716 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TGETKT:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tgetkt
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2106, ((int)LUAR_SLOT(in->b)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetkt), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetkt)>>32), ((int)in->pc));
#line 1726 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TGETKB:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tgetkb
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2106, ((int)LUAR_SLOT(in->b)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetkb), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetkb)>>32), ((int)in->pc));
#line 1736 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TGETKS:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tgetks
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2106, ((int)LUAR_SLOT(in->b)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetks), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetks)>>32), ((int)in->pc));
#line 1746 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TGETKF:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tgetkf
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2106, ((int)LUAR_SLOT(in->b)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetkf), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetkf)>>32), ((int)in->pc));
#line 1756 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TGETK:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tgetk
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2106, ((int)LUAR_SLOT(in->b)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetk)>>32), ((int)in->pc));
#line 1766 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSETDYN_KB:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov edx, ((int)in->ival)
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetdynkb
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2135, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)in->ival), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetdynkb), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetdynkb)>>32), ((int)in->pc));
#line 1778 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSETDYN_KI: {
        uint64_t bits;
        memcpy(&bits, &in->nval, sizeof(bits));
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rdx, (int64_t)bits
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetdynki
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2169, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)bits), (unsigned int)(((int64_t)bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetdynki), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetdynki)>>32), ((int)in->pc));
#line 1792 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_TSETDYN_KS: {
        uint64_t bits;
        memcpy(&bits, &in->nval, sizeof(bits));
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rdx, (int64_t)bits
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetdynks
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2169, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)bits), (unsigned int)(((int64_t)bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetdynks), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetdynks)>>32), ((int)in->pc));
#line 1807 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_TSETDYN_KF: {
        uint64_t bits;
        memcpy(&bits, &in->nval, sizeof(bits));
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rax, bits
        //| movq xmm0, rax
        //| mov rdx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetdynkf
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2205, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(bits), (unsigned int)((bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetdynkf), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetdynkf)>>32), ((int)in->pc));
#line 1823 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_TSETDYN:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->c))]
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetdyn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2246, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetdyn), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetdyn)>>32), ((int)in->pc));
#line 1836 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSETUPKN:
        //| mov edi, ((int)in->a)
        //| mov64 rsi, in->ival
        //| mov rdx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetupkn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2282, ((int)in->a), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetupkn), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetupkn)>>32), ((int)in->pc));
#line 1846 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSETUPKV:
        //| mov edi, ((int)in->a)
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetupv
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2308, ((int)in->a), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetupv), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetupv)>>32), ((int)in->pc));
#line 1857 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSETUPKT:
        //| mov edi, ((int)in->a)
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetupt
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2308, ((int)in->a), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetupt), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetupt)>>32), ((int)in->pc));
#line 1868 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSETUPKB:
        //| mov edi, ((int)in->a)
        //| mov64 rsi, in->ival
        //| mov edx, ((int)in->nval)
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetupkb
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2338, ((int)in->a), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)in->nval), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetupkb), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetupkb)>>32), ((int)in->pc));
#line 1879 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSETUPKS: {
        uint64_t bits;
        memcpy(&bits, &in->nval, sizeof(bits));
        //| mov edi, ((int)in->a)
        //| mov64 rsi, in->ival
        //| mov64 rdx, (int64_t)bits
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetupks
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2366, ((int)in->a), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)bits), (unsigned int)(((int64_t)bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetupks), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetupks)>>32), ((int)in->pc));
#line 1892 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_TSETUPKI: {
        uint64_t bits;
        memcpy(&bits, &in->nval, sizeof(bits));
        //| mov edi, ((int)in->a)
        //| mov64 rsi, in->ival
        //| mov64 rdx, (int64_t)bits
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetupki
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2366, ((int)in->a), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)bits), (unsigned int)(((int64_t)bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetupki), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetupki)>>32), ((int)in->pc));
#line 1906 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_TSETUPKF: {
        uint64_t bits;
        memcpy(&bits, &in->nval, sizeof(bits));
        //| mov edi, ((int)in->a)
        //| mov64 rsi, in->ival
        //| mov64 rax, bits
        //| movq xmm0, rax
        //| mov rdx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetupkf
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2396, ((int)in->a), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)(bits), (unsigned int)((bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetupkf), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetupkf)>>32), ((int)in->pc));
#line 1921 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_TSETKDB:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetkdb
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2431, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkdb), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkdb)>>32), ((int)in->pc));
#line 1934 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSETKB:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rsi, in->ival
        //| mov edx, ((int)in->nval)
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetkb
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2467, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)in->nval), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkb), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkb)>>32), ((int)in->pc));
#line 1946 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSETKT:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetkt
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2431, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkt), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkt)>>32), ((int)in->pc));
#line 1958 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSETKS:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetks
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2431, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetks), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetks)>>32), ((int)in->pc));
#line 1970 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSETKKS: {
        uint64_t bits;
        memcpy(&bits, &in->nval, sizeof(bits));
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rsi, in->ival
        //| mov64 rdx, (int64_t)bits
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetkks
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2501, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)bits), (unsigned int)(((int64_t)bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkks), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkks)>>32), ((int)in->pc));
#line 1984 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_TSETKKF: {
        uint64_t bits;
        memcpy(&bits, &in->nval, sizeof(bits));
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rsi, in->ival
        //| mov64 rdx, (int64_t)bits
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetkkf
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2501, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)bits), (unsigned int)(((int64_t)bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkkf), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkkf)>>32), ((int)in->pc));
#line 1999 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_TSETKF:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetkf
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2431, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkf), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkf)>>32), ((int)in->pc));
#line 2012 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSETKK:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rsi, in->ival
        //| mov64 rdx, (int64_t)in->nval
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetkk
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2501, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)in->nval), (unsigned int)(((int64_t)in->nval)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkk)>>32), ((int)in->pc));
#line 2024 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSETK:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetk
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2431, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetk)>>32), ((int)in->pc));
#line 2036 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TGETI:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tgeti
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2537, ((int)LUAR_SLOT(in->b)), in->ival, ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgeti), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgeti)>>32), ((int)in->pc));
#line 2046 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSETIF:
      case LUAR_IR_TSETIS:
      case LUAR_IR_TSETIB:
      case LUAR_IR_TSETIT: {
        const void *fn = (in->kind == LUAR_IR_TSETIF) ?
          (const void *)&luar_trace_tsetif :
          (in->kind == LUAR_IR_TSETIS) ?
          (const void *)&luar_trace_tsetis :
          (in->kind == LUAR_IR_TSETIB) ?
          (const void *)&luar_trace_tsetib :
          (const void *)&luar_trace_tsetit;
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)fn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2431, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)fn), (unsigned int)(((int64_t)(intptr_t)fn)>>32), ((int)in->pc));
#line 2068 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_TSETI:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tseti
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2566, ((int)LUAR_SLOT(in->a)), in->ival, ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tseti), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tseti)>>32), ((int)in->pc));
#line 2081 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL2II:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov esi, dword [rbx + ((int)LUAR_SLOT(in->c))]
        //| mov edx, dword [rbx + ((int)LUAR_SLOT((int)in->ival))]
        //| lea rcx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall2ii
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2602, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2ii), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2ii)>>32), ((int)in->pc));
#line 2092 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL2ID:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rsi, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT((int)in->ival))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall2id
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2633, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2id), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2id)>>32), ((int)in->pc));
#line 2103 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL2DI:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| movq xmm0, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| mov rsi, qword [rbx + ((int)LUAR_SLOT((int)in->ival))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall2di
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2668, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2di), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2di)>>32), ((int)in->pc));
#line 2114 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL2DF:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| movsd xmm1, qword [rbx + ((int)LUAR_SLOT((int)in->ival))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall2df
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2703, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2df), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2df)>>32), ((int)in->pc));
#line 2125 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL2FD:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| movsd xmm1, qword [rbx + ((int)LUAR_SLOT((int)in->ival))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall2fd
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2703, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2fd), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2fd)>>32), ((int)in->pc));
#line 2136 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL2IF:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rsi, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT((int)in->ival))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall2if
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2633, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2if), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2if)>>32), ((int)in->pc));
#line 2147 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL2FI:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| mov rsi, qword [rbx + ((int)LUAR_SLOT((int)in->ival))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall2fi
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2740, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2fi), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2fi)>>32), ((int)in->pc));
#line 2158 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL2FF:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| movq xmm0, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| movq xmm1, qword [rbx + ((int)LUAR_SLOT((int)in->ival))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall2ff
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2775, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2ff), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2ff)>>32), ((int)in->pc));
#line 2169 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL2DD:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| movsd xmm1, qword [rbx + ((int)LUAR_SLOT(in->ival))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall2dd
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2703, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2dd), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2dd)>>32), ((int)in->pc));
#line 2180 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL3IDD: {
        int a2 = (int)(in->ival & 0xff);
        int a3 = (int)((in->ival >> 8) & 0xff);
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rsi, qword [rbx + ((int)LUAR_SLOT(a2))]
        //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| movsd xmm1, qword [rbx + ((int)LUAR_SLOT(a3))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall3idd
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2812, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(a2)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(a3)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall3idd), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall3idd)>>32), ((int)in->pc));
#line 2194 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CCALL3DID: {
        int a2 = (int)(in->ival & 0xff);
        int a3 = (int)((in->ival >> 8) & 0xff);
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| mov rsi, qword [rbx + ((int)LUAR_SLOT(a2))]
        //| movsd xmm1, qword [rbx + ((int)LUAR_SLOT(a3))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall3did
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2853, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(a2)), ((int)LUAR_SLOT(a3)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall3did), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall3did)>>32), ((int)in->pc));
#line 2209 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CCALL3DDI: {
        int a2 = (int)(in->ival & 0xff);
        int a3 = (int)((in->ival >> 8) & 0xff);
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| movsd xmm1, qword [rbx + ((int)LUAR_SLOT(a2))]
        //| mov rsi, qword [rbx + ((int)LUAR_SLOT(a3))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall3ddi
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2894, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(a2)), ((int)LUAR_SLOT(a3)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall3ddi), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall3ddi)>>32), ((int)in->pc));
#line 2224 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CCALL3DD: {
        int a2 = (int)(in->ival & 0xff);
        int a3 = (int)((in->ival >> 8) & 0xff);
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| movsd xmm1, qword [rbx + ((int)LUAR_SLOT(a2))]
        //| movsd xmm2, qword [rbx + ((int)LUAR_SLOT(a3))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall3dd
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2935, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(a2)), ((int)LUAR_SLOT(a3)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall3dd), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall3dd)>>32), ((int)in->pc));
#line 2239 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CCALL1I:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rsi, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall1i
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMINT
        dasm_put(Dst, 2978, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall1i), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall1i)>>32), ((int)in->pc), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 2251 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CPSETI64: {
        const void *setfn = (in->kind == LUAR_IR_CPSETF) ?
          (const void *)&luar_ffi_trace_cset_ptr_double :
          (in->kind == LUAR_IR_CPSETI64 ?
           (const void *)&luar_ffi_trace_cset_ptr_i64 :
           (const void *)&luar_ffi_trace_cset_ptr_int);
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov rsi, ((int64_t)in->ival)
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rcx, [rbx + ((int)LUAR_SLOT(in->c))]
        //| mov64 rax, (int64_t)(intptr_t)setfn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 3011, ((int)LUAR_SLOT(in->a)), ((int64_t)in->ival), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)setfn), (unsigned int)(((int64_t)(intptr_t)setfn)>>32), ((int)in->pc));
#line 2267 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CPGETI:
      case LUAR_IR_CPGETF:
      case LUAR_IR_CPGETI64: {
        const void *getfn = (in->kind == LUAR_IR_CPGETF) ?
          (const void *)&luar_ffi_trace_cget_ptr_double :
          (in->kind == LUAR_IR_CPGETI64 ?
           (const void *)&luar_ffi_trace_cget_ptr_i64 :
           (const void *)&luar_ffi_trace_cget_ptr_int);
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rsi, ((int64_t)in->ival)
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->c))]
        //| lea rcx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)getfn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 3011, ((int)LUAR_SLOT(in->b)), ((int64_t)in->ival), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)getfn), (unsigned int)(((int64_t)(intptr_t)getfn)>>32), ((int)in->pc));
#line 2286 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CSETFIELDK: {
        int isf = (in->c == 2);
        const void *cfn = isf ?
          (const void *)&luar_ffi_trace_cset_field_double_const :
          (const void *)&luar_ffi_trace_cset_field_int_const;
        int64_t bits;
        memcpy(&bits, &in->nval, sizeof(bits));
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov rsi, ((int64_t)in->ival)
        //| mov rdx, ((int64_t)in->b)
        dasm_put(Dst, 3044, ((int)LUAR_SLOT(in->a)), ((int64_t)in->ival), ((int64_t)in->b));
#line 2298 "src/luar_emit_x64.dasc"
        if (isf) {
          //| mov64 rax, bits
          //| movq xmm0, rax
          dasm_put(Dst, 89, (unsigned int)(bits), (unsigned int)((bits)>>32));
#line 2301 "src/luar_emit_x64.dasc"
        }
        else {
          //| mov rcx, ((int64_t)in->nval)
          dasm_put(Dst, 3057, ((int64_t)in->nval));
#line 2304 "src/luar_emit_x64.dasc"
        }
        //| mov64 rax, (int64_t)(intptr_t)cfn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1809, (unsigned int)((int64_t)(intptr_t)cfn), (unsigned int)(((int64_t)(intptr_t)cfn)>>32), ((int)in->pc));
#line 2310 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CSETFIELD: {
        const void *setfn = (in->c == 2) ?
          (const void *)&luar_ffi_trace_cset_field_double :
          (const void *)&luar_ffi_trace_cset_field_int;
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov rsi, ((int64_t)in->ival)
        //| mov rdx, ((int64_t)in->nval)
        //| lea rcx, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rax, (int64_t)(intptr_t)setfn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 3062, ((int)LUAR_SLOT(in->a)), ((int64_t)in->ival), ((int64_t)in->nval), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)setfn), (unsigned int)(((int64_t)(intptr_t)setfn)>>32), ((int)in->pc));
#line 2325 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CGETFIELD: {
        const void *getfn = (in->c == 2) ?
          (const void *)&luar_ffi_trace_cget_field_double :
          (const void *)&luar_ffi_trace_cget_field_int;
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rsi, ((int64_t)in->ival)
        //| mov rdx, ((int64_t)in->nval)
        //| lea rcx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)getfn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 3062, ((int)LUAR_SLOT(in->b)), ((int64_t)in->ival), ((int64_t)in->nval), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)getfn), (unsigned int)(((int64_t)(intptr_t)getfn)>>32), ((int)in->pc));
#line 2340 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CSETF:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->c))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_cset_double
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1884, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_cset_double), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_cset_double)>>32), ((int)in->pc));
#line 2351 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CSETI:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->c))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_cset_int
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1884, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_cset_int), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_cset_int)>>32), ((int)in->pc));
#line 2361 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CSETI64:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->c))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_set_i64
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1884, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_set_i64), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_set_i64)>>32), ((int)in->pc));
#line 2371 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CMODI64:
      case LUAR_IR_CUMODI64: {
        int64_t divisor = in->ival;
        const void *fn = (in->kind == LUAR_IR_CUMODI64) ?
          (const void *)&luar_ffi_trace_umod_i64 :
          (const void *)&luar_ffi_trace_mod_i64;
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rdx, divisor
        //| mov64 rax, (int64_t)(intptr_t)fn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 3095, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(divisor), (unsigned int)((divisor)>>32), (unsigned int)((int64_t)(intptr_t)fn), (unsigned int)(((int64_t)(intptr_t)fn)>>32), ((int)in->pc));
#line 2386 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CBANDI64:
      case LUAR_IR_CBORI64:
      case LUAR_IR_CBXORI64: {
        int64_t factor = in->ival;
        const void *fn = (in->kind == LUAR_IR_CBANDI64) ?
          (const void *)&luar_ffi_trace_band_i64 :
          (in->kind == LUAR_IR_CBORI64) ?
          (const void *)&luar_ffi_trace_bor_i64 :
          (const void *)&luar_ffi_trace_bxor_i64;
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rdx, factor
        //| mov64 rax, (int64_t)(intptr_t)fn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 3095, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(factor), (unsigned int)((factor)>>32), (unsigned int)((int64_t)(intptr_t)fn), (unsigned int)(((int64_t)(intptr_t)fn)>>32), ((int)in->pc));
#line 2405 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CSHLI64:
      case LUAR_IR_CSHRI64: {
        int64_t count = in->ival;
        const void *fn = (in->kind == LUAR_IR_CSHLI64) ?
          (const void *)&luar_ffi_trace_shl_i64 :
          (const void *)&luar_ffi_trace_shr_i64;
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rdx, count
        //| mov64 rax, (int64_t)(intptr_t)fn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 3095, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(count), (unsigned int)((count)>>32), (unsigned int)((int64_t)(intptr_t)fn), (unsigned int)(((int64_t)(intptr_t)fn)>>32), ((int)in->pc));
#line 2421 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CNEGI64:
      case LUAR_IR_CBNOTI64: {
        const void *fn = (in->kind == LUAR_IR_CNEGI64) ?
          (const void *)&luar_ffi_trace_neg_i64 :
          (const void *)&luar_ffi_trace_bnot_i64;
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rax, (int64_t)(intptr_t)fn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1913, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)fn), (unsigned int)(((int64_t)(intptr_t)fn)>>32), ((int)in->pc));
#line 2435 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CDIVI64:
      case LUAR_IR_CUDIVI64: {
        int64_t divisor = in->ival;
        const void *fn = (in->kind == LUAR_IR_CUDIVI64) ?
          (const void *)&luar_ffi_trace_udiv_i64 :
          (const void *)&luar_ffi_trace_div_i64;
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rdx, divisor
        //| mov64 rax, (int64_t)(intptr_t)fn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 3095, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(divisor), (unsigned int)((divisor)>>32), (unsigned int)((int64_t)(intptr_t)fn), (unsigned int)(((int64_t)(intptr_t)fn)>>32), ((int)in->pc));
#line 2451 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CMULI64: {
        int64_t factor = in->ival;
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rdx, factor
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_mul_i64
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 3095, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(factor), (unsigned int)((factor)>>32), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_mul_i64), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_mul_i64)>>32), ((int)in->pc));
#line 2463 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CADDI64: {
        int64_t delta = in->ival;
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rdx, delta
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_add_i64
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 3095, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(delta), (unsigned int)((delta)>>32), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_add_i64), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_add_i64)>>32), ((int)in->pc));
#line 2475 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CCPYI64:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->c))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_copy_i64
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1884, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_copy_i64), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_copy_i64)>>32), ((int)in->pc));
#line 2486 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CGETI:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->c))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_cget_int
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1884, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_cget_int), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_cget_int)>>32), ((int)in->pc));
#line 2496 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TGET:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rsi, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        dasm_put(Dst, 3124, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)));
#line 2501 "src/luar_emit_x64.dasc"
        if (side) {
          //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tget_generic
          dasm_put(Dst, 3137, (unsigned int)((int64_t)(intptr_t)&luar_trace_tget_generic), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tget_generic)>>32));
#line 2503 "src/luar_emit_x64.dasc"
        }
        else {
          //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tget_int
          dasm_put(Dst, 3137, (unsigned int)((int64_t)(intptr_t)&luar_trace_tget_int), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tget_int)>>32));
#line 2506 "src/luar_emit_x64.dasc"
        }
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMINT
        dasm_put(Dst, 2994, ((int)in->pc), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 2512 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSET: {
        const void *tsetfn = (const void *)&luar_trace_tset_insert;
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov rsi, qword [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->c))]
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)tsetfn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 3142, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)tsetfn), (unsigned int)(((int64_t)(intptr_t)tsetfn)>>32), ((int)in->pc));
#line 2525 "src/luar_emit_x64.dasc"
        /* GC may have relocated the stack. */
        break;
      }
      case LUAR_IR_ADDK:
      case LUAR_IR_SUBK:
      case LUAR_IR_MULK:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(in->b))]
        dasm_put(Dst, 185, ((int)LUAR_SLOT(in->b)));
#line 2532 "src/luar_emit_x64.dasc"
        if (in->kind == LUAR_IR_ADDK) {
          //| add rax, in->ival
          dasm_put(Dst, 3178, in->ival);
#line 2534 "src/luar_emit_x64.dasc"
        }
        else if (in->kind == LUAR_IR_SUBK) {
          //| sub rax, in->ival
          dasm_put(Dst, 3183, in->ival);
#line 2537 "src/luar_emit_x64.dasc"
        }
        else {
          //| imul rax, in->ival
          dasm_put(Dst, 3188, in->ival);
#line 2540 "src/luar_emit_x64.dasc"
        }
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMINT
        dasm_put(Dst, 57, ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 2543 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CGUARD:
        //| mov r11d, ((int)in->pc)
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rcx, (int64_t)(in->ival)
        //| cmp rax, rcx
        //| jne >8
        dasm_put(Dst, 3193, ((int)in->pc), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(in->ival)), (unsigned int)(((int64_t)(in->ival))>>32));
#line 2550 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TGETF:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rsi, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tgetf_generic
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMFLT
        dasm_put(Dst, 2978, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetf_generic), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetf_generic)>>32), ((int)in->pc), ((int)LUAR_TAG(in->a)), LUA_VNUMFLT);
#line 2561 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_ITOF:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_itof
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1913, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_itof), (unsigned int)(((int64_t)(intptr_t)&luar_trace_itof)>>32), ((int)in->pc));
#line 2570 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_FADDI: {
        uint64_t bits;
        memcpy(&bits, &in->nval, sizeof(bits));
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rax, bits
        //| movq xmm0, rax
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_faddk
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 3212, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(bits), (unsigned int)((bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_faddk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_faddk)>>32), ((int)in->pc));
#line 2583 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_FMULK: {
        int64_t bits;
        memcpy(&bits, &in->nval, sizeof(bits));
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rax, bits
        //| movq xmm0, rax
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_fmulk
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 3212, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(bits), (unsigned int)((bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_fmulk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_fmulk)>>32), ((int)in->pc));
#line 2597 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_FADD:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->c))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_fadd
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1884, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_trace_fadd), (unsigned int)(((int64_t)(intptr_t)&luar_trace_fadd)>>32), ((int)in->pc));
#line 2608 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_FSUB:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->c))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_fsub
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1884, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_trace_fsub), (unsigned int)(((int64_t)(intptr_t)&luar_trace_fsub)>>32), ((int)in->pc));
#line 2618 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_FDIVK: {
        int64_t bits;
        memcpy(&bits, &in->nval, sizeof(bits));
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rax, bits
        //| movq xmm0, rax
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_fdivk
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 3212, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(bits), (unsigned int)((bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_fdivk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_fdivk)>>32), ((int)in->pc));
#line 2631 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CALLEXIT:
        //| mov r11d, ((int)in->pc)
        //| jmp >8
        dasm_put(Dst, 3246, ((int)in->pc));
#line 2636 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_FORLOOP:
        if (forslot >= 0)
          return 0;
        forslot = in->a;
        if (side) {
          //| mov rax, qword [rbx + ((int)LUAR_SLOT(forslot))]
          //| test rax, rax
          //| jz >5
          //| dec rax
          //| mov qword [rbx + ((int)LUAR_SLOT(forslot))], rax
          //| mov rcx, qword [rbx + ((int)LUAR_SLOT(forslot + 2))]
          //| add rcx, qword [rbx + ((int)LUAR_SLOT(forslot + 1))]
          //| mov qword [rbx + ((int)LUAR_SLOT(forslot + 2))], rcx
          //| xor eax, eax
          //| add rsp, 16
          //| pop rbx
          //| pop r15
          //| pop r12
          //| ret
          //|5:
          //| mov eax, 1
          //| add rsp, 16
          //| pop rbx
          //| pop r15
          //| pop r12
          //| ret
          dasm_put(Dst, 3254, ((int)LUAR_SLOT(forslot)), ((int)LUAR_SLOT(forslot)), ((int)LUAR_SLOT(forslot + 2)), ((int)LUAR_SLOT(forslot + 1)), ((int)LUAR_SLOT(forslot + 2)));
#line 2663 "src/luar_emit_x64.dasc"
          break;
        }
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(forslot))]
        //| test rax, rax
        //| jz >3
        //| dec rax
        //| mov qword [rbx + ((int)LUAR_SLOT(forslot))], rax
        //| mov rcx, qword [rbx + ((int)LUAR_SLOT(forslot + 2))]
        //| add rcx, qword [rbx + ((int)LUAR_SLOT(forslot + 1))]
        //| mov qword [rbx + ((int)LUAR_SLOT(forslot + 2))], rcx
        //| dec r15d
        //| jz >6
        //| jmp <1
        dasm_put(Dst, 3315, ((int)LUAR_SLOT(forslot)), ((int)LUAR_SLOT(forslot)), ((int)LUAR_SLOT(forslot + 2)), ((int)LUAR_SLOT(forslot + 1)), ((int)LUAR_SLOT(forslot + 2)));
#line 2676 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_EXIT:
      case LUAR_IR_RET:
        break;
      default:
        return 0;
    }
  }
  //|3:
  //| mov eax, 1
  //| add rsp, 16
  //| pop rbx
  //| pop r15
  //| pop r12
  //| ret
  //|2:
  dasm_put(Dst, 3359);
#line 2692 "src/luar_emit_x64.dasc"
  if (side) {
    //| mov eax, r11d
    //| neg eax
    //| sub eax, 2
    dasm_put(Dst, 3379);
#line 2696 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov eax, r11d
    //| neg eax
    //| sub eax, 2
    dasm_put(Dst, 3379);
#line 2701 "src/luar_emit_x64.dasc"
  }
  //| add rsp, 16
  //| pop rbx
  //| pop r15
  //| pop r12
  //| ret
  //|6:
  //| mov eax, 2
  //| add rsp, 16
  //| pop rbx
  //| pop r15
  //| pop r12
  //| ret
  //|8:
  //| mov eax, r11d
  //| neg eax
  //| sub eax, 2
  //| add rsp, 16
  //| pop rbx
  //| pop r15
  //| pop r12
  //| ret
  dasm_put(Dst, 3389);
#line 2723 "src/luar_emit_x64.dasc"
  return 1;
}

int luari_emit_trace_ir (const LuaR_TraceIR *t, void **code, size_t *size) {
  dasm_State *Dst;
  size_t sz = 0;
  void *mem;
  int status;
  if (t == NULL || code == NULL || size == NULL || t->nins <= 0)
    return 0;
  dasm_init(&Dst, DASM_MAXSECTION);
  dasm_setupglobal(&Dst, NULL, 0);
  dasm_setup(&Dst, luar_emit_actions);
  if (!emit_trace_ir_func(&Dst, t)) {
    dasm_free(&Dst);
    return 0;
  }
  if (dasm_checkstep(&Dst, -1) != 0 ||
      dasm_link(&Dst, &sz) != 0) {
    dasm_free(&Dst);
    return 0;
  }
  mem = mmap(NULL, sz, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mem == MAP_FAILED) {
    dasm_free(&Dst);
    return 0;
  }
  status = dasm_encode(&Dst, mem);
  if (status != 0 || mprotect(mem, sz, PROT_READ | PROT_EXEC) != 0) {
    if (status == 0) munmap(mem, sz);
    dasm_free(&Dst);
    return 0;
  }
  dasm_free(&Dst);
  *code = mem;
  *size = sz;
  return 1;
}


LuaR_LoopFn luari_emit_build (const LuaR_LoopDesc *d, size_t *code_size)
{
  dasm_State *Dst;
  size_t size = 0;
  void *mem;
  int status;
  if (d == NULL || code_size == NULL || d->ops == NULL || d->nops <= 0 ||
      (!d->is_while && d->for_a < 0) || d->for_a + 2 > 254)
    return NULL;
  dasm_init(&Dst, DASM_MAXSECTION);
  dasm_setupglobal(&Dst, NULL, 0);
  dasm_setup(&Dst, luar_emit_actions);
  if (d->has_branch && d->npc > 0)
    dasm_growpc(&Dst, (unsigned)d->npc + 2);
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

/* x86 has no add/sub/imul r64, imm64 form: the immediate must fit in a
** sign-extended 32-bit field.  Full-width Lua integer constants go through
** a scratch register instead of being silently truncated. */
static int imm32 (int64_t v)
{
  return v >= INT32_MIN && v <= INT32_MAX;
}

static void emit_add_imm (dasm_State **Dst, int64_t imm)
{
  if (imm32(imm))
    //| add rax, imm
    dasm_put(Dst, 3178, imm);
#line 2820 "src/luar_emit_x64.dasc"
  else {
    //| mov64 rcx, imm
    //| add rax, rcx
    dasm_put(Dst, 3438, (unsigned int)(imm), (unsigned int)((imm)>>32));
#line 2823 "src/luar_emit_x64.dasc"
  }
}

static void emit_sub_imm (dasm_State **Dst, int64_t imm)
{
  if (imm32(imm))
    //| sub rax, imm
    dasm_put(Dst, 3183, imm);
#line 2830 "src/luar_emit_x64.dasc"
  else {
    //| mov64 rcx, imm
    //| sub rax, rcx
    dasm_put(Dst, 3446, (unsigned int)(imm), (unsigned int)((imm)>>32));
#line 2833 "src/luar_emit_x64.dasc"
  }
}

static void emit_mul_imm (dasm_State **Dst, int64_t imm)
{
  if (imm32(imm))
    //| imul rax, imm
    dasm_put(Dst, 3188, imm);
#line 2840 "src/luar_emit_x64.dasc"
  else {
    //| mov64 rcx, imm
    //| imul rax, rcx
    dasm_put(Dst, 3454, (unsigned int)(imm), (unsigned int)((imm)>>32));
#line 2843 "src/luar_emit_x64.dasc"
  }
}
