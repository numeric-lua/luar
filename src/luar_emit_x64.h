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
static const unsigned char luar_emit_actions[3623] = {
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
  131,233,255,72,15,175,131,233,255,72,139,131,233,72,139,139,233,255,72,33,
  200,255,72,9,200,255,72,49,200,255,72,139,131,233,72,185,237,237,255,72,139,
  131,233,72,252,247,208,72,137,131,233,198,131,233,235,255,49,192,255,72,193,
  232,235,255,72,193,224,235,255,72,139,131,233,72,252,247,216,72,137,131,233,
  198,131,233,235,255,72,139,131,233,72,185,237,237,72,49,200,72,137,131,233,
  198,131,233,235,255,72,139,131,233,72,139,139,233,72,133,201,15,136,245,72,
  131,252,249,64,15,141,245,255,72,211,224,255,72,211,232,255,252,233,245,249,
  72,131,252,249,192,15,142,245,72,252,247,217,255,252,233,245,249,49,192,249,
  72,137,131,233,198,131,233,235,255,72,185,237,237,72,57,200,255,72,59,131,
  233,255,15,140,245,255,15,142,245,255,15,143,245,255,15,132,245,255,15,133,
  245,255,65,187,237,72,139,131,233,72,185,237,237,72,57,200,15,143,244,249,
  72,185,237,237,72,57,200,15,140,244,249,255,65,187,237,252,233,244,249,255,
  72,184,237,237,252,242,72,15,42,200,255,102,15,46,193,255,15,138,245,255,
  15,130,245,255,15,131,245,255,15,134,245,255,15,135,245,255,72,139,131,233,
  72,131,252,248,1,15,140,244,249,76,57,252,240,15,135,244,249,72,252,255,200,
  72,193,224,4,76,1,232,128,184,233,235,15,133,244,249,72,139,8,72,137,139,
  233,198,131,233,235,255,72,139,131,233,72,131,252,248,1,15,140,244,249,76,
  57,252,240,15,135,244,249,72,252,255,200,72,193,224,4,76,1,232,128,184,233,
  235,15,133,244,249,72,139,8,255,72,139,131,233,72,1,200,72,137,131,233,198,
  131,233,235,255,252,242,72,15,42,193,252,242,15,88,131,233,252,242,15,17,
  131,233,198,131,233,235,255,72,139,131,233,72,131,252,248,1,15,140,244,249,
  76,57,252,240,15,135,244,249,72,252,255,200,72,193,224,4,76,1,232,255,72,
  139,139,233,72,137,8,198,128,233,235,255,252,242,15,16,131,233,252,242,15,
  17,0,198,128,233,235,255,83,72,137,252,251,255,65,84,65,85,65,86,255,65,187,
  237,255,76,139,163,233,73,139,132,253,36,233,72,133,192,15,133,244,249,77,
  139,172,253,36,233,69,139,180,253,36,233,255,252,233,245,255,249,184,1,0,
  0,0,255,65,94,65,93,65,92,255,91,195,255,249,72,139,131,233,72,133,192,15,
  132,244,248,72,252,255,200,72,137,131,233,72,139,139,233,72,3,139,233,72,
  137,139,233,252,233,245,249,248,2,184,1,0,0,0,255,248,3,68,137,216,131,192,
  2,252,247,216,255,83,72,137,252,251,65,84,65,85,65,86,255,65,87,255,76,139,
  163,233,76,139,171,233,76,139,179,233,255,252,242,15,16,131,233,72,184,237,
  237,102,72,15,110,200,255,76,139,187,233,73,187,237,237,255,248,1,255,252,
  242,73,15,42,213,252,242,15,89,209,255,252,242,15,17,147,233,198,131,233,
  235,255,252,242,15,88,194,255,76,137,252,233,73,15,175,203,255,73,1,207,255,
  77,133,228,15,132,244,248,73,252,255,204,77,1,252,245,252,233,244,1,248,2,
  76,137,163,233,76,137,171,233,255,76,137,187,233,65,95,255,184,1,0,0,0,65,
  94,65,93,65,92,91,195,248,3,255,65,95,49,192,65,94,65,93,65,92,91,195,255,
  83,72,137,252,251,65,84,65,85,65,86,65,87,76,139,163,233,76,139,171,233,76,
  139,179,233,255,76,139,187,233,73,139,135,233,72,133,192,15,133,244,249,77,
  139,151,233,69,139,159,233,248,1,73,131,252,253,1,15,140,244,249,77,57,221,
  15,135,244,249,76,137,232,72,252,255,200,72,193,224,4,76,1,208,76,137,40,
  198,128,233,235,77,133,228,15,132,244,248,73,252,255,204,77,1,252,245,252,
  233,244,1,248,2,76,137,163,233,255,76,137,171,233,184,1,0,0,0,65,95,65,94,
  65,93,65,92,91,195,248,3,76,137,163,233,76,137,171,233,49,192,65,95,65,94,
  65,93,65,92,91,195,255,76,139,187,233,255,252,242,73,15,42,205,252,242,15,
  88,193,255,77,1,252,239,255,76,137,187,233,255,83,72,137,252,251,65,84,65,
  85,65,86,65,87,76,139,163,233,76,139,171,233,76,139,179,233,76,139,187,233,
  248,1,76,137,232,255,73,1,199,77,133,228,15,132,244,248,73,252,255,204,77,
  1,252,245,252,233,244,1,248,2,76,137,163,233,76,137,171,233,76,137,187,233,
  184,1,0,0,0,65,95,65,94,65,93,65,92,91,195,248,3,49,192,65,95,65,94,65,93,
  65,92,91,195,255,83,72,137,252,251,65,84,65,85,65,86,65,87,76,139,163,233,
  76,139,171,233,76,139,179,233,76,139,187,233,248,1,77,1,252,239,77,133,228,
  15,132,244,248,73,252,255,204,77,1,252,245,252,233,244,1,248,2,76,137,163,
  233,76,137,171,233,76,137,187,233,184,1,0,0,0,65,95,65,94,65,93,65,92,91,
  195,248,3,49,192,65,95,65,94,65,93,65,92,91,195,255,76,139,131,233,73,139,
  128,233,72,133,192,15,133,244,249,77,139,136,233,69,139,144,233,255,73,131,
  252,254,1,15,133,244,249,73,131,252,253,1,15,140,244,249,76,137,232,76,1,
  224,76,57,208,15,135,244,249,76,137,252,233,72,252,255,201,72,193,225,4,76,
  1,201,76,137,226,72,252,255,194,248,4,128,185,233,235,15,133,244,249,72,131,
  193,16,72,252,255,202,15,133,244,4,77,137,252,235,73,252,255,203,73,193,227,
  4,77,1,203,248,1,255,77,3,59,255,252,242,73,15,42,11,252,242,15,88,193,255,
  73,131,195,16,77,133,228,15,132,244,248,73,252,255,204,73,129,197,239,252,
  233,244,1,248,2,76,137,163,233,76,137,171,233,255,184,1,0,0,0,65,95,65,94,
  65,93,65,92,91,195,248,3,76,137,163,233,76,137,171,233,255,252,242,15,16,
  131,233,72,184,237,237,102,72,15,110,208,255,76,139,163,233,255,76,139,171,
  233,255,73,190,237,237,255,248,1,77,57,252,245,255,15,140,244,248,255,15,
  141,244,248,255,15,142,244,248,255,15,143,244,248,255,252,242,73,15,42,205,
  255,76,137,252,233,73,15,175,205,255,252,242,72,15,42,201,252,242,15,88,193,
  255,252,242,15,89,202,255,252,242,15,17,139,233,198,131,233,235,255,77,15,
  175,229,255,77,41,252,236,255,73,1,204,255,77,1,252,236,255,73,129,197,239,
  252,233,244,1,248,2,255,76,137,171,233,184,1,0,0,0,65,94,65,93,65,92,91,195,
  248,3,49,192,65,94,65,93,65,92,91,195,255,248,1,76,137,252,233,73,15,175,
  205,255,77,15,175,252,253,255,252,242,73,15,42,205,252,242,15,92,193,255,
  77,41,252,239,255,83,72,137,252,251,65,85,65,86,65,87,76,139,171,233,255,
  248,1,73,129,197,239,255,77,57,252,238,255,15,140,244,1,255,15,141,244,1,
  255,15,142,244,1,255,15,143,244,1,255,15,132,244,1,255,15,133,244,1,255,184,
  1,0,0,0,65,95,65,94,65,93,91,195,248,3,248,2,49,192,65,95,65,94,65,93,91,
  195,255,184,237,91,195,248,3,49,192,91,195,255,72,139,131,233,72,133,192,
  15,132,244,248,72,252,255,200,72,137,131,233,72,139,139,233,72,3,139,233,
  72,137,139,233,252,233,244,1,248,2,184,1,0,0,0,91,195,248,3,49,192,91,195,
  255,65,84,65,87,83,72,131,252,236,16,72,137,252,251,73,137,252,244,65,191,
  0,1,0,0,65,187,237,248,1,255,65,187,237,128,187,233,235,15,133,244,248,255,
  72,141,187,233,190,237,72,184,237,237,252,255,208,65,187,237,133,192,15,132,
  244,248,255,72,141,187,233,72,190,237,237,76,137,226,72,184,237,237,252,255,
  208,65,187,237,133,192,15,132,244,248,255,76,137,231,72,141,179,233,72,186,
  237,237,185,237,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,
  248,255,72,141,187,233,72,141,179,233,72,141,147,233,72,184,237,237,252,255,
  208,65,187,237,133,192,15,132,244,248,255,72,141,187,233,72,141,179,233,72,
  184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,72,141,187,
  233,72,139,179,233,72,141,147,233,72,184,237,237,252,255,208,65,187,237,133,
  192,15,132,244,248,255,72,141,187,233,252,242,15,16,131,233,72,141,179,233,
  72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,76,137,231,
  72,141,179,233,186,237,185,237,72,184,237,237,252,255,208,65,187,237,133,
  192,15,132,244,248,255,72,141,187,233,72,141,179,233,76,137,226,72,184,237,
  237,252,255,208,65,187,237,133,192,15,132,244,248,255,72,137,223,190,237,
  186,237,76,137,225,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,
  248,255,72,137,223,190,237,76,137,226,72,184,237,237,252,255,208,65,187,237,
  133,192,15,132,244,248,255,72,141,187,233,72,190,237,237,72,141,147,233,72,
  184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,72,141,187,
  233,72,141,179,233,186,237,76,137,225,72,184,237,237,252,255,208,65,187,237,
  72,133,192,15,132,244,248,72,137,195,255,72,141,187,233,72,141,179,233,72,
  186,237,237,76,137,225,72,184,237,237,252,255,208,65,187,237,72,133,192,15,
  132,244,248,72,137,195,255,72,141,187,233,72,141,179,233,72,184,237,237,102,
  72,15,110,192,76,137,226,72,184,237,237,252,255,208,65,187,237,72,133,192,
  15,132,244,248,72,137,195,255,72,141,187,233,72,141,179,233,72,141,147,233,
  76,137,225,72,184,237,237,252,255,208,65,187,237,72,133,192,15,132,244,248,
  72,137,195,255,191,237,72,190,237,237,76,137,226,72,184,237,237,252,255,208,
  65,187,237,133,192,15,132,244,248,255,191,237,72,190,237,237,72,141,147,233,
  76,137,225,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,
  191,237,72,190,237,237,186,237,76,137,225,72,184,237,237,252,255,208,65,187,
  237,133,192,15,132,244,248,255,191,237,72,190,237,237,72,186,237,237,76,137,
  225,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,191,
  237,72,190,237,237,72,184,237,237,102,72,15,110,192,76,137,226,72,184,237,
  237,252,255,208,65,187,237,133,192,15,132,244,248,255,72,141,187,233,72,190,
  237,237,72,141,147,233,76,137,225,72,184,237,237,252,255,208,65,187,237,72,
  133,192,15,132,244,248,72,137,195,255,72,141,187,233,72,190,237,237,186,237,
  76,137,225,72,184,237,237,252,255,208,65,187,237,72,133,192,15,132,244,248,
  72,137,195,255,72,141,187,233,72,190,237,237,72,186,237,237,76,137,225,72,
  184,237,237,252,255,208,65,187,237,72,133,192,15,132,244,248,72,137,195,255,
  72,141,187,233,72,199,198,237,72,141,147,233,72,184,237,237,252,255,208,65,
  187,237,133,192,15,132,244,248,255,72,141,187,233,72,199,198,237,72,141,147,
  233,76,137,225,72,184,237,237,252,255,208,65,187,237,72,133,192,15,132,244,
  248,72,137,195,255,72,141,187,233,139,179,233,139,147,233,72,141,139,233,
  72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,72,141,187,
  233,72,139,179,233,252,242,15,16,131,233,72,141,147,233,72,184,237,237,252,
  255,208,65,187,237,133,192,15,132,244,248,255,72,141,187,233,252,243,15,126,
  131,233,72,139,179,233,72,141,147,233,72,184,237,237,252,255,208,65,187,237,
  133,192,15,132,244,248,255,72,141,187,233,252,242,15,16,131,233,252,242,15,
  16,139,233,72,141,179,233,72,184,237,237,252,255,208,65,187,237,133,192,15,
  132,244,248,255,72,141,187,233,252,242,15,16,131,233,72,139,179,233,72,141,
  147,233,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,
  72,141,187,233,252,243,15,126,131,233,252,243,15,126,139,233,72,141,179,233,
  72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,72,141,187,
  233,72,139,179,233,252,242,15,16,131,233,252,242,15,16,139,233,72,141,147,
  233,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,72,141,
  187,233,252,242,15,16,131,233,72,139,179,233,252,242,15,16,139,233,72,141,
  147,233,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,
  72,141,187,233,252,242,15,16,131,233,252,242,15,16,139,233,72,139,179,233,
  72,141,147,233,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,
  255,72,141,187,233,252,242,15,16,131,233,252,242,15,16,139,233,252,242,15,
  16,147,233,72,141,179,233,72,184,237,237,252,255,208,65,187,237,133,192,15,
  132,244,248,255,72,141,187,233,72,139,179,233,72,141,147,233,72,184,237,237,
  252,255,208,65,187,237,133,192,15,132,244,248,198,131,233,235,255,72,141,
  187,233,72,199,198,237,72,141,147,233,72,141,139,233,72,184,237,237,252,255,
  208,65,187,237,133,192,15,132,244,248,255,72,141,187,233,72,199,198,237,72,
  199,194,237,255,72,199,193,237,255,72,141,187,233,72,199,198,237,72,199,194,
  237,72,141,139,233,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,
  248,255,72,141,187,233,72,141,179,233,72,186,237,237,72,184,237,237,252,255,
  208,65,187,237,133,192,15,132,244,248,255,72,141,187,233,72,139,179,233,72,
  141,147,233,255,72,184,237,237,255,72,141,187,233,72,139,179,233,72,141,147,
  233,76,137,225,72,184,237,237,252,255,208,65,187,237,72,133,192,15,132,244,
  248,72,137,195,255,72,129,192,239,255,72,129,232,239,255,72,105,192,239,255,
  65,187,237,72,139,131,233,72,185,237,237,72,57,200,15,133,244,254,255,72,
  141,187,233,72,141,179,233,72,184,237,237,102,72,15,110,192,72,184,237,237,
  252,255,208,65,187,237,133,192,15,132,244,248,255,65,187,237,252,233,244,
  254,255,72,139,131,233,72,133,192,15,132,244,251,72,252,255,200,72,137,131,
  233,72,139,139,233,72,3,139,233,72,137,139,233,49,192,72,131,196,16,91,65,
  95,65,92,195,248,5,184,1,0,0,0,72,131,196,16,91,65,95,65,92,195,255,72,139,
  131,233,72,133,192,15,132,244,249,72,252,255,200,72,137,131,233,72,139,139,
  233,72,3,139,233,72,137,139,233,65,252,255,207,15,132,244,252,252,233,244,
  1,255,248,3,184,1,0,0,0,72,131,196,16,91,65,95,65,92,195,248,2,255,68,137,
  216,252,247,216,131,232,2,255,72,131,196,16,91,65,95,65,92,195,248,6,184,
  2,0,0,0,72,131,196,16,91,65,95,65,92,195,248,8,68,137,216,252,247,216,131,
  232,2,72,131,196,16,91,65,95,65,92,195,255,72,185,237,237,72,1,200,255,72,
  185,237,237,72,41,200,255,72,185,237,237,72,15,175,193,255
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
      case LUAR_OP_BAND:
      case LUAR_OP_BOR:
      case LUAR_OP_BXOR:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
        //| mov rcx, qword [rbx + ((int)LUAR_SLOT(op->c))]
        dasm_put(Dst, 249, ((int)LUAR_SLOT(b)), ((int)LUAR_SLOT(op->c)));
#line 266 "src/luar_emit_x64.dasc"
        if (op->kind == LUAR_OP_BAND) {
          //| and rax, rcx
          dasm_put(Dst, 258);
#line 268 "src/luar_emit_x64.dasc"
        }
        if (op->kind == LUAR_OP_BOR) {
          //| or rax, rcx
          dasm_put(Dst, 262);
#line 271 "src/luar_emit_x64.dasc"
        }
        if (op->kind == LUAR_OP_BXOR) {
          //| xor rax, rcx
          dasm_put(Dst, 266);
#line 274 "src/luar_emit_x64.dasc"
        }
        //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
        dasm_put(Dst, 57, ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 277 "src/luar_emit_x64.dasc"
        break;
      case LUAR_OP_BANDK:
      case LUAR_OP_BORK:
      case LUAR_OP_BXORK:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
        //| mov64 rcx, op->imm
        dasm_put(Dst, 270, ((int)LUAR_SLOT(b)), (unsigned int)(op->imm), (unsigned int)((op->imm)>>32));
#line 283 "src/luar_emit_x64.dasc"
        if (op->kind == LUAR_OP_BANDK) {
          //| and rax, rcx
          dasm_put(Dst, 258);
#line 285 "src/luar_emit_x64.dasc"
        }
        if (op->kind == LUAR_OP_BORK) {
          //| or rax, rcx
          dasm_put(Dst, 262);
#line 288 "src/luar_emit_x64.dasc"
        }
        if (op->kind == LUAR_OP_BXORK) {
          //| xor rax, rcx
          dasm_put(Dst, 266);
#line 291 "src/luar_emit_x64.dasc"
        }
        //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
        dasm_put(Dst, 57, ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 294 "src/luar_emit_x64.dasc"
        break;
      case LUAR_OP_BNOT:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
        //| not rax
        //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
        dasm_put(Dst, 279, ((int)LUAR_SLOT(b)), ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 300 "src/luar_emit_x64.dasc"
        break;
      case LUAR_OP_SHRI: {
        int64_t count = op->imm;
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
        dasm_put(Dst, 185, ((int)LUAR_SLOT(b)));
#line 304 "src/luar_emit_x64.dasc"
        if (count > 0) {
          if (count >= 64)
            //| xor eax, eax
            dasm_put(Dst, 296);
#line 307 "src/luar_emit_x64.dasc"
          else
            //| shr rax, ((int)count)
            dasm_put(Dst, 299, ((int)count));
#line 309 "src/luar_emit_x64.dasc"
        }
        else if (count < 0) {
          if (count <= -64)
            //| xor eax, eax
            dasm_put(Dst, 296);
#line 313 "src/luar_emit_x64.dasc"
          else
            //| shl rax, ((int)-count)
            dasm_put(Dst, 304, ((int)-count));
#line 315 "src/luar_emit_x64.dasc"
        }
        //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
        dasm_put(Dst, 57, ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 318 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_OP_UNM:
        if (op->ta == LUAR_T_INT) {
          //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
          //| neg rax
          //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
          //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
          dasm_put(Dst, 309, ((int)LUAR_SLOT(b)), ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 326 "src/luar_emit_x64.dasc"
        }
        else {
          /* -x flips the IEEE sign bit, preserving zeros and NaNs. */
          //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
          //| mov64 rcx, INT64_MIN
          //| xor rax, rcx
          //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
          //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_flt)
          dasm_put(Dst, 326, ((int)LUAR_SLOT(b)), (unsigned int)(INT64_MIN), (unsigned int)((INT64_MIN)>>32), ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_flt));
#line 334 "src/luar_emit_x64.dasc"
        }
        break;
      case LUAR_OP_SHL:
      case LUAR_OP_SHR: {
        /* Lua shifts by a negative count in the other direction and by
        ** |count| >= 64 to zero.  The labels are derived from the bytecode
        ** PC so a loop body may contain more than one variable shift. */
        int lneg = 4096 + op->pc * 4;
        int lzero = lneg + 1;
        int ldone = lneg + 2;
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
        //| mov rcx, qword [rbx + ((int)LUAR_SLOT(op->c))]
        //| test rcx, rcx
        //| js =>lneg
        //| cmp rcx, 64
        //| jge =>lzero
        dasm_put(Dst, 346, ((int)LUAR_SLOT(b)), ((int)LUAR_SLOT(op->c)), lneg, lzero);
#line 350 "src/luar_emit_x64.dasc"
        if (op->kind == LUAR_OP_SHL) {
          //| shl rax, cl
          dasm_put(Dst, 369);
#line 352 "src/luar_emit_x64.dasc"
        }
        else {
          //| shr rax, cl
          dasm_put(Dst, 373);
#line 355 "src/luar_emit_x64.dasc"
        }
        //| jmp =>ldone
        //|=>lneg:
        //| cmp rcx, -64
        //| jle =>lzero
        //| neg rcx
        dasm_put(Dst, 377, ldone, lneg, lzero);
#line 361 "src/luar_emit_x64.dasc"
        if (op->kind == LUAR_OP_SHL) {
          //| shr rax, cl
          dasm_put(Dst, 373);
#line 363 "src/luar_emit_x64.dasc"
        }
        else {
          //| shl rax, cl
          dasm_put(Dst, 369);
#line 366 "src/luar_emit_x64.dasc"
        }
        //| jmp =>ldone
        //|=>lzero:
        //| xor eax, eax
        //|=>ldone:
        //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
        dasm_put(Dst, 394, ldone, lzero, ldone, ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 373 "src/luar_emit_x64.dasc"
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
#line 401 "src/luar_emit_x64.dasc"
    if (b == 0xFF) {
      //| mov64 rcx, op->imm
      //| cmp rax, rcx
      dasm_put(Dst, 410, (unsigned int)(op->imm), (unsigned int)((op->imm)>>32));
#line 404 "src/luar_emit_x64.dasc"
    }
    else {
      //| cmp rax, qword [rbx + ((int)LUAR_SLOT(b))]
      dasm_put(Dst, 418, ((int)LUAR_SLOT(b)));
#line 407 "src/luar_emit_x64.dasc"
    }
    switch (op->kind) {
      case LUAR_OP_LT:
        if (k) {
          //| jl =>((int)op->target)
          dasm_put(Dst, 423, ((int)op->target));
#line 412 "src/luar_emit_x64.dasc"
        }
        else {
          //| jge =>((int)op->target)
          dasm_put(Dst, 365, ((int)op->target));
#line 415 "src/luar_emit_x64.dasc"
        }
        break;
      case LUAR_OP_LE:
        if (k) {
          //| jle =>((int)op->target)
          dasm_put(Dst, 427, ((int)op->target));
#line 420 "src/luar_emit_x64.dasc"
        }
        else {
          //| jg =>((int)op->target)
          dasm_put(Dst, 431, ((int)op->target));
#line 423 "src/luar_emit_x64.dasc"
        }
        break;
      case LUAR_OP_GT:
        if (k) {
          //| jg =>((int)op->target)
          dasm_put(Dst, 431, ((int)op->target));
#line 428 "src/luar_emit_x64.dasc"
        }
        else {
          //| jle =>((int)op->target)
          dasm_put(Dst, 427, ((int)op->target));
#line 431 "src/luar_emit_x64.dasc"
        }
        break;
      case LUAR_OP_GE:
        if (k) {
          //| jge =>((int)op->target)
          dasm_put(Dst, 365, ((int)op->target));
#line 436 "src/luar_emit_x64.dasc"
        }
        else {
          //| jl =>((int)op->target)
          dasm_put(Dst, 423, ((int)op->target));
#line 439 "src/luar_emit_x64.dasc"
        }
        break;
      case LUAR_OP_EQ:
        if (k) {
          //| je =>((int)op->target)
          dasm_put(Dst, 435, ((int)op->target));
#line 444 "src/luar_emit_x64.dasc"
        }
        else {
          //| jne =>((int)op->target)
          dasm_put(Dst, 439, ((int)op->target));
#line 447 "src/luar_emit_x64.dasc"
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
    dasm_put(Dst, 443, ((int)op->pc), ((int)LUAR_SLOT(a)), (unsigned int)(9007199254740992), (unsigned int)((9007199254740992)>>32), (unsigned int)(-9007199254740992), (unsigned int)((-9007199254740992)>>32));
#line 468 "src/luar_emit_x64.dasc"
  }
  if (op->tb == LUAR_T_INT && b == 0xFF) {
    if (op->imm > 9007199254740992LL ||
        op->imm < -9007199254740992LL) {
      //| mov r11d, ((int)op->pc)
      //| jmp >3
      dasm_put(Dst, 473, ((int)op->pc));
#line 474 "src/luar_emit_x64.dasc"
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
    dasm_put(Dst, 443, ((int)op->pc), ((int)LUAR_SLOT(b)), (unsigned int)(9007199254740992), (unsigned int)((9007199254740992)>>32), (unsigned int)(-9007199254740992), (unsigned int)((-9007199254740992)>>32));
#line 485 "src/luar_emit_x64.dasc"
  }

  /* At least one operand is a double: convert both and use ucomisd. */
  load_double(Dst, a, op->ta);
  if (b == 0xFF) {
    if (op->tb == LUAR_T_INT) {
      //| mov64 rax, op->imm
      //| cvtsi2sd xmm1, rax
      dasm_put(Dst, 481, (unsigned int)(op->imm), (unsigned int)((op->imm)>>32));
#line 493 "src/luar_emit_x64.dasc"
    }
    else {
      int64_t bits;
      memcpy(&bits, &op->num, sizeof(bits));
      //| mov64 rax, bits
      //| movq xmm1, rax
      dasm_put(Dst, 151, (unsigned int)(bits), (unsigned int)((bits)>>32));
#line 499 "src/luar_emit_x64.dasc"
    }
  }
  else if (op->tb == LUAR_T_INT) {
    //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
    //| cvtsi2sd xmm1, rax
    dasm_put(Dst, 190, ((int)LUAR_SLOT(b)));
#line 504 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd xmm1, qword [rbx + ((int)LUAR_SLOT(b))]
    dasm_put(Dst, 125, ((int)LUAR_SLOT(b)));
#line 507 "src/luar_emit_x64.dasc"
  }
  //| ucomisd xmm0, xmm1
  dasm_put(Dst, 492);
#line 509 "src/luar_emit_x64.dasc"
  /* Unordered (NaN) comparisons are false, as in Lua. */
  if (k) {
    //| jp =>((int)fallthrough)
    dasm_put(Dst, 497, ((int)fallthrough));
#line 512 "src/luar_emit_x64.dasc"
  }
  else {
    //| jp =>((int)op->target)
    dasm_put(Dst, 497, ((int)op->target));
#line 515 "src/luar_emit_x64.dasc"
  }
  switch (op->kind) {
    case LUAR_OP_LT:
      if (k) {
          //| jb =>((int)op->target)
          dasm_put(Dst, 501, ((int)op->target));
#line 520 "src/luar_emit_x64.dasc"
        }
        else {
          //| jae =>((int)op->target)
          dasm_put(Dst, 505, ((int)op->target));
#line 523 "src/luar_emit_x64.dasc"
        }
      break;
    case LUAR_OP_LE:
      if (k) {
          //| jbe =>((int)op->target)
          dasm_put(Dst, 509, ((int)op->target));
#line 528 "src/luar_emit_x64.dasc"
        }
        else {
          //| ja =>((int)op->target)
          dasm_put(Dst, 513, ((int)op->target));
#line 531 "src/luar_emit_x64.dasc"
        }
      break;
    case LUAR_OP_GT:
      if (k) {
          //| ja =>((int)op->target)
          dasm_put(Dst, 513, ((int)op->target));
#line 536 "src/luar_emit_x64.dasc"
        }
        else {
          //| jbe =>((int)op->target)
          dasm_put(Dst, 509, ((int)op->target));
#line 539 "src/luar_emit_x64.dasc"
        }
      break;
    case LUAR_OP_GE:
      if (k) {
          //| jae =>((int)op->target)
          dasm_put(Dst, 505, ((int)op->target));
#line 544 "src/luar_emit_x64.dasc"
        }
        else {
          //| jb =>((int)op->target)
          dasm_put(Dst, 501, ((int)op->target));
#line 547 "src/luar_emit_x64.dasc"
        }
      break;
    case LUAR_OP_EQ:
      if (k) {
          //| je =>((int)op->target)
          dasm_put(Dst, 435, ((int)op->target));
#line 552 "src/luar_emit_x64.dasc"
        }
        else {
          //| jne =>((int)op->target)
          dasm_put(Dst, 439, ((int)op->target));
#line 555 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 517, ((int)LUAR_SLOT(op->c)), 8, ((int)d->tag_int), ((int)LUAR_SLOT(op->a)), ((int)LUAR_TAG(op->a)), ((int)d->tag_int));
#line 577 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 569, ((int)LUAR_SLOT(op->c)), 8, ((int)d->tag_int));
#line 593 "src/luar_emit_x64.dasc"
  if (op->ta == LUAR_T_INT) {
    //| mov rax, qword [rbx + ((int)LUAR_SLOT(op->a))]
    //| add rax, rcx
    //| mov qword [rbx + ((int)LUAR_SLOT(op->a))], rax
    //| mov byte [rbx + ((int)LUAR_TAG(op->a))], ((int)d->tag_int)
    dasm_put(Dst, 613, ((int)LUAR_SLOT(op->a)), ((int)LUAR_SLOT(op->a)), ((int)LUAR_TAG(op->a)), ((int)d->tag_int));
#line 598 "src/luar_emit_x64.dasc"
  }
  else {
    //| cvtsi2sd xmm0, rcx
    //| addsd xmm0, qword [rbx + ((int)LUAR_SLOT(op->a))]
    //| movsd qword [rbx + ((int)LUAR_SLOT(op->a))], xmm0
    //| mov byte [rbx + ((int)LUAR_TAG(op->a))], ((int)d->tag_flt)
    dasm_put(Dst, 629, ((int)LUAR_SLOT(op->a)), ((int)LUAR_SLOT(op->a)), ((int)LUAR_TAG(op->a)), ((int)d->tag_flt));
#line 604 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 652, ((int)LUAR_SLOT(op->b)));
#line 618 "src/luar_emit_x64.dasc"
  if (op->ta == LUAR_T_INT) {
    //| mov rcx, qword [rbx + ((int)LUAR_SLOT(op->c))]
    //| mov qword [rax], rcx
    //| mov byte [rax + 8], ((int)d->tag_int)
    dasm_put(Dst, 685, ((int)LUAR_SLOT(op->c)), 8, ((int)d->tag_int));
#line 622 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(op->c))]
    //| movsd qword [rax], xmm0
    //| mov byte [rax + 8], ((int)d->tag_flt)
    dasm_put(Dst, 697, ((int)LUAR_SLOT(op->c)), 8, ((int)d->tag_flt));
#line 627 "src/luar_emit_x64.dasc"
  }
}

static void emit_branch_code (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  int i;
  //| push rbx
  //| mov rbx, rdi
  dasm_put(Dst, 713);
#line 635 "src/luar_emit_x64.dasc"
  if (d->has_table) {
    //| push r12
    //| push r13
    //| push r14
    dasm_put(Dst, 719);
#line 639 "src/luar_emit_x64.dasc"
  }
  //| mov r11d, ((int)d->entry_pc)
  dasm_put(Dst, 726, ((int)d->entry_pc));
#line 641 "src/luar_emit_x64.dasc"
  emit_guards(Dst, d);
  if (d->has_table) {
    //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->table_reg))]
    //| mov rax, qword [r12 + ((int)d->table_metatable_off)]
    //| test rax, rax
    //| jnz >3
    //| mov r13, qword [r12 + ((int)d->table_array_off)]
    //| mov r14d, dword [r12 + ((int)d->table_asize_off)]
    dasm_put(Dst, 730, ((int)LUAR_SLOT(d->table_reg)), ((int)d->table_metatable_off), ((int)d->table_array_off), ((int)d->table_asize_off));
#line 649 "src/luar_emit_x64.dasc"
  }
  for (i = 0; i < d->nops; i++) {
    const LuaR_Op *op = &d->ops[i];
    //|=>((int)i):
    dasm_put(Dst, 7, ((int)i));
#line 653 "src/luar_emit_x64.dasc"
    if (op->kind == LUAR_OP_GETTABLE || op->kind == LUAR_OP_TABLE_ADD ||
        op->kind == LUAR_OP_SETTABLE)
      //| mov r11d, ((int)op->pc)
      dasm_put(Dst, 726, ((int)op->pc));
#line 656 "src/luar_emit_x64.dasc"
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
          dasm_put(Dst, 760, ((int)(d->nops + 1)));
#line 667 "src/luar_emit_x64.dasc"
        else
          //| jmp =>((int)(i + 2))
          dasm_put(Dst, 760, ((int)(i + 2)));
#line 669 "src/luar_emit_x64.dasc"
        break;
      case LUAR_OP_JMP:
        //| jmp =>((int)op->target)
        dasm_put(Dst, 760, ((int)op->target));
#line 672 "src/luar_emit_x64.dasc"
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
      dasm_put(Dst, 760, 0);
#line 693 "src/luar_emit_x64.dasc"
    if (d->repeat)
      //|=>((int)(d->nops + 1)):
      dasm_put(Dst, 7, ((int)(d->nops + 1)));
#line 695 "src/luar_emit_x64.dasc"
    else
      //|=>((int)d->nops):
    //| mov eax, 1
    dasm_put(Dst, 764, ((int)d->nops));
#line 698 "src/luar_emit_x64.dasc"
    if (d->has_table) {
      //| pop r14
      //| pop r13
      //| pop r12
      dasm_put(Dst, 771);
#line 702 "src/luar_emit_x64.dasc"
    }
    //| pop rbx
    //| ret
    dasm_put(Dst, 778);
#line 705 "src/luar_emit_x64.dasc"
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
    dasm_put(Dst, 781, ((int)d->nops), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)), ((int)LUAR_SLOT(d->for_a + 2)), 0, ((int)(d->nops + 1)));
#line 720 "src/luar_emit_x64.dasc"
    if (d->has_table) {
      //| pop r14
      //| pop r13
      //| pop r12
      dasm_put(Dst, 771);
#line 724 "src/luar_emit_x64.dasc"
    }
    //| pop rbx
    //| ret
    dasm_put(Dst, 778);
#line 727 "src/luar_emit_x64.dasc"
  }
  //|3:
  //| mov eax, r11d
  //| add eax, 2
  //| neg eax
  dasm_put(Dst, 825);
#line 732 "src/luar_emit_x64.dasc"
  if (d->has_table) {
    //| pop r14
    //| pop r13
    //| pop r12
    dasm_put(Dst, 771);
#line 736 "src/luar_emit_x64.dasc"
  }
  //| pop rbx
  //| ret
  dasm_put(Dst, 778);
#line 739 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 837);
#line 750 "src/luar_emit_x64.dasc"
  if (!d->ff_is_float)
    //| push r15
    dasm_put(Dst, 849);
#line 752 "src/luar_emit_x64.dasc"
  emit_guards(Dst, d);
  //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->ff_idx))]
  //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  dasm_put(Dst, 852, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->ff_idx)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 756 "src/luar_emit_x64.dasc"
  if (d->ff_is_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->ff_acc))]
    //| mov64 rax, bits
    //| movq xmm1, rax
    dasm_put(Dst, 865, ((int)LUAR_SLOT(d->ff_acc)), (unsigned int)(bits), (unsigned int)((bits)>>32));
#line 760 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->ff_acc))]
    //| mov64 r11, d->ff_iconst
    dasm_put(Dst, 881, ((int)LUAR_SLOT(d->ff_acc)), (unsigned int)(d->ff_iconst), (unsigned int)((d->ff_iconst)>>32));
#line 764 "src/luar_emit_x64.dasc"
  }
  //|1:
  dasm_put(Dst, 890);
#line 766 "src/luar_emit_x64.dasc"
  if (d->ff_is_float) {
    //| cvtsi2sd xmm2, r13
    //| mulsd xmm2, xmm1
    dasm_put(Dst, 893);
#line 769 "src/luar_emit_x64.dasc"
    if (d->ff_temp >= 0) {
      //| movsd qword [rbx + ((int)LUAR_SLOT(d->ff_temp))], xmm2
      //| mov byte [rbx + ((int)LUAR_TAG(d->ff_temp))], ((int)d->tag_flt)
      dasm_put(Dst, 905, ((int)LUAR_SLOT(d->ff_temp)), ((int)LUAR_TAG(d->ff_temp)), ((int)d->tag_flt));
#line 772 "src/luar_emit_x64.dasc"
    }
    //| addsd xmm0, xmm2
    dasm_put(Dst, 916);
#line 774 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov rcx, r13
    //| imul rcx, r11
    dasm_put(Dst, 922);
#line 778 "src/luar_emit_x64.dasc"
    if (d->ff_temp >= 0) {
      //| mov qword [rbx + ((int)LUAR_SLOT(d->ff_temp))], rcx
      //| mov byte [rbx + ((int)LUAR_TAG(d->ff_temp))], ((int)d->tag_int)
      dasm_put(Dst, 560, ((int)LUAR_SLOT(d->ff_temp)), ((int)LUAR_TAG(d->ff_temp)), ((int)d->tag_int));
#line 781 "src/luar_emit_x64.dasc"
    }
    //| add r15, rcx
    dasm_put(Dst, 931);
#line 783 "src/luar_emit_x64.dasc"
  }
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 935, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 792 "src/luar_emit_x64.dasc"
  if (d->ff_is_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->ff_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->ff_acc)));
#line 794 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->ff_acc))], r15
    //| pop r15
    dasm_put(Dst, 965, ((int)LUAR_SLOT(d->ff_acc)));
#line 798 "src/luar_emit_x64.dasc"
  }
  //| mov eax, 1
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  //|3:
  dasm_put(Dst, 972);
#line 806 "src/luar_emit_x64.dasc"
  if (!d->ff_is_float)
    //| pop r15
  //| xor eax, eax
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 988);
#line 814 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1001, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 827 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1027, ((int)LUAR_SLOT(d->ff_table)), ((int)d->table_metatable_off), ((int)d->table_array_off), ((int)d->table_asize_off), 8, ((int)d->tag_int), ((int)LUAR_SLOT(d->for_a)));
#line 853 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1115, ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 870 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1001, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 883 "src/luar_emit_x64.dasc"
  if (d->fi_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fi_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->fi_acc)));
#line 885 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fi_acc))]
    dasm_put(Dst, 1157, ((int)LUAR_SLOT(d->fi_acc)));
#line 888 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  dasm_put(Dst, 890);
#line 891 "src/luar_emit_x64.dasc"
  if (d->fi_float) {
    //| cvtsi2sd xmm1, r13
    //| addsd xmm0, xmm1
    dasm_put(Dst, 1162);
#line 894 "src/luar_emit_x64.dasc"
  }
  else {
    //| add r15, r13
    dasm_put(Dst, 1174);
#line 897 "src/luar_emit_x64.dasc"
  }
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 935, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 906 "src/luar_emit_x64.dasc"
  if (d->fi_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fi_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fi_acc)));
#line 908 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fi_acc))], r15
    dasm_put(Dst, 1179, ((int)LUAR_SLOT(d->fi_acc)));
#line 911 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1119, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 929 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1184, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)), ((int)LUAR_SLOT(d->fm_acc)));
#line 945 "src/luar_emit_x64.dasc"
  emit_mul_imm(Dst, d->fm_mult);
  if (d->fm_res >= 0) {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fm_res))], rax
    //| mov byte [rbx + ((int)LUAR_TAG(d->fm_res))], ((int)d->tag_int)
    dasm_put(Dst, 57, ((int)LUAR_SLOT(d->fm_res)), ((int)LUAR_TAG(d->fm_res)), ((int)d->tag_int));
#line 949 "src/luar_emit_x64.dasc"
  }
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
  dasm_put(Dst, 1219, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->fm_acc)));
#line 975 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1285, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)), ((int)LUAR_SLOT(d->fc_upval_slot)), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->fc_upval_slot)));
#line 1016 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1001, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->ft_idx)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 1031 "src/luar_emit_x64.dasc"
  if (!isf) {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->ft_acc))]
    dasm_put(Dst, 1157, ((int)LUAR_SLOT(d->ft_acc)));
#line 1033 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->ft_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->ft_acc)));
#line 1036 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //| mov r8, qword [rbx + ((int)LUAR_SLOT(d->ft_table))]
  //| mov rax, qword [r8 + ((int)d->table_metatable_off)]
  //| test rax, rax
  //| jnz >3
  //| mov r9, qword [r8 + ((int)d->table_array_off)]
  //| mov r10d, dword [r8 + ((int)d->table_asize_off)]
  dasm_put(Dst, 1383, ((int)LUAR_SLOT(d->ft_table)), ((int)d->table_metatable_off), ((int)d->table_array_off), ((int)d->table_asize_off));
#line 1044 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1407, 8, ((int)d->tag_int));
#line 1070 "src/luar_emit_x64.dasc"
  if (!isf) {
    //| add r15, qword [r11]
    dasm_put(Dst, 1500);
#line 1072 "src/luar_emit_x64.dasc"
  }
  else {
    //| cvtsi2sd xmm1, qword [r11]
    //| addsd xmm0, xmm1
    dasm_put(Dst, 1504);
#line 1076 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1516, d->fw_step, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 1086 "src/luar_emit_x64.dasc"
  if (!isf) {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->ft_acc))], r15
    dasm_put(Dst, 1179, ((int)LUAR_SLOT(d->ft_acc)));
#line 1088 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->ft_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->ft_acc)));
#line 1091 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1550, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 1102 "src/luar_emit_x64.dasc"
  if (!isf) {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->ft_acc))], r15
    dasm_put(Dst, 1179, ((int)LUAR_SLOT(d->ft_acc)));
#line 1104 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->ft_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->ft_acc)));
#line 1107 "src/luar_emit_x64.dasc"
  }
  //| xor eax, eax
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 1144);
#line 1115 "src/luar_emit_x64.dasc"
}

static void emit_fast_while (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  int is_lt = (d->fw_kind == LUAR_OP_LT);
  //| push rbx
  //| mov rbx, rdi
  //| push r12
  //| push r13
  //| push r14
  dasm_put(Dst, 837);
#line 1125 "src/luar_emit_x64.dasc"
  if (d->fw_float) {
    union { double d; uint64_t u; } cv;
    cv.d = d->fw_const;
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fw_acc))]
    //| mov64 rax, cv.u
    //| movq xmm2, rax
    dasm_put(Dst, 1576, ((int)LUAR_SLOT(d->fw_acc)), (unsigned int)(cv.u), (unsigned int)((cv.u)>>32));
#line 1131 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->fw_acc))]
    dasm_put(Dst, 1592, ((int)LUAR_SLOT(d->fw_acc)));
#line 1134 "src/luar_emit_x64.dasc"
  }
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->fw_idx))]
  dasm_put(Dst, 1597, ((int)LUAR_SLOT(d->fw_idx)));
#line 1136 "src/luar_emit_x64.dasc"
  if (d->fw_limit == 0xFF) {
    //| mov64 r14, d->fw_imm
    dasm_put(Dst, 1602, (unsigned int)(d->fw_imm), (unsigned int)((d->fw_imm)>>32));
#line 1138 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->fw_limit))]
    dasm_put(Dst, 860, ((int)LUAR_SLOT(d->fw_limit)));
#line 1141 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  //| cmp r13, r14
  dasm_put(Dst, 1607);
#line 1145 "src/luar_emit_x64.dasc"
  if (is_lt) {
    if (d->fw_k) {
      //| jl >2
      dasm_put(Dst, 1614);
#line 1148 "src/luar_emit_x64.dasc"
    }
    else {
      //| jge >2
      dasm_put(Dst, 1619);
#line 1151 "src/luar_emit_x64.dasc"
    }
  }
  else {
    if (d->fw_k) {
      //| jle >2
      dasm_put(Dst, 1624);
#line 1156 "src/luar_emit_x64.dasc"
    }
    else {
      //| jg >2
      dasm_put(Dst, 1629);
#line 1159 "src/luar_emit_x64.dasc"
    }
  }
  if (d->fw_float) {
    //| cvtsi2sd xmm1, r13
    dasm_put(Dst, 1634);
#line 1163 "src/luar_emit_x64.dasc"
    if (d->fw_prod) {
      //| mulsd xmm0, xmm1
      dasm_put(Dst, 173);
#line 1165 "src/luar_emit_x64.dasc"
    }
    else if (d->fw_sub) {
      //| subsd xmm0, xmm1
      dasm_put(Dst, 167);
#line 1168 "src/luar_emit_x64.dasc"
    }
    else if (d->fw_div) {
      //| divsd xmm0, xmm1
      dasm_put(Dst, 179);
#line 1171 "src/luar_emit_x64.dasc"
    }
    else if (d->fw_square) {
      //| mov rcx, r13
      //| imul rcx, r13
      dasm_put(Dst, 1641);
#line 1175 "src/luar_emit_x64.dasc"
      if (d->fw_temp >= 0) {
        //| mov qword [rbx + ((int)LUAR_SLOT(d->fw_temp))], rcx
        //| mov byte [rbx + ((int)LUAR_TAG(d->fw_temp))], ((int)d->tag_int)
        dasm_put(Dst, 560, ((int)LUAR_SLOT(d->fw_temp)), ((int)LUAR_TAG(d->fw_temp)), ((int)d->tag_int));
#line 1178 "src/luar_emit_x64.dasc"
      }
      //| cvtsi2sd xmm1, rcx
      //| addsd xmm0, xmm1
      dasm_put(Dst, 1650);
#line 1181 "src/luar_emit_x64.dasc"
    }
    else if (d->fw_linear) {
      //| mulsd xmm1, xmm2
      dasm_put(Dst, 1662);
#line 1184 "src/luar_emit_x64.dasc"
      if (d->fw_temp >= 0) {
        //| movsd qword [rbx + ((int)LUAR_SLOT(d->fw_temp))], xmm1
        //| mov byte [rbx + ((int)LUAR_TAG(d->fw_temp))], ((int)d->tag_flt)
        dasm_put(Dst, 1668, ((int)LUAR_SLOT(d->fw_temp)), ((int)LUAR_TAG(d->fw_temp)), ((int)d->tag_flt));
#line 1187 "src/luar_emit_x64.dasc"
      }
      //| addsd xmm0, xmm1
      dasm_put(Dst, 161);
#line 1189 "src/luar_emit_x64.dasc"
    }
    else {
      //| addsd xmm0, xmm1
      dasm_put(Dst, 161);
#line 1192 "src/luar_emit_x64.dasc"
    }
  }
  else if (d->fw_prod) {
    //| imul r12, r13
    dasm_put(Dst, 1679);
#line 1196 "src/luar_emit_x64.dasc"
  }
  else if (d->fw_sub) {
    //| sub r12, r13
    dasm_put(Dst, 1684);
#line 1199 "src/luar_emit_x64.dasc"
  }
  else if (d->fw_square) {
    //| mov rcx, r13
    //| imul rcx, r13
    dasm_put(Dst, 1641);
#line 1203 "src/luar_emit_x64.dasc"
    if (d->fw_temp >= 0) {
      //| mov qword [rbx + ((int)LUAR_SLOT(d->fw_temp))], rcx
      //| mov byte [rbx + ((int)LUAR_TAG(d->fw_temp))], ((int)d->tag_int)
      dasm_put(Dst, 560, ((int)LUAR_SLOT(d->fw_temp)), ((int)LUAR_TAG(d->fw_temp)), ((int)d->tag_int));
#line 1206 "src/luar_emit_x64.dasc"
    }
    //| add r12, rcx
    dasm_put(Dst, 1689);
#line 1208 "src/luar_emit_x64.dasc"
  }
  else {
    //| add r12, r13
    dasm_put(Dst, 1693);
#line 1211 "src/luar_emit_x64.dasc"
  }
  //| add r13, d->fw_step
  //| jmp <1
  //|2:
  dasm_put(Dst, 1698, d->fw_step);
#line 1215 "src/luar_emit_x64.dasc"
  if (d->fw_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fw_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fw_acc)));
#line 1217 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fw_acc))], r12
    dasm_put(Dst, 1110, ((int)LUAR_SLOT(d->fw_acc)));
#line 1220 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1709, ((int)LUAR_SLOT(d->fw_idx)));
#line 1235 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1001, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 1248 "src/luar_emit_x64.dasc"
  if (d->fs_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fs_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->fs_acc)));
#line 1250 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fs_acc))]
    dasm_put(Dst, 1157, ((int)LUAR_SLOT(d->fs_acc)));
#line 1253 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  //| mov rcx, r13
  //| imul rcx, r13
  dasm_put(Dst, 1739);
#line 1258 "src/luar_emit_x64.dasc"
  if (d->fs_temp >= 0) {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fs_temp))], rcx
    //| mov byte [rbx + ((int)LUAR_TAG(d->fs_temp))], ((int)d->tag_int)
    dasm_put(Dst, 560, ((int)LUAR_SLOT(d->fs_temp)), ((int)LUAR_TAG(d->fs_temp)), ((int)d->tag_int));
#line 1261 "src/luar_emit_x64.dasc"
  }
  if (d->fs_float) {
    //| cvtsi2sd xmm1, rcx
    //| addsd xmm0, xmm1
    dasm_put(Dst, 1650);
#line 1265 "src/luar_emit_x64.dasc"
  }
  else {
    //| add r15, rcx
    dasm_put(Dst, 931);
#line 1268 "src/luar_emit_x64.dasc"
  }
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 935, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 1277 "src/luar_emit_x64.dasc"
  if (d->fs_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fs_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fs_acc)));
#line 1279 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fs_acc))], r15
    dasm_put(Dst, 1179, ((int)LUAR_SLOT(d->fs_acc)));
#line 1282 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1255);
#line 1298 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1001, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 1311 "src/luar_emit_x64.dasc"
  if (d->fp_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fp_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->fp_acc)));
#line 1313 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fp_acc))]
    dasm_put(Dst, 1157, ((int)LUAR_SLOT(d->fp_acc)));
#line 1316 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  dasm_put(Dst, 890);
#line 1319 "src/luar_emit_x64.dasc"
  if (d->fp_float) {
    //| cvtsi2sd xmm1, r13
    dasm_put(Dst, 1634);
#line 1321 "src/luar_emit_x64.dasc"
    if (d->fp_div)
      //| divsd xmm0, xmm1
      dasm_put(Dst, 179);
#line 1323 "src/luar_emit_x64.dasc"
    else
      //| mulsd xmm0, xmm1
      dasm_put(Dst, 173);
#line 1325 "src/luar_emit_x64.dasc"
  }
  else {
    //| imul r15, r13
    dasm_put(Dst, 1750);
#line 1328 "src/luar_emit_x64.dasc"
  }
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 935, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 1337 "src/luar_emit_x64.dasc"
  if (d->fp_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fp_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fp_acc)));
#line 1339 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fp_acc))], r15
    dasm_put(Dst, 1179, ((int)LUAR_SLOT(d->fp_acc)));
#line 1342 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1255);
#line 1358 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1001, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 1371 "src/luar_emit_x64.dasc"
  if (d->fsb_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fsb_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->fsb_acc)));
#line 1373 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fsb_acc))]
    dasm_put(Dst, 1157, ((int)LUAR_SLOT(d->fsb_acc)));
#line 1376 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  dasm_put(Dst, 890);
#line 1379 "src/luar_emit_x64.dasc"
  if (d->fsb_float) {
    //| cvtsi2sd xmm1, r13
    //| subsd xmm0, xmm1
    dasm_put(Dst, 1756);
#line 1382 "src/luar_emit_x64.dasc"
  }
  else {
    //| sub r15, r13
    dasm_put(Dst, 1768);
#line 1385 "src/luar_emit_x64.dasc"
  }
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 935, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 1394 "src/luar_emit_x64.dasc"
  if (d->fsb_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fsb_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fsb_acc)));
#line 1396 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fsb_acc))], r15
    dasm_put(Dst, 1179, ((int)LUAR_SLOT(d->fsb_acc)));
#line 1399 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1255);
#line 1415 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1773, ((int)LUAR_SLOT(d->fr_idx)));
#line 1426 "src/luar_emit_x64.dasc"
  if (d->fr_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fr_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->fr_acc)));
#line 1428 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fr_acc))]
    dasm_put(Dst, 1157, ((int)LUAR_SLOT(d->fr_acc)));
#line 1431 "src/luar_emit_x64.dasc"
  }
  if (d->fr_limit != 0xFF) {
    //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->fr_limit))]
    dasm_put(Dst, 860, ((int)LUAR_SLOT(d->fr_limit)));
#line 1434 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov64 r14, d->fr_immediate
    dasm_put(Dst, 1602, (unsigned int)(d->fr_immediate), (unsigned int)((d->fr_immediate)>>32));
#line 1437 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  //| add r13, d->fr_step
  dasm_put(Dst, 1789, d->fr_step);
#line 1441 "src/luar_emit_x64.dasc"
  if (d->fr_float) {
    //| cvtsi2sd xmm1, r13
    dasm_put(Dst, 1634);
#line 1443 "src/luar_emit_x64.dasc"
    if (d->fr_op == 1) {
      //| subsd xmm0, xmm1
      dasm_put(Dst, 167);
#line 1445 "src/luar_emit_x64.dasc"
    }
    else if (d->fr_op == 2) {
      //| mulsd xmm0, xmm1
      dasm_put(Dst, 173);
#line 1448 "src/luar_emit_x64.dasc"
    }
    else if (d->fr_op == 3) {
      //| mov rcx, r13
      //| imul rcx, r13
      dasm_put(Dst, 1641);
#line 1452 "src/luar_emit_x64.dasc"
      if (d->fr_temp >= 0) {
        //| mov qword [rbx + ((int)LUAR_SLOT(d->fr_temp))], rcx
        //| mov byte [rbx + ((int)LUAR_TAG(d->fr_temp))], ((int)d->tag_int)
        dasm_put(Dst, 560, ((int)LUAR_SLOT(d->fr_temp)), ((int)LUAR_TAG(d->fr_temp)), ((int)d->tag_int));
#line 1455 "src/luar_emit_x64.dasc"
      }
      //| cvtsi2sd xmm1, rcx
      //| addsd xmm0, xmm1
      dasm_put(Dst, 1650);
#line 1458 "src/luar_emit_x64.dasc"
    }
    else if (d->fr_op == 4) {
      //| divsd xmm0, xmm1
      dasm_put(Dst, 179);
#line 1461 "src/luar_emit_x64.dasc"
    }
    else {
      //| addsd xmm0, xmm1
      dasm_put(Dst, 161);
#line 1464 "src/luar_emit_x64.dasc"
    }
  }
  else if (d->fr_op == 1) {
    //| sub r15, r13
    dasm_put(Dst, 1768);
#line 1468 "src/luar_emit_x64.dasc"
  }
  else if (d->fr_op == 2) {
    //| imul r15, r13
    dasm_put(Dst, 1750);
#line 1471 "src/luar_emit_x64.dasc"
  }
  else if (d->fr_op == 3) {
    //| mov rcx, r13
    //| imul rcx, r13
    dasm_put(Dst, 1641);
#line 1475 "src/luar_emit_x64.dasc"
    if (d->fr_temp >= 0) {
      //| mov qword [rbx + ((int)LUAR_SLOT(d->fr_temp))], rcx
      //| mov byte [rbx + ((int)LUAR_TAG(d->fr_temp))], ((int)d->tag_int)
      dasm_put(Dst, 560, ((int)LUAR_SLOT(d->fr_temp)), ((int)LUAR_TAG(d->fr_temp)), ((int)d->tag_int));
#line 1478 "src/luar_emit_x64.dasc"
    }
    //| add r15, rcx
    dasm_put(Dst, 931);
#line 1480 "src/luar_emit_x64.dasc"
  }
  else {
    //| add r15, r13
    dasm_put(Dst, 1174);
#line 1483 "src/luar_emit_x64.dasc"
  }
  if (d->fr_idx_left) {
    //| cmp r13, r14
    dasm_put(Dst, 1609);
#line 1486 "src/luar_emit_x64.dasc"
  }
  else {
    //| cmp r14, r13
    dasm_put(Dst, 1796);
#line 1489 "src/luar_emit_x64.dasc"
  }
  if (kind == LUAR_OP_LT) {
    if (k) {
      //| jl <1
      dasm_put(Dst, 1801);
#line 1493 "src/luar_emit_x64.dasc"
    }
    else {
      //| jge <1
      dasm_put(Dst, 1806);
#line 1496 "src/luar_emit_x64.dasc"
    }
  }
  else if (kind == LUAR_OP_LE) {
    if (k) {
      //| jle <1
      dasm_put(Dst, 1811);
#line 1501 "src/luar_emit_x64.dasc"
    }
    else {
      //| jg <1
      dasm_put(Dst, 1816);
#line 1504 "src/luar_emit_x64.dasc"
    }
  }
  else if (kind == LUAR_OP_GT) {
    if (k) {
      //| jg <1
      dasm_put(Dst, 1816);
#line 1509 "src/luar_emit_x64.dasc"
    }
    else {
      //| jle <1
      dasm_put(Dst, 1811);
#line 1512 "src/luar_emit_x64.dasc"
    }
  }
  else if (kind == LUAR_OP_GE) {
    if (k) {
      //| jge <1
      dasm_put(Dst, 1806);
#line 1517 "src/luar_emit_x64.dasc"
    }
    else {
      //| jl <1
      dasm_put(Dst, 1801);
#line 1520 "src/luar_emit_x64.dasc"
    }
  }
  else {
    if (k) {
      //| je <1
      dasm_put(Dst, 1821);
#line 1525 "src/luar_emit_x64.dasc"
    }
    else {
      //| jne <1
      dasm_put(Dst, 1826);
#line 1528 "src/luar_emit_x64.dasc"
    }
  }
  //| mov qword [rbx + ((int)LUAR_SLOT(d->fr_idx))], r13
  dasm_put(Dst, 960, ((int)LUAR_SLOT(d->fr_idx)));
#line 1531 "src/luar_emit_x64.dasc"
  if (d->fr_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fr_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fr_acc)));
#line 1533 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fr_acc))], r15
    dasm_put(Dst, 1179, ((int)LUAR_SLOT(d->fr_acc)));
#line 1536 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1831);
#line 1551 "src/luar_emit_x64.dasc"
}

static void emit_partial_code (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  //| push rbx
  //| mov rbx, rdi
  dasm_put(Dst, 713);
#line 1557 "src/luar_emit_x64.dasc"
  emit_guards(Dst, d);
  emit_body(Dst, d);
  //| mov eax, -((int)d->exit_pc + 2)
  //| pop rbx
  //| ret
  //|3:
  //| xor eax, eax
  //| pop rbx
  //| ret
  dasm_put(Dst, 1859, -((int)d->exit_pc + 2));
#line 1566 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 713);
#line 1624 "src/luar_emit_x64.dasc"
  emit_guards(Dst, d);
  //|1:
  dasm_put(Dst, 890);
#line 1626 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1870, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 1647 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1921, ((int)t->startpc));
#line 1661 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 1951, ((int)in->pc), ((int)LUAR_TAG(in->a)), tag);
#line 1675 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_LOADI:
        //| mov64 rax, in->ival
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMINT
        dasm_put(Dst, 53, (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 1681 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_KBOOL:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov esi, ((int)in->nval)
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_kbool
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1963, ((int)LUAR_SLOT(in->a)), ((int)in->nval), (unsigned int)((int64_t)(intptr_t)&luar_trace_kbool), (unsigned int)(((int64_t)(intptr_t)&luar_trace_kbool)>>32), ((int)in->pc));
#line 1690 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 1986, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_kstr), (unsigned int)(((int64_t)(intptr_t)&luar_trace_kstr)>>32), ((int)in->pc));
#line 1700 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_LOADF: {
        int64_t bits;
        memcpy(&bits, &in->nval, sizeof(bits));
        //| mov64 rax, bits
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMFLT
        dasm_put(Dst, 53, (unsigned int)(bits), (unsigned int)((bits)>>32), ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->a)), LUA_VNUMFLT);
#line 1707 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_ADDI:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(in->b))]
        //| add rax, in->ival
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMINT
        dasm_put(Dst, 72, ((int)LUAR_SLOT(in->b)), in->ival, ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 1714 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_SLOAD:
      case LUAR_IR_SSTORE:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov al, byte [rbx + ((int)LUAR_TAG(in->b))]
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], al
        dasm_put(Dst, 38, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->b)), ((int)LUAR_TAG(in->a)));
#line 1721 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2014, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)in->nval), (unsigned int)((int64_t)(intptr_t)&luar_trace_uload), (unsigned int)(((int64_t)(intptr_t)&luar_trace_uload)>>32), ((int)in->pc));
#line 1732 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_ADD:
      case LUAR_IR_SUB:
      case LUAR_IR_MUL:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(in->b))]
        dasm_put(Dst, 185, ((int)LUAR_SLOT(in->b)));
#line 1737 "src/luar_emit_x64.dasc"
        if (in->kind == LUAR_IR_ADD) {
          //| add rax, qword [rbx + ((int)LUAR_SLOT(in->c))]
          dasm_put(Dst, 233, ((int)LUAR_SLOT(in->c)));
#line 1739 "src/luar_emit_x64.dasc"
        }
        else if (in->kind == LUAR_IR_SUB) {
          //| sub rax, qword [rbx + ((int)LUAR_SLOT(in->c))]
          dasm_put(Dst, 238, ((int)LUAR_SLOT(in->c)));
#line 1742 "src/luar_emit_x64.dasc"
        }
        else {
          //| imul rax, qword [rbx + ((int)LUAR_SLOT(in->c))]
          dasm_put(Dst, 243, ((int)LUAR_SLOT(in->c)));
#line 1745 "src/luar_emit_x64.dasc"
        }
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMINT
        dasm_put(Dst, 57, ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 1748 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2044, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_cget_double), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_cget_double)>>32), ((int)in->pc));
#line 1758 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2073, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall0_double), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall0_double)>>32), ((int)in->pc));
#line 1769 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL0I:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall0_int
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2073, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall0_int), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall0_int)>>32), ((int)in->pc));
#line 1778 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2098, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall1di), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall1di)>>32), ((int)in->pc));
#line 1788 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2127, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall1dd), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall1dd)>>32), ((int)in->pc));
#line 1798 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2127, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall1ff), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall1ff)>>32), ((int)in->pc));
#line 1808 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2158, ((int)LUAR_SLOT(in->a)), ((int)in->ival), ((int)in->nval), (unsigned int)((int64_t)(intptr_t)&luar_trace_tnew), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tnew)>>32), ((int)in->pc));
#line 1819 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2186, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tlen), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tlen)>>32), ((int)in->pc));
#line 1829 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_STRLEN:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_strlen
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2073, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_strlen), (unsigned int)(((int64_t)(intptr_t)&luar_trace_strlen)>>32), ((int)in->pc));
#line 1838 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2214, ((int)in->a), ((int)in->ival), (unsigned int)((int64_t)(intptr_t)&luar_trace_concatn), (unsigned int)(((int64_t)(intptr_t)&luar_trace_concatn)>>32), ((int)in->pc));
#line 1849 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2241, ((int)in->a), (unsigned int)((int64_t)(intptr_t)&luar_trace_concat3), (unsigned int)(((int64_t)(intptr_t)&luar_trace_concat3)>>32), ((int)in->pc));
#line 1859 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2241, ((int)in->a), (unsigned int)((int64_t)(intptr_t)&luar_trace_concat2), (unsigned int)(((int64_t)(intptr_t)&luar_trace_concat2)>>32), ((int)in->pc));
#line 1869 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2266, ((int)LUAR_SLOT(in->b)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetkt), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetkt)>>32), ((int)in->pc));
#line 1879 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2266, ((int)LUAR_SLOT(in->b)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetkb), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetkb)>>32), ((int)in->pc));
#line 1889 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2266, ((int)LUAR_SLOT(in->b)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetks), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetks)>>32), ((int)in->pc));
#line 1899 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2266, ((int)LUAR_SLOT(in->b)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetkf), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetkf)>>32), ((int)in->pc));
#line 1909 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2266, ((int)LUAR_SLOT(in->b)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetk)>>32), ((int)in->pc));
#line 1919 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2295, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)in->ival), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetdynkb), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetdynkb)>>32), ((int)in->pc));
#line 1931 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2329, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)bits), (unsigned int)(((int64_t)bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetdynki), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetdynki)>>32), ((int)in->pc));
#line 1945 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2329, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)bits), (unsigned int)(((int64_t)bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetdynks), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetdynks)>>32), ((int)in->pc));
#line 1960 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2365, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(bits), (unsigned int)((bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetdynkf), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetdynkf)>>32), ((int)in->pc));
#line 1976 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2406, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetdyn), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetdyn)>>32), ((int)in->pc));
#line 1989 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2442, ((int)in->a), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetupkn), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetupkn)>>32), ((int)in->pc));
#line 1999 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2468, ((int)in->a), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetupv), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetupv)>>32), ((int)in->pc));
#line 2010 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2468, ((int)in->a), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetupt), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetupt)>>32), ((int)in->pc));
#line 2021 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2498, ((int)in->a), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)in->nval), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetupkb), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetupkb)>>32), ((int)in->pc));
#line 2032 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2526, ((int)in->a), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)bits), (unsigned int)(((int64_t)bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetupks), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetupks)>>32), ((int)in->pc));
#line 2045 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2526, ((int)in->a), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)bits), (unsigned int)(((int64_t)bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetupki), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetupki)>>32), ((int)in->pc));
#line 2059 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2556, ((int)in->a), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)(bits), (unsigned int)((bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetupkf), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetupkf)>>32), ((int)in->pc));
#line 2074 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2591, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkdb), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkdb)>>32), ((int)in->pc));
#line 2087 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2627, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)in->nval), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkb), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkb)>>32), ((int)in->pc));
#line 2099 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2591, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkt), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkt)>>32), ((int)in->pc));
#line 2111 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2591, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetks), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetks)>>32), ((int)in->pc));
#line 2123 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2661, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)bits), (unsigned int)(((int64_t)bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkks), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkks)>>32), ((int)in->pc));
#line 2137 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2661, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)bits), (unsigned int)(((int64_t)bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkkf), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkkf)>>32), ((int)in->pc));
#line 2152 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2591, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkf), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkf)>>32), ((int)in->pc));
#line 2165 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2661, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)in->nval), (unsigned int)(((int64_t)in->nval)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkk)>>32), ((int)in->pc));
#line 2177 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2591, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetk)>>32), ((int)in->pc));
#line 2189 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2697, ((int)LUAR_SLOT(in->b)), in->ival, ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgeti), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgeti)>>32), ((int)in->pc));
#line 2199 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2591, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)fn), (unsigned int)(((int64_t)(intptr_t)fn)>>32), ((int)in->pc));
#line 2221 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2726, ((int)LUAR_SLOT(in->a)), in->ival, ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tseti), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tseti)>>32), ((int)in->pc));
#line 2234 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2762, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2ii), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2ii)>>32), ((int)in->pc));
#line 2245 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2793, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2id), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2id)>>32), ((int)in->pc));
#line 2256 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2828, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2di), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2di)>>32), ((int)in->pc));
#line 2267 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2863, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2df), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2df)>>32), ((int)in->pc));
#line 2278 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2863, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2fd), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2fd)>>32), ((int)in->pc));
#line 2289 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2793, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2if), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2if)>>32), ((int)in->pc));
#line 2300 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2900, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2fi), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2fi)>>32), ((int)in->pc));
#line 2311 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2935, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2ff), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2ff)>>32), ((int)in->pc));
#line 2322 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2863, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2dd), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2dd)>>32), ((int)in->pc));
#line 2333 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2972, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(a2)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(a3)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall3idd), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall3idd)>>32), ((int)in->pc));
#line 2347 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3013, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(a2)), ((int)LUAR_SLOT(a3)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall3did), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall3did)>>32), ((int)in->pc));
#line 2362 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3054, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(a2)), ((int)LUAR_SLOT(a3)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall3ddi), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall3ddi)>>32), ((int)in->pc));
#line 2377 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3095, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(a2)), ((int)LUAR_SLOT(a3)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall3dd), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall3dd)>>32), ((int)in->pc));
#line 2392 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3138, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall1i), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall1i)>>32), ((int)in->pc), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 2404 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3171, ((int)LUAR_SLOT(in->a)), ((int64_t)in->ival), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)setfn), (unsigned int)(((int64_t)(intptr_t)setfn)>>32), ((int)in->pc));
#line 2420 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3171, ((int)LUAR_SLOT(in->b)), ((int64_t)in->ival), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)getfn), (unsigned int)(((int64_t)(intptr_t)getfn)>>32), ((int)in->pc));
#line 2439 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3204, ((int)LUAR_SLOT(in->a)), ((int64_t)in->ival), ((int64_t)in->b));
#line 2451 "src/luar_emit_x64.dasc"
        if (isf) {
          //| mov64 rax, bits
          //| movq xmm0, rax
          dasm_put(Dst, 89, (unsigned int)(bits), (unsigned int)((bits)>>32));
#line 2454 "src/luar_emit_x64.dasc"
        }
        else {
          //| mov rcx, ((int64_t)in->nval)
          dasm_put(Dst, 3217, ((int64_t)in->nval));
#line 2457 "src/luar_emit_x64.dasc"
        }
        //| mov64 rax, (int64_t)(intptr_t)cfn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1969, (unsigned int)((int64_t)(intptr_t)cfn), (unsigned int)(((int64_t)(intptr_t)cfn)>>32), ((int)in->pc));
#line 2463 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3222, ((int)LUAR_SLOT(in->a)), ((int64_t)in->ival), ((int64_t)in->nval), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)setfn), (unsigned int)(((int64_t)(intptr_t)setfn)>>32), ((int)in->pc));
#line 2478 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3222, ((int)LUAR_SLOT(in->b)), ((int64_t)in->ival), ((int64_t)in->nval), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)getfn), (unsigned int)(((int64_t)(intptr_t)getfn)>>32), ((int)in->pc));
#line 2493 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2044, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_cset_double), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_cset_double)>>32), ((int)in->pc));
#line 2504 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2044, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_cset_int), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_cset_int)>>32), ((int)in->pc));
#line 2514 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2044, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_set_i64), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_set_i64)>>32), ((int)in->pc));
#line 2524 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3255, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(divisor), (unsigned int)((divisor)>>32), (unsigned int)((int64_t)(intptr_t)fn), (unsigned int)(((int64_t)(intptr_t)fn)>>32), ((int)in->pc));
#line 2539 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3255, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(factor), (unsigned int)((factor)>>32), (unsigned int)((int64_t)(intptr_t)fn), (unsigned int)(((int64_t)(intptr_t)fn)>>32), ((int)in->pc));
#line 2558 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3255, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(count), (unsigned int)((count)>>32), (unsigned int)((int64_t)(intptr_t)fn), (unsigned int)(((int64_t)(intptr_t)fn)>>32), ((int)in->pc));
#line 2574 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2073, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)fn), (unsigned int)(((int64_t)(intptr_t)fn)>>32), ((int)in->pc));
#line 2588 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3255, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(divisor), (unsigned int)((divisor)>>32), (unsigned int)((int64_t)(intptr_t)fn), (unsigned int)(((int64_t)(intptr_t)fn)>>32), ((int)in->pc));
#line 2604 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3255, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(factor), (unsigned int)((factor)>>32), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_mul_i64), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_mul_i64)>>32), ((int)in->pc));
#line 2616 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3255, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(delta), (unsigned int)((delta)>>32), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_add_i64), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_add_i64)>>32), ((int)in->pc));
#line 2628 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2044, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_copy_i64), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_copy_i64)>>32), ((int)in->pc));
#line 2639 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2044, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_cget_int), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_cget_int)>>32), ((int)in->pc));
#line 2649 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TGET:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rsi, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        dasm_put(Dst, 3284, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)));
#line 2654 "src/luar_emit_x64.dasc"
        if (side) {
          //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tget_generic
          dasm_put(Dst, 3297, (unsigned int)((int64_t)(intptr_t)&luar_trace_tget_generic), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tget_generic)>>32));
#line 2656 "src/luar_emit_x64.dasc"
        }
        else {
          //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tget_int
          dasm_put(Dst, 3297, (unsigned int)((int64_t)(intptr_t)&luar_trace_tget_int), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tget_int)>>32));
#line 2659 "src/luar_emit_x64.dasc"
        }
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMINT
        dasm_put(Dst, 3154, ((int)in->pc), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 2665 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3302, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)tsetfn), (unsigned int)(((int64_t)(intptr_t)tsetfn)>>32), ((int)in->pc));
#line 2678 "src/luar_emit_x64.dasc"
        /* GC may have relocated the stack. */
        break;
      }
      case LUAR_IR_ADDK:
      case LUAR_IR_SUBK:
      case LUAR_IR_MULK:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(in->b))]
        dasm_put(Dst, 185, ((int)LUAR_SLOT(in->b)));
#line 2685 "src/luar_emit_x64.dasc"
        if (in->kind == LUAR_IR_ADDK) {
          //| add rax, in->ival
          dasm_put(Dst, 3338, in->ival);
#line 2687 "src/luar_emit_x64.dasc"
        }
        else if (in->kind == LUAR_IR_SUBK) {
          //| sub rax, in->ival
          dasm_put(Dst, 3343, in->ival);
#line 2690 "src/luar_emit_x64.dasc"
        }
        else {
          //| imul rax, in->ival
          dasm_put(Dst, 3348, in->ival);
#line 2693 "src/luar_emit_x64.dasc"
        }
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMINT
        dasm_put(Dst, 57, ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 2696 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CGUARD:
        //| mov r11d, ((int)in->pc)
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rcx, (int64_t)(in->ival)
        //| cmp rax, rcx
        //| jne >8
        dasm_put(Dst, 3353, ((int)in->pc), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(in->ival)), (unsigned int)(((int64_t)(in->ival))>>32));
#line 2703 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3138, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetf_generic), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetf_generic)>>32), ((int)in->pc), ((int)LUAR_TAG(in->a)), LUA_VNUMFLT);
#line 2714 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_ITOF:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_itof
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2073, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_itof), (unsigned int)(((int64_t)(intptr_t)&luar_trace_itof)>>32), ((int)in->pc));
#line 2723 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3372, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(bits), (unsigned int)((bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_faddk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_faddk)>>32), ((int)in->pc));
#line 2736 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3372, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(bits), (unsigned int)((bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_fmulk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_fmulk)>>32), ((int)in->pc));
#line 2750 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2044, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_trace_fadd), (unsigned int)(((int64_t)(intptr_t)&luar_trace_fadd)>>32), ((int)in->pc));
#line 2761 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2044, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_trace_fsub), (unsigned int)(((int64_t)(intptr_t)&luar_trace_fsub)>>32), ((int)in->pc));
#line 2771 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3372, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(bits), (unsigned int)((bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_fdivk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_fdivk)>>32), ((int)in->pc));
#line 2784 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CALLEXIT:
        //| mov r11d, ((int)in->pc)
        //| jmp >8
        dasm_put(Dst, 3406, ((int)in->pc));
#line 2789 "src/luar_emit_x64.dasc"
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
          dasm_put(Dst, 3414, ((int)LUAR_SLOT(forslot)), ((int)LUAR_SLOT(forslot)), ((int)LUAR_SLOT(forslot + 2)), ((int)LUAR_SLOT(forslot + 1)), ((int)LUAR_SLOT(forslot + 2)));
#line 2816 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3475, ((int)LUAR_SLOT(forslot)), ((int)LUAR_SLOT(forslot)), ((int)LUAR_SLOT(forslot + 2)), ((int)LUAR_SLOT(forslot + 1)), ((int)LUAR_SLOT(forslot + 2)));
#line 2829 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 3519);
#line 2845 "src/luar_emit_x64.dasc"
  if (side) {
    //| mov eax, r11d
    //| neg eax
    //| sub eax, 2
    dasm_put(Dst, 3539);
#line 2849 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov eax, r11d
    //| neg eax
    //| sub eax, 2
    dasm_put(Dst, 3539);
#line 2854 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 3549);
#line 2876 "src/luar_emit_x64.dasc"
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
  {
    unsigned maxpc = (d->has_branch && d->npc > 0) ?
                     (unsigned)d->npc + 2 : 0;
    int i;
    for (i = 0; i < d->nops; i++) {
      const LuaR_Op *op = &d->ops[i];
      if (op->kind == LUAR_OP_SHL || op->kind == LUAR_OP_SHR) {
        unsigned need = 4096u + (unsigned)op->pc * 4u + 3u;
        if (need > maxpc)
          maxpc = need;
      }
    }
    if (maxpc > 0)
      dasm_growpc(&Dst, maxpc);
  }
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
    dasm_put(Dst, 3338, imm);
#line 2986 "src/luar_emit_x64.dasc"
  else {
    //| mov64 rcx, imm
    //| add rax, rcx
    dasm_put(Dst, 3598, (unsigned int)(imm), (unsigned int)((imm)>>32));
#line 2989 "src/luar_emit_x64.dasc"
  }
}

static void emit_sub_imm (dasm_State **Dst, int64_t imm)
{
  if (imm32(imm))
    //| sub rax, imm
    dasm_put(Dst, 3343, imm);
#line 2996 "src/luar_emit_x64.dasc"
  else {
    //| mov64 rcx, imm
    //| sub rax, rcx
    dasm_put(Dst, 3606, (unsigned int)(imm), (unsigned int)((imm)>>32));
#line 2999 "src/luar_emit_x64.dasc"
  }
}

static void emit_mul_imm (dasm_State **Dst, int64_t imm)
{
  if (imm32(imm))
    //| imul rax, imm
    dasm_put(Dst, 3348, imm);
#line 3006 "src/luar_emit_x64.dasc"
  else {
    //| mov64 rcx, imm
    //| imul rax, rcx
    dasm_put(Dst, 3614, (unsigned int)(imm), (unsigned int)((imm)>>32));
#line 3009 "src/luar_emit_x64.dasc"
  }
}
