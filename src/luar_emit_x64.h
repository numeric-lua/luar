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
static const unsigned char luar_emit_actions[3986] = {
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
  131,233,255,72,15,175,131,233,255,72,185,237,237,255,72,139,139,233,65,187,
  237,72,133,201,255,15,132,244,249,255,15,132,244,250,255,72,252,255,193,255,
  72,252,255,201,255,72,153,72,252,247,252,249,255,72,137,208,72,133,192,15,
  132,245,73,137,192,73,49,200,15,137,245,72,1,200,255,72,133,210,15,132,245,
  73,137,208,73,49,200,15,137,245,72,252,255,200,255,249,72,137,131,233,198,
  131,233,235,255,72,139,131,233,72,139,139,233,255,72,33,200,255,72,9,200,
  255,72,49,200,255,72,139,131,233,72,185,237,237,255,72,139,131,233,72,252,
  247,208,72,137,131,233,198,131,233,235,255,49,192,255,72,193,232,235,255,
  72,193,224,235,255,72,139,131,233,72,252,247,216,72,137,131,233,198,131,233,
  235,255,72,139,131,233,72,185,237,237,72,49,200,72,137,131,233,198,131,233,
  235,255,72,184,237,237,72,139,139,233,72,133,201,15,136,245,72,131,252,249,
  64,15,141,245,72,211,224,252,233,245,249,72,131,252,249,192,15,142,245,72,
  252,247,217,72,211,232,252,233,245,249,49,192,249,72,137,131,233,198,131,
  233,235,255,72,139,131,233,72,139,139,233,72,133,201,15,136,245,72,131,252,
  249,64,15,141,245,255,72,211,224,255,72,211,232,255,252,233,245,249,72,131,
  252,249,192,15,142,245,72,252,247,217,255,72,185,237,237,72,57,200,255,72,
  59,131,233,255,15,140,245,255,15,142,245,255,15,143,245,255,15,132,245,255,
  15,133,245,255,65,187,237,72,139,131,233,72,185,237,237,72,57,200,15,143,
  244,249,72,185,237,237,72,57,200,15,140,244,249,255,65,187,237,252,233,244,
  249,255,72,184,237,237,252,242,72,15,42,200,255,102,15,46,193,255,15,138,
  245,255,15,130,245,255,15,131,245,255,15,134,245,255,15,135,245,255,72,139,
  131,233,72,131,252,248,1,15,140,244,249,76,57,252,240,15,135,244,249,65,128,
  188,253,5,233,235,15,133,244,249,72,137,194,72,193,226,3,72,252,247,218,73,
  139,76,21,0,72,137,139,233,198,131,233,235,255,72,139,131,233,72,131,252,
  248,1,15,140,244,249,76,57,252,240,15,135,244,249,65,128,188,253,5,233,235,
  15,133,244,249,72,137,194,72,193,226,3,72,252,247,218,73,139,76,21,0,255,
  72,139,131,233,72,1,200,72,137,131,233,198,131,233,235,255,252,242,72,15,
  42,193,252,242,15,88,131,233,252,242,15,17,131,233,198,131,233,235,255,184,
  237,72,131,252,248,1,15,140,244,249,76,57,252,240,15,135,244,249,65,128,188,
  253,5,233,235,15,133,244,249,72,137,194,72,193,226,3,72,252,247,218,73,139,
  76,21,0,72,137,139,233,198,131,233,235,255,184,237,72,131,252,248,1,15,140,
  244,249,76,57,252,240,15,135,244,249,65,128,188,253,5,233,235,15,133,244,
  249,72,137,194,72,193,226,3,72,252,247,218,73,139,76,21,0,255,72,139,131,
  233,72,131,252,248,1,15,140,244,249,76,57,252,240,15,135,244,249,72,137,194,
  72,193,226,3,72,252,247,218,255,72,139,139,233,73,137,76,21,0,65,198,132,
  253,5,233,235,255,252,242,15,16,131,233,252,242,65,15,17,68,21,0,65,198,132,
  253,5,233,235,255,252,242,15,16,131,233,252,242,15,88,131,233,252,242,15,
  16,139,233,102,15,87,210,102,15,46,202,15,135,245,252,233,245,249,102,15,
  46,131,233,15,138,245,15,134,245,252,233,245,249,252,242,15,16,139,233,102,
  15,46,200,15,138,245,255,15,134,245,252,233,245,249,252,242,15,17,131,233,
  255,252,233,245,255,252,233,244,1,249,255,83,72,137,252,251,255,65,84,65,
  85,65,86,255,65,187,237,255,76,139,163,233,73,139,132,253,36,233,72,133,192,
  15,133,244,249,77,139,172,253,36,233,69,139,180,253,36,233,255,249,184,1,
  0,0,0,255,65,94,65,93,65,92,255,91,195,255,72,139,131,233,72,133,192,15,132,
  244,248,72,252,255,200,72,137,131,233,72,139,139,233,72,3,139,233,72,137,
  139,233,252,233,245,255,249,248,2,184,1,0,0,0,255,248,3,68,137,216,131,192,
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
  15,135,244,249,255,76,137,252,234,72,193,226,3,72,252,247,218,77,137,44,18,
  67,198,132,253,42,233,235,77,133,228,15,132,244,248,73,252,255,204,77,1,252,
  245,252,233,244,1,248,2,76,137,163,233,76,137,171,233,184,1,0,0,0,65,95,65,
  94,65,93,65,92,91,195,248,3,76,137,163,233,76,137,171,233,49,192,65,95,65,
  94,65,93,65,92,91,195,255,76,139,187,233,255,252,242,73,15,42,205,252,242,
  15,88,193,255,77,1,252,239,255,76,137,187,233,255,83,72,137,252,251,65,84,
  65,85,65,86,65,87,76,139,163,233,76,139,171,233,76,139,179,233,76,139,187,
  233,248,1,76,137,232,255,73,1,199,77,133,228,15,132,244,248,73,252,255,204,
  77,1,252,245,252,233,244,1,248,2,76,137,163,233,76,137,171,233,76,137,187,
  233,184,1,0,0,0,65,95,65,94,65,93,65,92,91,195,248,3,49,192,65,95,65,94,65,
  93,65,92,91,195,255,83,72,137,252,251,65,84,65,85,65,86,65,87,76,139,163,
  233,76,139,171,233,76,139,179,233,76,139,187,233,248,1,77,1,252,239,77,133,
  228,15,132,244,248,73,252,255,204,77,1,252,245,252,233,244,1,248,2,76,137,
  163,233,76,137,171,233,76,137,187,233,184,1,0,0,0,65,95,65,94,65,93,65,92,
  91,195,248,3,49,192,65,95,65,94,65,93,65,92,91,195,255,76,139,131,233,73,
  139,128,233,72,133,192,15,133,244,249,77,139,136,233,69,139,144,233,255,73,
  131,252,254,1,15,133,244,249,73,131,252,253,1,15,140,244,249,76,137,232,76,
  1,224,76,57,208,15,135,244,249,76,137,201,76,1,252,233,72,131,193,3,76,137,
  226,72,252,255,194,248,4,128,57,235,15,133,244,249,72,252,255,193,72,252,
  255,202,15,133,244,4,77,137,252,235,73,193,227,3,73,252,247,219,77,1,203,
  248,1,73,139,11,255,252,242,72,15,42,201,252,242,15,88,193,255,73,131,252,
  235,8,77,133,228,15,132,244,248,73,252,255,204,255,73,252,255,197,252,233,
  244,1,248,2,76,137,163,233,76,137,171,233,255,184,1,0,0,0,65,95,65,94,65,
  93,65,92,91,195,248,3,76,137,163,233,76,137,171,233,255,252,242,15,16,131,
  233,72,184,237,237,102,72,15,110,208,255,76,139,163,233,255,76,139,171,233,
  255,73,190,237,237,255,248,1,77,57,252,245,255,15,140,244,248,255,15,141,
  244,248,255,15,142,244,248,255,15,143,244,248,255,252,242,73,15,42,205,255,
  76,137,252,233,73,15,175,205,255,252,242,15,89,202,255,252,242,15,17,139,
  233,198,131,233,235,255,77,15,175,229,255,77,41,252,236,255,73,1,204,255,
  77,1,252,236,255,73,129,197,239,252,233,244,1,248,2,255,76,137,163,233,255,
  76,137,171,233,184,1,0,0,0,65,94,65,93,65,92,91,195,248,3,49,192,65,94,65,
  93,65,92,91,195,255,248,1,76,137,252,233,73,15,175,205,255,77,15,175,252,
  253,255,252,242,73,15,42,205,252,242,15,92,193,255,77,41,252,239,255,83,72,
  137,252,251,65,85,65,86,65,87,76,139,171,233,255,248,1,73,129,197,239,255,
  77,57,252,238,255,15,140,244,1,255,15,141,244,1,255,15,142,244,1,255,15,143,
  244,1,255,15,132,244,1,255,15,133,244,1,255,184,1,0,0,0,65,95,65,94,65,93,
  91,195,248,3,248,2,49,192,65,95,65,94,65,93,91,195,255,184,237,91,195,248,
  4,68,137,216,131,192,2,252,247,216,91,195,248,3,49,192,91,195,255,72,139,
  131,233,72,133,192,15,132,244,248,72,252,255,200,72,137,131,233,72,139,139,
  233,72,3,139,233,72,137,139,233,252,233,244,1,255,248,2,184,1,0,0,0,91,195,
  248,4,68,137,216,131,192,2,252,247,216,91,195,248,3,49,192,91,195,255,65,
  84,65,87,83,72,131,252,236,16,72,137,252,251,73,137,252,244,65,191,0,1,0,
  0,65,187,237,248,1,255,65,187,237,128,187,233,235,15,133,244,248,255,72,141,
  187,233,190,237,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,
  248,255,72,141,187,233,72,190,237,237,76,137,226,72,184,237,237,252,255,208,
  65,187,237,133,192,15,132,244,248,255,76,137,231,72,141,179,233,72,186,237,
  237,185,237,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,
  255,72,141,187,233,72,141,179,233,72,141,147,233,72,184,237,237,252,255,208,
  65,187,237,133,192,15,132,244,248,255,72,141,187,233,72,141,179,233,72,184,
  237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,72,141,187,233,
  72,139,179,233,72,141,147,233,72,184,237,237,252,255,208,65,187,237,133,192,
  15,132,244,248,255,72,141,187,233,252,242,15,16,131,233,72,141,179,233,72,
  184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,76,137,231,
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

static void emit_body (dasm_State **Dst, const LuaR_LoopDesc *d, int fail)
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
      case LUAR_OP_MOD:
      case LUAR_OP_IDIV:
      case LUAR_OP_MODK:
      case LUAR_OP_IDIVK: {
        int ismod = (op->kind == LUAR_OP_MOD || op->kind == LUAR_OP_MODK);
        int isk = (op->kind == LUAR_OP_MODK ||
                   op->kind == LUAR_OP_IDIVK);
        int ldone = 6000 + op->pc * 8;
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(op->b))]
        dasm_put(Dst, 185, ((int)LUAR_SLOT(op->b)));
#line 270 "src/luar_emit_x64.dasc"
        if (isk) {
          //| mov64 rcx, op->imm
          dasm_put(Dst, 249, (unsigned int)(op->imm), (unsigned int)((op->imm)>>32));
#line 272 "src/luar_emit_x64.dasc"
        }
        else {
          //| mov rcx, qword [rbx + ((int)LUAR_SLOT(op->c))]
          //| mov r11d, ((int)op->pc)
          //| test rcx, rcx
          dasm_put(Dst, 254, ((int)LUAR_SLOT(op->c)), ((int)op->pc));
#line 277 "src/luar_emit_x64.dasc"
          if (fail == 3) {
            //| jz >3
            dasm_put(Dst, 265);
#line 279 "src/luar_emit_x64.dasc"
          }
          else {
            //| jz >4
            dasm_put(Dst, 270);
#line 282 "src/luar_emit_x64.dasc"
          }
          /* inc/dec avoids a negative immediate encoding and detects the
          ** n == -1 overflow special case exactly. */
          //| inc rcx
          dasm_put(Dst, 275);
#line 286 "src/luar_emit_x64.dasc"
          if (fail == 3) {
            //| jz >3
            dasm_put(Dst, 265);
#line 288 "src/luar_emit_x64.dasc"
          }
          else {
            //| jz >4
            dasm_put(Dst, 270);
#line 291 "src/luar_emit_x64.dasc"
          }
          //| dec rcx
          dasm_put(Dst, 280);
#line 293 "src/luar_emit_x64.dasc"
        }
        //| cqo
        //| idiv rcx
        dasm_put(Dst, 285);
#line 296 "src/luar_emit_x64.dasc"
        if (ismod) {
          //| mov rax, rdx
          //| test rax, rax
          //| jz =>ldone
          //| mov r8, rax
          //| xor r8, rcx
          //| jns =>ldone
          //| add rax, rcx
          dasm_put(Dst, 293, ldone, ldone);
#line 304 "src/luar_emit_x64.dasc"
        }
        else {
          //| test rdx, rdx
          //| jz =>ldone
          //| mov r8, rdx
          //| xor r8, rcx
          //| jns =>ldone
          //| dec rax
          dasm_put(Dst, 315, ldone, ldone);
#line 312 "src/luar_emit_x64.dasc"
        }
        //|=>ldone:
        //| mov qword [rbx + ((int)LUAR_SLOT(op->a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(op->a))], ((int)d->tag_int)
        dasm_put(Dst, 335, ldone, ((int)LUAR_SLOT(op->a)), ((int)LUAR_TAG(op->a)), ((int)d->tag_int));
#line 316 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_OP_BAND:
      case LUAR_OP_BOR:
      case LUAR_OP_BXOR:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
        //| mov rcx, qword [rbx + ((int)LUAR_SLOT(op->c))]
        dasm_put(Dst, 345, ((int)LUAR_SLOT(b)), ((int)LUAR_SLOT(op->c)));
#line 323 "src/luar_emit_x64.dasc"
        if (op->kind == LUAR_OP_BAND) {
          //| and rax, rcx
          dasm_put(Dst, 354);
#line 325 "src/luar_emit_x64.dasc"
        }
        if (op->kind == LUAR_OP_BOR) {
          //| or rax, rcx
          dasm_put(Dst, 358);
#line 328 "src/luar_emit_x64.dasc"
        }
        if (op->kind == LUAR_OP_BXOR) {
          //| xor rax, rcx
          dasm_put(Dst, 362);
#line 331 "src/luar_emit_x64.dasc"
        }
        //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
        dasm_put(Dst, 57, ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 334 "src/luar_emit_x64.dasc"
        break;
      case LUAR_OP_BANDK:
      case LUAR_OP_BORK:
      case LUAR_OP_BXORK:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
        //| mov64 rcx, op->imm
        dasm_put(Dst, 366, ((int)LUAR_SLOT(b)), (unsigned int)(op->imm), (unsigned int)((op->imm)>>32));
#line 340 "src/luar_emit_x64.dasc"
        if (op->kind == LUAR_OP_BANDK) {
          //| and rax, rcx
          dasm_put(Dst, 354);
#line 342 "src/luar_emit_x64.dasc"
        }
        if (op->kind == LUAR_OP_BORK) {
          //| or rax, rcx
          dasm_put(Dst, 358);
#line 345 "src/luar_emit_x64.dasc"
        }
        if (op->kind == LUAR_OP_BXORK) {
          //| xor rax, rcx
          dasm_put(Dst, 362);
#line 348 "src/luar_emit_x64.dasc"
        }
        //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
        dasm_put(Dst, 57, ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 351 "src/luar_emit_x64.dasc"
        break;
      case LUAR_OP_BNOT:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
        //| not rax
        //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
        dasm_put(Dst, 375, ((int)LUAR_SLOT(b)), ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 357 "src/luar_emit_x64.dasc"
        break;
      case LUAR_OP_SHRI: {
        int64_t count = op->imm;
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
        dasm_put(Dst, 185, ((int)LUAR_SLOT(b)));
#line 361 "src/luar_emit_x64.dasc"
        if (count > 0) {
          if (count >= 64)
            //| xor eax, eax
            dasm_put(Dst, 392);
#line 364 "src/luar_emit_x64.dasc"
          else
            //| shr rax, ((int)count)
            dasm_put(Dst, 395, ((int)count));
#line 366 "src/luar_emit_x64.dasc"
        }
        else if (count < 0) {
          if (count <= -64)
            //| xor eax, eax
            dasm_put(Dst, 392);
#line 370 "src/luar_emit_x64.dasc"
          else
            //| shl rax, ((int)-count)
            dasm_put(Dst, 400, ((int)-count));
#line 372 "src/luar_emit_x64.dasc"
        }
        //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
        dasm_put(Dst, 57, ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 375 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_OP_UNM:
        if (op->ta == LUAR_T_INT) {
          //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
          //| neg rax
          //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
          //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
          dasm_put(Dst, 405, ((int)LUAR_SLOT(b)), ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 383 "src/luar_emit_x64.dasc"
        }
        else {
          /* -x flips the IEEE sign bit, preserving zeros and NaNs. */
          //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
          //| mov64 rcx, INT64_MIN
          //| xor rax, rcx
          //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
          //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_flt)
          dasm_put(Dst, 422, ((int)LUAR_SLOT(b)), (unsigned int)(INT64_MIN), (unsigned int)((INT64_MIN)>>32), ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_flt));
#line 391 "src/luar_emit_x64.dasc"
        }
        break;
      case LUAR_OP_SHLI: {
        /* SHLI shifts a small constant by a register count. */
        int lneg = 4096 + op->pc * 4;
        int lzero = lneg + 1;
        int ldone = lneg + 2;
        //| mov64 rax, op->imm
        //| mov rcx, qword [rbx + ((int)LUAR_SLOT(op->b))]
        //| test rcx, rcx
        //| js =>lneg
        //| cmp rcx, 64
        //| jge =>lzero
        //| shl rax, cl
        //| jmp =>ldone
        //|=>lneg:
        //| cmp rcx, -64
        //| jle =>lzero
        //| neg rcx
        //| shr rax, cl
        //| jmp =>ldone
        //|=>lzero:
        //| xor eax, eax
        //|=>ldone:
        //| mov qword [rbx + ((int)LUAR_SLOT(op->a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(op->a))], ((int)d->tag_int)
        dasm_put(Dst, 442, (unsigned int)(op->imm), (unsigned int)((op->imm)>>32), ((int)LUAR_SLOT(op->b)), lneg, lzero, ldone, lneg, lzero, ldone, lzero, ldone, ((int)LUAR_SLOT(op->a)), ((int)LUAR_TAG(op->a)), ((int)d->tag_int));
#line 417 "src/luar_emit_x64.dasc"
        break;
      }
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
        dasm_put(Dst, 502, ((int)LUAR_SLOT(b)), ((int)LUAR_SLOT(op->c)), lneg, lzero);
#line 433 "src/luar_emit_x64.dasc"
        if (op->kind == LUAR_OP_SHL) {
          //| shl rax, cl
          dasm_put(Dst, 525);
#line 435 "src/luar_emit_x64.dasc"
        }
        else {
          //| shr rax, cl
          dasm_put(Dst, 529);
#line 438 "src/luar_emit_x64.dasc"
        }
        //| jmp =>ldone
        //|=>lneg:
        //| cmp rcx, -64
        //| jle =>lzero
        //| neg rcx
        dasm_put(Dst, 533, ldone, lneg, lzero);
#line 444 "src/luar_emit_x64.dasc"
        if (op->kind == LUAR_OP_SHL) {
          //| shr rax, cl
          dasm_put(Dst, 529);
#line 446 "src/luar_emit_x64.dasc"
        }
        else {
          //| shl rax, cl
          dasm_put(Dst, 525);
#line 449 "src/luar_emit_x64.dasc"
        }
        //| jmp =>ldone
        //|=>lzero:
        //| xor eax, eax
        //|=>ldone:
        //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
        dasm_put(Dst, 486, ldone, lzero, ldone, ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 456 "src/luar_emit_x64.dasc"
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
  emit_body(Dst, &one, 3);
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
#line 484 "src/luar_emit_x64.dasc"
    if (b == 0xFF) {
      //| mov64 rcx, op->imm
      //| cmp rax, rcx
      dasm_put(Dst, 550, (unsigned int)(op->imm), (unsigned int)((op->imm)>>32));
#line 487 "src/luar_emit_x64.dasc"
    }
    else {
      //| cmp rax, qword [rbx + ((int)LUAR_SLOT(b))]
      dasm_put(Dst, 558, ((int)LUAR_SLOT(b)));
#line 490 "src/luar_emit_x64.dasc"
    }
    switch (op->kind) {
      case LUAR_OP_LT:
        if (k) {
          //| jl =>((int)op->target)
          dasm_put(Dst, 563, ((int)op->target));
#line 495 "src/luar_emit_x64.dasc"
        }
        else {
          //| jge =>((int)op->target)
          dasm_put(Dst, 521, ((int)op->target));
#line 498 "src/luar_emit_x64.dasc"
        }
        break;
      case LUAR_OP_LE:
        if (k) {
          //| jle =>((int)op->target)
          dasm_put(Dst, 567, ((int)op->target));
#line 503 "src/luar_emit_x64.dasc"
        }
        else {
          //| jg =>((int)op->target)
          dasm_put(Dst, 571, ((int)op->target));
#line 506 "src/luar_emit_x64.dasc"
        }
        break;
      case LUAR_OP_GT:
        if (k) {
          //| jg =>((int)op->target)
          dasm_put(Dst, 571, ((int)op->target));
#line 511 "src/luar_emit_x64.dasc"
        }
        else {
          //| jle =>((int)op->target)
          dasm_put(Dst, 567, ((int)op->target));
#line 514 "src/luar_emit_x64.dasc"
        }
        break;
      case LUAR_OP_GE:
        if (k) {
          //| jge =>((int)op->target)
          dasm_put(Dst, 521, ((int)op->target));
#line 519 "src/luar_emit_x64.dasc"
        }
        else {
          //| jl =>((int)op->target)
          dasm_put(Dst, 563, ((int)op->target));
#line 522 "src/luar_emit_x64.dasc"
        }
        break;
      case LUAR_OP_EQ:
        if (k) {
          //| je =>((int)op->target)
          dasm_put(Dst, 575, ((int)op->target));
#line 527 "src/luar_emit_x64.dasc"
        }
        else {
          //| jne =>((int)op->target)
          dasm_put(Dst, 579, ((int)op->target));
#line 530 "src/luar_emit_x64.dasc"
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
    dasm_put(Dst, 583, ((int)op->pc), ((int)LUAR_SLOT(a)), (unsigned int)(9007199254740992), (unsigned int)((9007199254740992)>>32), (unsigned int)(-9007199254740992), (unsigned int)((-9007199254740992)>>32));
#line 551 "src/luar_emit_x64.dasc"
  }
  if (op->tb == LUAR_T_INT && b == 0xFF) {
    if (op->imm > 9007199254740992LL ||
        op->imm < -9007199254740992LL) {
      //| mov r11d, ((int)op->pc)
      //| jmp >3
      dasm_put(Dst, 613, ((int)op->pc));
#line 557 "src/luar_emit_x64.dasc"
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
    dasm_put(Dst, 583, ((int)op->pc), ((int)LUAR_SLOT(b)), (unsigned int)(9007199254740992), (unsigned int)((9007199254740992)>>32), (unsigned int)(-9007199254740992), (unsigned int)((-9007199254740992)>>32));
#line 568 "src/luar_emit_x64.dasc"
  }

  /* At least one operand is a double: convert both and use ucomisd. */
  load_double(Dst, a, op->ta);
  if (b == 0xFF) {
    if (op->tb == LUAR_T_INT) {
      //| mov64 rax, op->imm
      //| cvtsi2sd xmm1, rax
      dasm_put(Dst, 621, (unsigned int)(op->imm), (unsigned int)((op->imm)>>32));
#line 576 "src/luar_emit_x64.dasc"
    }
    else {
      int64_t bits;
      memcpy(&bits, &op->num, sizeof(bits));
      //| mov64 rax, bits
      //| movq xmm1, rax
      dasm_put(Dst, 151, (unsigned int)(bits), (unsigned int)((bits)>>32));
#line 582 "src/luar_emit_x64.dasc"
    }
  }
  else if (op->tb == LUAR_T_INT) {
    //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
    //| cvtsi2sd xmm1, rax
    dasm_put(Dst, 190, ((int)LUAR_SLOT(b)));
#line 587 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd xmm1, qword [rbx + ((int)LUAR_SLOT(b))]
    dasm_put(Dst, 125, ((int)LUAR_SLOT(b)));
#line 590 "src/luar_emit_x64.dasc"
  }
  //| ucomisd xmm0, xmm1
  dasm_put(Dst, 632);
#line 592 "src/luar_emit_x64.dasc"
  /* Unordered (NaN) comparisons are false, as in Lua. */
  if (k) {
    //| jp =>((int)fallthrough)
    dasm_put(Dst, 637, ((int)fallthrough));
#line 595 "src/luar_emit_x64.dasc"
  }
  else {
    //| jp =>((int)op->target)
    dasm_put(Dst, 637, ((int)op->target));
#line 598 "src/luar_emit_x64.dasc"
  }
  switch (op->kind) {
    case LUAR_OP_LT:
      if (k) {
          //| jb =>((int)op->target)
          dasm_put(Dst, 641, ((int)op->target));
#line 603 "src/luar_emit_x64.dasc"
        }
        else {
          //| jae =>((int)op->target)
          dasm_put(Dst, 645, ((int)op->target));
#line 606 "src/luar_emit_x64.dasc"
        }
      break;
    case LUAR_OP_LE:
      if (k) {
          //| jbe =>((int)op->target)
          dasm_put(Dst, 649, ((int)op->target));
#line 611 "src/luar_emit_x64.dasc"
        }
        else {
          //| ja =>((int)op->target)
          dasm_put(Dst, 653, ((int)op->target));
#line 614 "src/luar_emit_x64.dasc"
        }
      break;
    case LUAR_OP_GT:
      if (k) {
          //| ja =>((int)op->target)
          dasm_put(Dst, 653, ((int)op->target));
#line 619 "src/luar_emit_x64.dasc"
        }
        else {
          //| jbe =>((int)op->target)
          dasm_put(Dst, 649, ((int)op->target));
#line 622 "src/luar_emit_x64.dasc"
        }
      break;
    case LUAR_OP_GE:
      if (k) {
          //| jae =>((int)op->target)
          dasm_put(Dst, 645, ((int)op->target));
#line 627 "src/luar_emit_x64.dasc"
        }
        else {
          //| jb =>((int)op->target)
          dasm_put(Dst, 641, ((int)op->target));
#line 630 "src/luar_emit_x64.dasc"
        }
      break;
    case LUAR_OP_EQ:
      if (k) {
          //| je =>((int)op->target)
          dasm_put(Dst, 575, ((int)op->target));
#line 635 "src/luar_emit_x64.dasc"
        }
        else {
          //| jne =>((int)op->target)
          dasm_put(Dst, 579, ((int)op->target));
#line 638 "src/luar_emit_x64.dasc"
        }
      break;
    default: break;
  }
}

static void emit_table_get (dasm_State **Dst, const LuaR_LoopDesc *d,
                            const LuaR_Op *op)
{
  /* Lua 5.5 keeps the array part as an inverted Value array followed by
  ** a separate tag array (src/ltable.h getArrVal/getArrTag).  The old J2
  ** code treated it as 16-byte TValues and read a neighbouring payload as
  ** the tag, which either forced an exit or accepted a wrong slot. */
  //| mov rax, qword [rbx + ((int)LUAR_SLOT(op->c))]
  //| cmp rax, 1
  //| jl >3
  //| cmp rax, r14
  //| ja >3
  //| cmp byte [r13 + rax + 3], ((int)d->tag_int)
  //| jne >3
  //| mov rdx, rax
  //| shl rdx, 3
  //| neg rdx
  //| mov rcx, qword [r13 + rdx]
  //| mov qword [rbx + ((int)LUAR_SLOT(op->a))], rcx
  //| mov byte [rbx + ((int)LUAR_TAG(op->a))], ((int)d->tag_int)
  dasm_put(Dst, 657, ((int)LUAR_SLOT(op->c)), 3, ((int)d->tag_int), ((int)LUAR_SLOT(op->a)), ((int)LUAR_TAG(op->a)), ((int)d->tag_int));
#line 664 "src/luar_emit_x64.dasc"
}

static void emit_table_add (dasm_State **Dst, const LuaR_LoopDesc *d,
                            const LuaR_Op *op)
{
  //| mov rax, qword [rbx + ((int)LUAR_SLOT(op->c))]
  //| cmp rax, 1
  //| jl >3
  //| cmp rax, r14
  //| ja >3
  //| cmp byte [r13 + rax + 3], ((int)d->tag_int)
  //| jne >3
  //| mov rdx, rax
  //| shl rdx, 3
  //| neg rdx
  //| mov rcx, qword [r13 + rdx]
  dasm_put(Dst, 714, ((int)LUAR_SLOT(op->c)), 3, ((int)d->tag_int));
#line 680 "src/luar_emit_x64.dasc"
  /* The bytecode lookup writes R[ka] before the fused add.  Preserve it
  ** when a later instruction or the caller can observe that register. */
  if (op->res != 0xFF) {
    //| mov qword [rbx + ((int)LUAR_SLOT(op->res))], rcx
    //| mov byte [rbx + ((int)LUAR_TAG(op->res))], ((int)d->tag_int)
    dasm_put(Dst, 705, ((int)LUAR_SLOT(op->res)), ((int)LUAR_TAG(op->res)), ((int)d->tag_int));
#line 685 "src/luar_emit_x64.dasc"
  }
  if (op->ta == LUAR_T_INT) {
    //| mov rax, qword [rbx + ((int)LUAR_SLOT(op->a))]
    //| add rax, rcx
    //| mov qword [rbx + ((int)LUAR_SLOT(op->a))], rax
    //| mov byte [rbx + ((int)LUAR_TAG(op->a))], ((int)d->tag_int)
    dasm_put(Dst, 763, ((int)LUAR_SLOT(op->a)), ((int)LUAR_SLOT(op->a)), ((int)LUAR_TAG(op->a)), ((int)d->tag_int));
#line 691 "src/luar_emit_x64.dasc"
  }
  else {
    //| cvtsi2sd xmm0, rcx
    //| addsd xmm0, qword [rbx + ((int)LUAR_SLOT(op->a))]
    //| movsd qword [rbx + ((int)LUAR_SLOT(op->a))], xmm0
    //| mov byte [rbx + ((int)LUAR_TAG(op->a))], ((int)d->tag_flt)
    dasm_put(Dst, 779, ((int)LUAR_SLOT(op->a)), ((int)LUAR_SLOT(op->a)), ((int)LUAR_TAG(op->a)), ((int)d->tag_flt));
#line 697 "src/luar_emit_x64.dasc"
  }
}

static void emit_table_geti (dasm_State **Dst, const LuaR_LoopDesc *d,
                             const LuaR_Op *op)
{
  /* Same split-array access as emit_table_get, with the key immediate. */
  //| mov eax, ((int)op->imm)
  //| cmp rax, 1
  //| jl >3
  //| cmp rax, r14
  //| ja >3
  //| cmp byte [r13 + rax + 3], ((int)d->tag_int)
  //| jne >3
  //| mov rdx, rax
  //| shl rdx, 3
  //| neg rdx
  //| mov rcx, qword [r13 + rdx]
  //| mov qword [rbx + ((int)LUAR_SLOT(op->a))], rcx
  //| mov byte [rbx + ((int)LUAR_TAG(op->a))], ((int)d->tag_int)
  dasm_put(Dst, 802, ((int)op->imm), 3, ((int)d->tag_int), ((int)LUAR_SLOT(op->a)), ((int)LUAR_TAG(op->a)), ((int)d->tag_int));
#line 717 "src/luar_emit_x64.dasc"
}

static void emit_table_addi (dasm_State **Dst, const LuaR_LoopDesc *d,
                             const LuaR_Op *op)
{
  //| mov eax, ((int)op->imm)
  //| cmp rax, 1
  //| jl >3
  //| cmp rax, r14
  //| ja >3
  //| cmp byte [r13 + rax + 3], ((int)d->tag_int)
  //| jne >3
  //| mov rdx, rax
  //| shl rdx, 3
  //| neg rdx
  //| mov rcx, qword [r13 + rdx]
  dasm_put(Dst, 857, ((int)op->imm), 3, ((int)d->tag_int));
#line 733 "src/luar_emit_x64.dasc"
  /* Materialise the GETI result even though the accumulator is the only
  ** register the fused add needs.  "local x = t[1]; s = s + x" must leave
  ** x holding the last element, as the interpreter does. */
  if (op->res != 0xFF) {
    //| mov qword [rbx + ((int)LUAR_SLOT(op->res))], rcx
    //| mov byte [rbx + ((int)LUAR_TAG(op->res))], ((int)d->tag_int)
    dasm_put(Dst, 705, ((int)LUAR_SLOT(op->res)), ((int)LUAR_TAG(op->res)), ((int)d->tag_int));
#line 739 "src/luar_emit_x64.dasc"
  }
  if (op->ta == LUAR_T_INT) {
    //| mov rax, qword [rbx + ((int)LUAR_SLOT(op->a))]
    //| add rax, rcx
    //| mov qword [rbx + ((int)LUAR_SLOT(op->a))], rax
    //| mov byte [rbx + ((int)LUAR_TAG(op->a))], ((int)d->tag_int)
    dasm_put(Dst, 763, ((int)LUAR_SLOT(op->a)), ((int)LUAR_SLOT(op->a)), ((int)LUAR_TAG(op->a)), ((int)d->tag_int));
#line 745 "src/luar_emit_x64.dasc"
  }
  else {
    //| cvtsi2sd xmm0, rcx
    //| addsd xmm0, qword [rbx + ((int)LUAR_SLOT(op->a))]
    //| movsd qword [rbx + ((int)LUAR_SLOT(op->a))], xmm0
    //| mov byte [rbx + ((int)LUAR_TAG(op->a))], ((int)d->tag_flt)
    dasm_put(Dst, 779, ((int)LUAR_SLOT(op->a)), ((int)LUAR_SLOT(op->a)), ((int)LUAR_TAG(op->a)), ((int)d->tag_flt));
#line 751 "src/luar_emit_x64.dasc"
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
  //| mov rdx, rax
  //| shl rdx, 3
  //| neg rdx
  dasm_put(Dst, 904, ((int)LUAR_SLOT(op->b)));
#line 765 "src/luar_emit_x64.dasc"
  if (op->ta == LUAR_T_INT) {
    //| mov rcx, qword [rbx + ((int)LUAR_SLOT(op->c))]
    //| mov qword [r13 + rdx], rcx
    //| mov byte [r13 + rax + 3], ((int)d->tag_int)
    dasm_put(Dst, 937, ((int)LUAR_SLOT(op->c)), 3, ((int)d->tag_int));
#line 769 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(op->c))]
    //| movsd qword [r13 + rdx], xmm0
    //| mov byte [r13 + rax + 3], ((int)d->tag_flt)
    dasm_put(Dst, 954, ((int)LUAR_SLOT(op->c)), 3, ((int)d->tag_flt));
#line 774 "src/luar_emit_x64.dasc"
  }
}

/* Float numeric-for epilogue.  At entry R[A] is the limit, R[A+1] the
** step and R[A+2] the current index (the interpreter's float FORPREP
** layout).  Compute the next index, test it exactly as floatforloop() does
** (including unordered/NaN comparisons), and store it only when another
** iteration runs. */
static void emit_float_forloop (dasm_State **Dst, const LuaR_LoopDesc *d,
                                int back, int pc_back)
{
  int lpos = 9000, ldesc = 9001, lclose = 9002, lcont = 9003;
  //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))]
  //| addsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  //| movsd xmm1, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  //| xorpd xmm2, xmm2
  //| ucomisd xmm1, xmm2
  //| ja =>lpos
  //| jmp =>ldesc
  //|=>lpos:
  //| ucomisd xmm0, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| jp =>lclose
  //| jbe =>lcont
  //| jmp =>lclose
  //|=>ldesc:
  //| movsd xmm1, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| ucomisd xmm1, xmm0
  //| jp =>lclose
  //| jbe =>lcont
  dasm_put(Dst, 976, ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)), ((int)LUAR_SLOT(d->for_a + 1)), lpos, ldesc, lpos, ((int)LUAR_SLOT(d->for_a)), lclose, lcont, lclose, ldesc, ((int)LUAR_SLOT(d->for_a)), lclose);
#line 803 "src/luar_emit_x64.dasc"
  //| jmp =>lclose
  //|=>lcont:
  //| movsd qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], xmm0
  dasm_put(Dst, 1038, lcont, lclose, lcont, ((int)LUAR_SLOT(d->for_a + 2)));
#line 806 "src/luar_emit_x64.dasc"
  if (pc_back)
    //| jmp =>((int)back)
    dasm_put(Dst, 1052, ((int)back));
#line 808 "src/luar_emit_x64.dasc"
  else
    //| jmp <1
  //|=>lclose:
  dasm_put(Dst, 1056, lclose);
#line 811 "src/luar_emit_x64.dasc"
}

static void emit_branch_code (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  int i;
  //| push rbx
  //| mov rbx, rdi
  dasm_put(Dst, 1062);
#line 818 "src/luar_emit_x64.dasc"
  if (d->has_table) {
    //| push r12
    //| push r13
    //| push r14
    dasm_put(Dst, 1068);
#line 822 "src/luar_emit_x64.dasc"
  }
  //| mov r11d, ((int)d->entry_pc)
  dasm_put(Dst, 1075, ((int)d->entry_pc));
#line 824 "src/luar_emit_x64.dasc"
  emit_guards(Dst, d);
  if (d->has_table) {
    //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->table_reg))]
    //| mov rax, qword [r12 + ((int)d->table_metatable_off)]
    //| test rax, rax
    //| jnz >3
    //| mov r13, qword [r12 + ((int)d->table_array_off)]
    //| mov r14d, dword [r12 + ((int)d->table_asize_off)]
    dasm_put(Dst, 1079, ((int)LUAR_SLOT(d->table_reg)), ((int)d->table_metatable_off), ((int)d->table_array_off), ((int)d->table_asize_off));
#line 832 "src/luar_emit_x64.dasc"
  }
  for (i = 0; i < d->nops; i++) {
    const LuaR_Op *op = &d->ops[i];
    //|=>((int)i):
    dasm_put(Dst, 7, ((int)i));
#line 836 "src/luar_emit_x64.dasc"
    if (op->kind == LUAR_OP_GETTABLE || op->kind == LUAR_OP_TABLE_ADD ||
        op->kind == LUAR_OP_SETTABLE || op->kind == LUAR_OP_GETI ||
        op->kind == LUAR_OP_TABLE_ADDI)
      //| mov r11d, ((int)op->pc)
      dasm_put(Dst, 1075, ((int)op->pc));
#line 840 "src/luar_emit_x64.dasc"
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
          dasm_put(Dst, 1052, ((int)(d->nops + 1)));
#line 851 "src/luar_emit_x64.dasc"
        else
          //| jmp =>((int)(i + 2))
          dasm_put(Dst, 1052, ((int)(i + 2)));
#line 853 "src/luar_emit_x64.dasc"
        break;
      case LUAR_OP_JMP:
        //| jmp =>((int)op->target)
        dasm_put(Dst, 1052, ((int)op->target));
#line 856 "src/luar_emit_x64.dasc"
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
      case LUAR_OP_GETI:
        emit_table_geti(Dst, d, op);
        break;
      case LUAR_OP_TABLE_ADDI:
        emit_table_addi(Dst, d, op);
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
      dasm_put(Dst, 1052, 0);
#line 883 "src/luar_emit_x64.dasc"
    if (d->repeat)
      //|=>((int)(d->nops + 1)):
      dasm_put(Dst, 7, ((int)(d->nops + 1)));
#line 885 "src/luar_emit_x64.dasc"
    else
      //|=>((int)d->nops):
    //| mov eax, 1
    dasm_put(Dst, 1109, ((int)d->nops));
#line 888 "src/luar_emit_x64.dasc"
    if (d->has_table) {
      //| pop r14
      //| pop r13
      //| pop r12
      dasm_put(Dst, 1116);
#line 892 "src/luar_emit_x64.dasc"
    }
    //| pop rbx
    //| ret
    dasm_put(Dst, 1123);
#line 895 "src/luar_emit_x64.dasc"
  }
  else {
    //|=>((int)d->nops):
    dasm_put(Dst, 7, ((int)d->nops));
#line 898 "src/luar_emit_x64.dasc"
    if (d->for_float) {
      emit_float_forloop(Dst, d, 0, 1);
    }
    else {
      //| mov rax, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
      //| test rax, rax
      //| jz >2
      //| dec rax
      //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], rax
      //| mov rcx, qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))]
      //| add rcx, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
      //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], rcx
      //| jmp =>0
      dasm_put(Dst, 1126, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)), ((int)LUAR_SLOT(d->for_a + 2)), 0);
#line 911 "src/luar_emit_x64.dasc"
    }
    //|=>((int)(d->nops + 1)):
    //|2:
    //| mov eax, 1
    dasm_put(Dst, 1161, ((int)(d->nops + 1)));
#line 915 "src/luar_emit_x64.dasc"
    if (d->has_table) {
      //| pop r14
      //| pop r13
      //| pop r12
      dasm_put(Dst, 1116);
#line 919 "src/luar_emit_x64.dasc"
    }
    //| pop rbx
    //| ret
    dasm_put(Dst, 1123);
#line 922 "src/luar_emit_x64.dasc"
  }
  //|3:
  //| mov eax, r11d
  //| add eax, 2
  //| neg eax
  dasm_put(Dst, 1170);
#line 927 "src/luar_emit_x64.dasc"
  if (d->has_table) {
    //| pop r14
    //| pop r13
    //| pop r12
    dasm_put(Dst, 1116);
#line 931 "src/luar_emit_x64.dasc"
  }
  //| pop rbx
  //| ret
  dasm_put(Dst, 1123);
#line 934 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1182);
#line 945 "src/luar_emit_x64.dasc"
  if (!d->ff_is_float)
    //| push r15
    dasm_put(Dst, 1194);
#line 947 "src/luar_emit_x64.dasc"
  emit_guards(Dst, d);
  //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->ff_idx))]
  //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  dasm_put(Dst, 1197, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->ff_idx)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 951 "src/luar_emit_x64.dasc"
  if (d->ff_is_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->ff_acc))]
    //| mov64 rax, bits
    //| movq xmm1, rax
    dasm_put(Dst, 1210, ((int)LUAR_SLOT(d->ff_acc)), (unsigned int)(bits), (unsigned int)((bits)>>32));
#line 955 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->ff_acc))]
    //| mov64 r11, d->ff_iconst
    dasm_put(Dst, 1226, ((int)LUAR_SLOT(d->ff_acc)), (unsigned int)(d->ff_iconst), (unsigned int)((d->ff_iconst)>>32));
#line 959 "src/luar_emit_x64.dasc"
  }
  //|1:
  dasm_put(Dst, 1235);
#line 961 "src/luar_emit_x64.dasc"
  if (d->ff_is_float) {
    //| cvtsi2sd xmm2, r13
    //| mulsd xmm2, xmm1
    dasm_put(Dst, 1238);
#line 964 "src/luar_emit_x64.dasc"
    if (d->ff_temp >= 0) {
      //| movsd qword [rbx + ((int)LUAR_SLOT(d->ff_temp))], xmm2
      //| mov byte [rbx + ((int)LUAR_TAG(d->ff_temp))], ((int)d->tag_flt)
      dasm_put(Dst, 1250, ((int)LUAR_SLOT(d->ff_temp)), ((int)LUAR_TAG(d->ff_temp)), ((int)d->tag_flt));
#line 967 "src/luar_emit_x64.dasc"
    }
    //| addsd xmm0, xmm2
    dasm_put(Dst, 1261);
#line 969 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov rcx, r13
    //| imul rcx, r11
    dasm_put(Dst, 1267);
#line 973 "src/luar_emit_x64.dasc"
    if (d->ff_temp >= 0) {
      //| mov qword [rbx + ((int)LUAR_SLOT(d->ff_temp))], rcx
      //| mov byte [rbx + ((int)LUAR_TAG(d->ff_temp))], ((int)d->tag_int)
      dasm_put(Dst, 705, ((int)LUAR_SLOT(d->ff_temp)), ((int)LUAR_TAG(d->ff_temp)), ((int)d->tag_int));
#line 976 "src/luar_emit_x64.dasc"
    }
    //| add r15, rcx
    dasm_put(Dst, 1276);
#line 978 "src/luar_emit_x64.dasc"
  }
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 1280, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 987 "src/luar_emit_x64.dasc"
  if (d->ff_is_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->ff_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->ff_acc)));
#line 989 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->ff_acc))], r15
    //| pop r15
    dasm_put(Dst, 1310, ((int)LUAR_SLOT(d->ff_acc)));
#line 993 "src/luar_emit_x64.dasc"
  }
  //| mov eax, 1
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  //|3:
  dasm_put(Dst, 1317);
#line 1001 "src/luar_emit_x64.dasc"
  if (!d->ff_is_float)
    //| pop r15
  //| xor eax, eax
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 1333);
#line 1009 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1346, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 1022 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1372, ((int)LUAR_SLOT(d->ff_table)), ((int)d->table_metatable_off), ((int)d->table_array_off), ((int)d->table_asize_off));
#line 1034 "src/luar_emit_x64.dasc"
  /* Lua 5.5 split array: value at array-key, tag at array+key+3. */
  //| mov rdx, r13
  //| shl rdx, 3
  //| neg rdx
  //| mov qword [r10 + rdx], r13
  //| mov byte [r10 + r13 + 3], ((int)d->tag_int)
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
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
  dasm_put(Dst, 1414, 3, ((int)d->tag_int), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 1065 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1346, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 1078 "src/luar_emit_x64.dasc"
  if (d->fi_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fi_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->fi_acc)));
#line 1080 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fi_acc))]
    dasm_put(Dst, 1504, ((int)LUAR_SLOT(d->fi_acc)));
#line 1083 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  dasm_put(Dst, 1235);
#line 1086 "src/luar_emit_x64.dasc"
  if (d->fi_float) {
    //| cvtsi2sd xmm1, r13
    //| addsd xmm0, xmm1
    dasm_put(Dst, 1509);
#line 1089 "src/luar_emit_x64.dasc"
  }
  else {
    //| add r15, r13
    dasm_put(Dst, 1521);
#line 1092 "src/luar_emit_x64.dasc"
  }
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 1280, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 1101 "src/luar_emit_x64.dasc"
  if (d->fi_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fi_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fi_acc)));
#line 1103 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fi_acc))], r15
    dasm_put(Dst, 1526, ((int)LUAR_SLOT(d->fi_acc)));
#line 1106 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1466, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 1124 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1531, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)), ((int)LUAR_SLOT(d->fm_acc)));
#line 1140 "src/luar_emit_x64.dasc"
  emit_mul_imm(Dst, d->fm_mult);
  if (d->fm_res >= 0) {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fm_res))], rax
    //| mov byte [rbx + ((int)LUAR_TAG(d->fm_res))], ((int)d->tag_int)
    dasm_put(Dst, 57, ((int)LUAR_SLOT(d->fm_res)), ((int)LUAR_TAG(d->fm_res)), ((int)d->tag_int));
#line 1144 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1566, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->fm_acc)));
#line 1170 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1632, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)), ((int)LUAR_SLOT(d->fc_upval_slot)), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->fc_upval_slot)));
#line 1211 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1346, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->ft_idx)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 1226 "src/luar_emit_x64.dasc"
  if (!isf) {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->ft_acc))]
    dasm_put(Dst, 1504, ((int)LUAR_SLOT(d->ft_acc)));
#line 1228 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->ft_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->ft_acc)));
#line 1231 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //| mov r8, qword [rbx + ((int)LUAR_SLOT(d->ft_table))]
  //| mov rax, qword [r8 + ((int)d->table_metatable_off)]
  //| test rax, rax
  //| jnz >3
  //| mov r9, qword [r8 + ((int)d->table_array_off)]
  //| mov r10d, dword [r8 + ((int)d->table_asize_off)]
  dasm_put(Dst, 1730, ((int)LUAR_SLOT(d->ft_table)), ((int)d->table_metatable_off), ((int)d->table_array_off), ((int)d->table_asize_off));
#line 1239 "src/luar_emit_x64.dasc"
  /* This fast path only handles step 1 and pre-validates the whole range. */
  //| cmp r14, 1
  //| jne >3
  //| cmp r13, 1
  //| jl >3
  //| mov rax, r13
  //| add rax, r12
  //| cmp rax, r10
  //| ja >3
  //| mov rcx, r9
  //| add rcx, r13
  //| add rcx, 3
  //| mov rdx, r12
  //| inc rdx
  //|4:
  //| cmp byte [rcx], ((int)d->tag_int)
  //| jne >3
  //| inc rcx
  //| dec rdx
  //| jnz <4
  //| mov r11, r13
  //| shl r11, 3
  //| neg r11
  //| add r11, r9
  //|1:
  //| mov rcx, qword [r11]
  dasm_put(Dst, 1754, ((int)d->tag_int));
#line 1265 "src/luar_emit_x64.dasc"
  /* The GETTABLE result is a real bytecode register and may be read after
  ** the loop ("local x; x = t[i]; s = s + x").  Materialise it exactly as
  ** the interpreter does; only the accumulator lives in r15/xmm0. */
  if (d->ft_res >= 0) {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->ft_res))], rcx
    //| mov byte [rbx + ((int)LUAR_TAG(d->ft_res))], ((int)d->tag_int)
    dasm_put(Dst, 705, ((int)LUAR_SLOT(d->ft_res)), ((int)LUAR_TAG(d->ft_res)), ((int)d->tag_int));
#line 1271 "src/luar_emit_x64.dasc"
  }
  if (!isf) {
    //| add r15, rcx
    dasm_put(Dst, 1276);
#line 1274 "src/luar_emit_x64.dasc"
  }
  else {
    //| cvtsi2sd xmm1, rcx
    //| addsd xmm0, xmm1
    dasm_put(Dst, 1845);
#line 1278 "src/luar_emit_x64.dasc"
  }
  //| sub r11, 8
  //| test r12, r12
  //| jz >2
  //| dec r12
  dasm_put(Dst, 1857);
#line 1283 "src/luar_emit_x64.dasc"
  /* This fast path is selected only for step 1. */
  //| inc r13
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 1874, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 1289 "src/luar_emit_x64.dasc"
  if (!isf) {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->ft_acc))], r15
    dasm_put(Dst, 1526, ((int)LUAR_SLOT(d->ft_acc)));
#line 1291 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->ft_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->ft_acc)));
#line 1294 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1893, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 1305 "src/luar_emit_x64.dasc"
  if (!isf) {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->ft_acc))], r15
    dasm_put(Dst, 1526, ((int)LUAR_SLOT(d->ft_acc)));
#line 1307 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->ft_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->ft_acc)));
#line 1310 "src/luar_emit_x64.dasc"
  }
  //| xor eax, eax
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 1491);
#line 1318 "src/luar_emit_x64.dasc"
}

static void emit_fast_while (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  int is_lt = (d->fw_kind == LUAR_OP_LT);
  //| push rbx
  //| mov rbx, rdi
  //| push r12
  //| push r13
  //| push r14
  dasm_put(Dst, 1182);
#line 1328 "src/luar_emit_x64.dasc"
  if (d->fw_float) {
    union { double d; uint64_t u; } cv;
    cv.d = d->fw_const;
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fw_acc))]
    //| mov64 rax, cv.u
    //| movq xmm2, rax
    dasm_put(Dst, 1919, ((int)LUAR_SLOT(d->fw_acc)), (unsigned int)(cv.u), (unsigned int)((cv.u)>>32));
#line 1334 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->fw_acc))]
    dasm_put(Dst, 1935, ((int)LUAR_SLOT(d->fw_acc)));
#line 1337 "src/luar_emit_x64.dasc"
  }
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->fw_idx))]
  dasm_put(Dst, 1940, ((int)LUAR_SLOT(d->fw_idx)));
#line 1339 "src/luar_emit_x64.dasc"
  if (d->fw_limit == 0xFF) {
    //| mov64 r14, d->fw_imm
    dasm_put(Dst, 1945, (unsigned int)(d->fw_imm), (unsigned int)((d->fw_imm)>>32));
#line 1341 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->fw_limit))]
    dasm_put(Dst, 1205, ((int)LUAR_SLOT(d->fw_limit)));
#line 1344 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  //| cmp r13, r14
  dasm_put(Dst, 1950);
#line 1348 "src/luar_emit_x64.dasc"
  if (is_lt) {
    if (d->fw_k) {
      //| jl >2
      dasm_put(Dst, 1957);
#line 1351 "src/luar_emit_x64.dasc"
    }
    else {
      //| jge >2
      dasm_put(Dst, 1962);
#line 1354 "src/luar_emit_x64.dasc"
    }
  }
  else {
    if (d->fw_k) {
      //| jle >2
      dasm_put(Dst, 1967);
#line 1359 "src/luar_emit_x64.dasc"
    }
    else {
      //| jg >2
      dasm_put(Dst, 1972);
#line 1362 "src/luar_emit_x64.dasc"
    }
  }
  if (d->fw_float) {
    //| cvtsi2sd xmm1, r13
    dasm_put(Dst, 1977);
#line 1366 "src/luar_emit_x64.dasc"
    if (d->fw_prod) {
      //| mulsd xmm0, xmm1
      dasm_put(Dst, 173);
#line 1368 "src/luar_emit_x64.dasc"
    }
    else if (d->fw_sub) {
      //| subsd xmm0, xmm1
      dasm_put(Dst, 167);
#line 1371 "src/luar_emit_x64.dasc"
    }
    else if (d->fw_div) {
      //| divsd xmm0, xmm1
      dasm_put(Dst, 179);
#line 1374 "src/luar_emit_x64.dasc"
    }
    else if (d->fw_square) {
      //| mov rcx, r13
      //| imul rcx, r13
      dasm_put(Dst, 1984);
#line 1378 "src/luar_emit_x64.dasc"
      if (d->fw_temp >= 0) {
        //| mov qword [rbx + ((int)LUAR_SLOT(d->fw_temp))], rcx
        //| mov byte [rbx + ((int)LUAR_TAG(d->fw_temp))], ((int)d->tag_int)
        dasm_put(Dst, 705, ((int)LUAR_SLOT(d->fw_temp)), ((int)LUAR_TAG(d->fw_temp)), ((int)d->tag_int));
#line 1381 "src/luar_emit_x64.dasc"
      }
      //| cvtsi2sd xmm1, rcx
      //| addsd xmm0, xmm1
      dasm_put(Dst, 1845);
#line 1384 "src/luar_emit_x64.dasc"
    }
    else if (d->fw_linear) {
      //| mulsd xmm1, xmm2
      dasm_put(Dst, 1993);
#line 1387 "src/luar_emit_x64.dasc"
      if (d->fw_temp >= 0) {
        //| movsd qword [rbx + ((int)LUAR_SLOT(d->fw_temp))], xmm1
        //| mov byte [rbx + ((int)LUAR_TAG(d->fw_temp))], ((int)d->tag_flt)
        dasm_put(Dst, 1999, ((int)LUAR_SLOT(d->fw_temp)), ((int)LUAR_TAG(d->fw_temp)), ((int)d->tag_flt));
#line 1390 "src/luar_emit_x64.dasc"
      }
      //| addsd xmm0, xmm1
      dasm_put(Dst, 161);
#line 1392 "src/luar_emit_x64.dasc"
    }
    else {
      //| addsd xmm0, xmm1
      dasm_put(Dst, 161);
#line 1395 "src/luar_emit_x64.dasc"
    }
  }
  else if (d->fw_prod) {
    //| imul r12, r13
    dasm_put(Dst, 2010);
#line 1399 "src/luar_emit_x64.dasc"
  }
  else if (d->fw_sub) {
    //| sub r12, r13
    dasm_put(Dst, 2015);
#line 1402 "src/luar_emit_x64.dasc"
  }
  else if (d->fw_square) {
    //| mov rcx, r13
    //| imul rcx, r13
    dasm_put(Dst, 1984);
#line 1406 "src/luar_emit_x64.dasc"
    if (d->fw_temp >= 0) {
      //| mov qword [rbx + ((int)LUAR_SLOT(d->fw_temp))], rcx
      //| mov byte [rbx + ((int)LUAR_TAG(d->fw_temp))], ((int)d->tag_int)
      dasm_put(Dst, 705, ((int)LUAR_SLOT(d->fw_temp)), ((int)LUAR_TAG(d->fw_temp)), ((int)d->tag_int));
#line 1409 "src/luar_emit_x64.dasc"
    }
    //| add r12, rcx
    dasm_put(Dst, 2020);
#line 1411 "src/luar_emit_x64.dasc"
  }
  else {
    //| add r12, r13
    dasm_put(Dst, 2024);
#line 1414 "src/luar_emit_x64.dasc"
  }
  //| add r13, d->fw_step
  //| jmp <1
  //|2:
  dasm_put(Dst, 2029, d->fw_step);
#line 1418 "src/luar_emit_x64.dasc"
  if (d->fw_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fw_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fw_acc)));
#line 1420 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fw_acc))], r12
    dasm_put(Dst, 2040, ((int)LUAR_SLOT(d->fw_acc)));
#line 1423 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 2045, ((int)LUAR_SLOT(d->fw_idx)));
#line 1438 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1346, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 1451 "src/luar_emit_x64.dasc"
  if (d->fs_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fs_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->fs_acc)));
#line 1453 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fs_acc))]
    dasm_put(Dst, 1504, ((int)LUAR_SLOT(d->fs_acc)));
#line 1456 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  //| mov rcx, r13
  //| imul rcx, r13
  dasm_put(Dst, 2075);
#line 1461 "src/luar_emit_x64.dasc"
  if (d->fs_temp >= 0) {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fs_temp))], rcx
    //| mov byte [rbx + ((int)LUAR_TAG(d->fs_temp))], ((int)d->tag_int)
    dasm_put(Dst, 705, ((int)LUAR_SLOT(d->fs_temp)), ((int)LUAR_TAG(d->fs_temp)), ((int)d->tag_int));
#line 1464 "src/luar_emit_x64.dasc"
  }
  if (d->fs_float) {
    //| cvtsi2sd xmm1, rcx
    //| addsd xmm0, xmm1
    dasm_put(Dst, 1845);
#line 1468 "src/luar_emit_x64.dasc"
  }
  else {
    //| add r15, rcx
    dasm_put(Dst, 1276);
#line 1471 "src/luar_emit_x64.dasc"
  }
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 1280, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 1480 "src/luar_emit_x64.dasc"
  if (d->fs_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fs_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fs_acc)));
#line 1482 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fs_acc))], r15
    dasm_put(Dst, 1526, ((int)LUAR_SLOT(d->fs_acc)));
#line 1485 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1602);
#line 1501 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1346, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 1514 "src/luar_emit_x64.dasc"
  if (d->fp_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fp_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->fp_acc)));
#line 1516 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fp_acc))]
    dasm_put(Dst, 1504, ((int)LUAR_SLOT(d->fp_acc)));
#line 1519 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  dasm_put(Dst, 1235);
#line 1522 "src/luar_emit_x64.dasc"
  if (d->fp_float) {
    //| cvtsi2sd xmm1, r13
    dasm_put(Dst, 1977);
#line 1524 "src/luar_emit_x64.dasc"
    if (d->fp_div)
      //| divsd xmm0, xmm1
      dasm_put(Dst, 179);
#line 1526 "src/luar_emit_x64.dasc"
    else
      //| mulsd xmm0, xmm1
      dasm_put(Dst, 173);
#line 1528 "src/luar_emit_x64.dasc"
  }
  else {
    //| imul r15, r13
    dasm_put(Dst, 2086);
#line 1531 "src/luar_emit_x64.dasc"
  }
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 1280, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 1540 "src/luar_emit_x64.dasc"
  if (d->fp_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fp_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fp_acc)));
#line 1542 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fp_acc))], r15
    dasm_put(Dst, 1526, ((int)LUAR_SLOT(d->fp_acc)));
#line 1545 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1602);
#line 1561 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1346, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 1574 "src/luar_emit_x64.dasc"
  if (d->fsb_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fsb_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->fsb_acc)));
#line 1576 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fsb_acc))]
    dasm_put(Dst, 1504, ((int)LUAR_SLOT(d->fsb_acc)));
#line 1579 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  dasm_put(Dst, 1235);
#line 1582 "src/luar_emit_x64.dasc"
  if (d->fsb_float) {
    //| cvtsi2sd xmm1, r13
    //| subsd xmm0, xmm1
    dasm_put(Dst, 2092);
#line 1585 "src/luar_emit_x64.dasc"
  }
  else {
    //| sub r15, r13
    dasm_put(Dst, 2104);
#line 1588 "src/luar_emit_x64.dasc"
  }
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 1280, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 1597 "src/luar_emit_x64.dasc"
  if (d->fsb_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fsb_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fsb_acc)));
#line 1599 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fsb_acc))], r15
    dasm_put(Dst, 1526, ((int)LUAR_SLOT(d->fsb_acc)));
#line 1602 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1602);
#line 1618 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 2109, ((int)LUAR_SLOT(d->fr_idx)));
#line 1629 "src/luar_emit_x64.dasc"
  if (d->fr_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fr_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->fr_acc)));
#line 1631 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fr_acc))]
    dasm_put(Dst, 1504, ((int)LUAR_SLOT(d->fr_acc)));
#line 1634 "src/luar_emit_x64.dasc"
  }
  if (d->fr_limit != 0xFF) {
    //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->fr_limit))]
    dasm_put(Dst, 1205, ((int)LUAR_SLOT(d->fr_limit)));
#line 1637 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov64 r14, d->fr_immediate
    dasm_put(Dst, 1945, (unsigned int)(d->fr_immediate), (unsigned int)((d->fr_immediate)>>32));
#line 1640 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  //| add r13, d->fr_step
  dasm_put(Dst, 2125, d->fr_step);
#line 1644 "src/luar_emit_x64.dasc"
  if (d->fr_float) {
    //| cvtsi2sd xmm1, r13
    dasm_put(Dst, 1977);
#line 1646 "src/luar_emit_x64.dasc"
    if (d->fr_op == 1) {
      //| subsd xmm0, xmm1
      dasm_put(Dst, 167);
#line 1648 "src/luar_emit_x64.dasc"
    }
    else if (d->fr_op == 2) {
      //| mulsd xmm0, xmm1
      dasm_put(Dst, 173);
#line 1651 "src/luar_emit_x64.dasc"
    }
    else if (d->fr_op == 3) {
      //| mov rcx, r13
      //| imul rcx, r13
      dasm_put(Dst, 1984);
#line 1655 "src/luar_emit_x64.dasc"
      if (d->fr_temp >= 0) {
        //| mov qword [rbx + ((int)LUAR_SLOT(d->fr_temp))], rcx
        //| mov byte [rbx + ((int)LUAR_TAG(d->fr_temp))], ((int)d->tag_int)
        dasm_put(Dst, 705, ((int)LUAR_SLOT(d->fr_temp)), ((int)LUAR_TAG(d->fr_temp)), ((int)d->tag_int));
#line 1658 "src/luar_emit_x64.dasc"
      }
      //| cvtsi2sd xmm1, rcx
      //| addsd xmm0, xmm1
      dasm_put(Dst, 1845);
#line 1661 "src/luar_emit_x64.dasc"
    }
    else if (d->fr_op == 4) {
      //| divsd xmm0, xmm1
      dasm_put(Dst, 179);
#line 1664 "src/luar_emit_x64.dasc"
    }
    else {
      //| addsd xmm0, xmm1
      dasm_put(Dst, 161);
#line 1667 "src/luar_emit_x64.dasc"
    }
  }
  else if (d->fr_op == 1) {
    //| sub r15, r13
    dasm_put(Dst, 2104);
#line 1671 "src/luar_emit_x64.dasc"
  }
  else if (d->fr_op == 2) {
    //| imul r15, r13
    dasm_put(Dst, 2086);
#line 1674 "src/luar_emit_x64.dasc"
  }
  else if (d->fr_op == 3) {
    //| mov rcx, r13
    //| imul rcx, r13
    dasm_put(Dst, 1984);
#line 1678 "src/luar_emit_x64.dasc"
    if (d->fr_temp >= 0) {
      //| mov qword [rbx + ((int)LUAR_SLOT(d->fr_temp))], rcx
      //| mov byte [rbx + ((int)LUAR_TAG(d->fr_temp))], ((int)d->tag_int)
      dasm_put(Dst, 705, ((int)LUAR_SLOT(d->fr_temp)), ((int)LUAR_TAG(d->fr_temp)), ((int)d->tag_int));
#line 1681 "src/luar_emit_x64.dasc"
    }
    //| add r15, rcx
    dasm_put(Dst, 1276);
#line 1683 "src/luar_emit_x64.dasc"
  }
  else {
    //| add r15, r13
    dasm_put(Dst, 1521);
#line 1686 "src/luar_emit_x64.dasc"
  }
  if (d->fr_idx_left) {
    //| cmp r13, r14
    dasm_put(Dst, 1952);
#line 1689 "src/luar_emit_x64.dasc"
  }
  else {
    //| cmp r14, r13
    dasm_put(Dst, 2132);
#line 1692 "src/luar_emit_x64.dasc"
  }
  if (kind == LUAR_OP_LT) {
    if (k) {
      //| jl <1
      dasm_put(Dst, 2137);
#line 1696 "src/luar_emit_x64.dasc"
    }
    else {
      //| jge <1
      dasm_put(Dst, 2142);
#line 1699 "src/luar_emit_x64.dasc"
    }
  }
  else if (kind == LUAR_OP_LE) {
    if (k) {
      //| jle <1
      dasm_put(Dst, 2147);
#line 1704 "src/luar_emit_x64.dasc"
    }
    else {
      //| jg <1
      dasm_put(Dst, 2152);
#line 1707 "src/luar_emit_x64.dasc"
    }
  }
  else if (kind == LUAR_OP_GT) {
    if (k) {
      //| jg <1
      dasm_put(Dst, 2152);
#line 1712 "src/luar_emit_x64.dasc"
    }
    else {
      //| jle <1
      dasm_put(Dst, 2147);
#line 1715 "src/luar_emit_x64.dasc"
    }
  }
  else if (kind == LUAR_OP_GE) {
    if (k) {
      //| jge <1
      dasm_put(Dst, 2142);
#line 1720 "src/luar_emit_x64.dasc"
    }
    else {
      //| jl <1
      dasm_put(Dst, 2137);
#line 1723 "src/luar_emit_x64.dasc"
    }
  }
  else {
    if (k) {
      //| je <1
      dasm_put(Dst, 2157);
#line 1728 "src/luar_emit_x64.dasc"
    }
    else {
      //| jne <1
      dasm_put(Dst, 2162);
#line 1731 "src/luar_emit_x64.dasc"
    }
  }
  //| mov qword [rbx + ((int)LUAR_SLOT(d->fr_idx))], r13
  dasm_put(Dst, 1305, ((int)LUAR_SLOT(d->fr_idx)));
#line 1734 "src/luar_emit_x64.dasc"
  if (d->fr_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fr_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fr_acc)));
#line 1736 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fr_acc))], r15
    dasm_put(Dst, 1526, ((int)LUAR_SLOT(d->fr_acc)));
#line 1739 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 2167);
#line 1754 "src/luar_emit_x64.dasc"
}

static void emit_partial_code (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  //| push rbx
  //| mov rbx, rdi
  dasm_put(Dst, 1062);
#line 1760 "src/luar_emit_x64.dasc"
  emit_guards(Dst, d);
  emit_body(Dst, d, 4);
  //| mov eax, -((int)d->exit_pc + 2)
  //| pop rbx
  //| ret
  //|4:
  //| mov eax, r11d
  //| add eax, 2
  //| neg eax
  //| pop rbx
  //| ret
  //|3:
  //| xor eax, eax
  //| pop rbx
  //| ret
  dasm_put(Dst, 2195, -((int)d->exit_pc + 2));
#line 1775 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1062);
#line 1833 "src/luar_emit_x64.dasc"
  emit_guards(Dst, d);
  //|1:
  dasm_put(Dst, 1235);
#line 1835 "src/luar_emit_x64.dasc"
  emit_body(Dst, d, 4);
  if (d->for_float) {
    emit_float_forloop(Dst, d, 1, 0);
  }
  else {
    /* Exact FORLOOP semantics: while count > 0, decrement, advance the
    ** index and run the body again.  When count is already zero the body
    ** just ran for the final iteration and the index must not advance. */
    //| mov rax, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
    //| test rax, rax
    //| jz >2
    //| dec rax
    //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], rax
    //| mov rcx, qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))]
    //| add rcx, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
    //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], rcx
    //| jmp <1
    dasm_put(Dst, 2219, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 1852 "src/luar_emit_x64.dasc"
  }
  //|2:
  //| mov eax, 1
  //| pop rbx
  //| ret
  //|4:
  //| mov eax, r11d
  //| add eax, 2
  //| neg eax
  //| pop rbx
  //| ret
  //|3:
  //| xor eax, eax
  //| pop rbx
  //| ret
  dasm_put(Dst, 2255);
#line 1867 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 2284, ((int)t->startpc));
#line 1881 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2314, ((int)in->pc), ((int)LUAR_TAG(in->a)), tag);
#line 1895 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_LOADI:
        //| mov64 rax, in->ival
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMINT
        dasm_put(Dst, 53, (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 1901 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_KBOOL:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov esi, ((int)in->nval)
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_kbool
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2326, ((int)LUAR_SLOT(in->a)), ((int)in->nval), (unsigned int)((int64_t)(intptr_t)&luar_trace_kbool), (unsigned int)(((int64_t)(intptr_t)&luar_trace_kbool)>>32), ((int)in->pc));
#line 1910 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2349, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_kstr), (unsigned int)(((int64_t)(intptr_t)&luar_trace_kstr)>>32), ((int)in->pc));
#line 1920 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_LOADF: {
        int64_t bits;
        memcpy(&bits, &in->nval, sizeof(bits));
        //| mov64 rax, bits
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMFLT
        dasm_put(Dst, 53, (unsigned int)(bits), (unsigned int)((bits)>>32), ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->a)), LUA_VNUMFLT);
#line 1927 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_ADDI:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(in->b))]
        //| add rax, in->ival
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMINT
        dasm_put(Dst, 72, ((int)LUAR_SLOT(in->b)), in->ival, ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 1934 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_SLOAD:
      case LUAR_IR_SSTORE:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov al, byte [rbx + ((int)LUAR_TAG(in->b))]
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], al
        dasm_put(Dst, 38, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->b)), ((int)LUAR_TAG(in->a)));
#line 1941 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2377, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)in->nval), (unsigned int)((int64_t)(intptr_t)&luar_trace_uload), (unsigned int)(((int64_t)(intptr_t)&luar_trace_uload)>>32), ((int)in->pc));
#line 1952 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_ADD:
      case LUAR_IR_SUB:
      case LUAR_IR_MUL:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(in->b))]
        dasm_put(Dst, 185, ((int)LUAR_SLOT(in->b)));
#line 1957 "src/luar_emit_x64.dasc"
        if (in->kind == LUAR_IR_ADD) {
          //| add rax, qword [rbx + ((int)LUAR_SLOT(in->c))]
          dasm_put(Dst, 233, ((int)LUAR_SLOT(in->c)));
#line 1959 "src/luar_emit_x64.dasc"
        }
        else if (in->kind == LUAR_IR_SUB) {
          //| sub rax, qword [rbx + ((int)LUAR_SLOT(in->c))]
          dasm_put(Dst, 238, ((int)LUAR_SLOT(in->c)));
#line 1962 "src/luar_emit_x64.dasc"
        }
        else {
          //| imul rax, qword [rbx + ((int)LUAR_SLOT(in->c))]
          dasm_put(Dst, 243, ((int)LUAR_SLOT(in->c)));
#line 1965 "src/luar_emit_x64.dasc"
        }
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMINT
        dasm_put(Dst, 57, ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 1968 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2407, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_cget_double), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_cget_double)>>32), ((int)in->pc));
#line 1978 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2436, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall0_double), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall0_double)>>32), ((int)in->pc));
#line 1989 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL0I:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall0_int
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2436, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall0_int), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall0_int)>>32), ((int)in->pc));
#line 1998 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2461, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall1di), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall1di)>>32), ((int)in->pc));
#line 2008 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2490, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall1dd), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall1dd)>>32), ((int)in->pc));
#line 2018 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2490, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall1ff), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall1ff)>>32), ((int)in->pc));
#line 2028 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2521, ((int)LUAR_SLOT(in->a)), ((int)in->ival), ((int)in->nval), (unsigned int)((int64_t)(intptr_t)&luar_trace_tnew), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tnew)>>32), ((int)in->pc));
#line 2039 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2549, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tlen), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tlen)>>32), ((int)in->pc));
#line 2049 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_STRLEN:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_strlen
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2436, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_strlen), (unsigned int)(((int64_t)(intptr_t)&luar_trace_strlen)>>32), ((int)in->pc));
#line 2058 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2577, ((int)in->a), ((int)in->ival), (unsigned int)((int64_t)(intptr_t)&luar_trace_concatn), (unsigned int)(((int64_t)(intptr_t)&luar_trace_concatn)>>32), ((int)in->pc));
#line 2069 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2604, ((int)in->a), (unsigned int)((int64_t)(intptr_t)&luar_trace_concat3), (unsigned int)(((int64_t)(intptr_t)&luar_trace_concat3)>>32), ((int)in->pc));
#line 2079 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2604, ((int)in->a), (unsigned int)((int64_t)(intptr_t)&luar_trace_concat2), (unsigned int)(((int64_t)(intptr_t)&luar_trace_concat2)>>32), ((int)in->pc));
#line 2089 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2629, ((int)LUAR_SLOT(in->b)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetkt), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetkt)>>32), ((int)in->pc));
#line 2099 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2629, ((int)LUAR_SLOT(in->b)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetkb), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetkb)>>32), ((int)in->pc));
#line 2109 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2629, ((int)LUAR_SLOT(in->b)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetks), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetks)>>32), ((int)in->pc));
#line 2119 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2629, ((int)LUAR_SLOT(in->b)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetkf), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetkf)>>32), ((int)in->pc));
#line 2129 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2629, ((int)LUAR_SLOT(in->b)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetk)>>32), ((int)in->pc));
#line 2139 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2658, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)in->ival), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetdynkb), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetdynkb)>>32), ((int)in->pc));
#line 2151 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2692, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)bits), (unsigned int)(((int64_t)bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetdynki), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetdynki)>>32), ((int)in->pc));
#line 2165 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2692, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)bits), (unsigned int)(((int64_t)bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetdynks), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetdynks)>>32), ((int)in->pc));
#line 2180 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2728, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(bits), (unsigned int)((bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetdynkf), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetdynkf)>>32), ((int)in->pc));
#line 2196 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2769, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetdyn), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetdyn)>>32), ((int)in->pc));
#line 2209 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2805, ((int)in->a), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetupkn), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetupkn)>>32), ((int)in->pc));
#line 2219 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2831, ((int)in->a), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetupv), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetupv)>>32), ((int)in->pc));
#line 2230 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2831, ((int)in->a), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetupt), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetupt)>>32), ((int)in->pc));
#line 2241 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2861, ((int)in->a), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)in->nval), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetupkb), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetupkb)>>32), ((int)in->pc));
#line 2252 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2889, ((int)in->a), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)bits), (unsigned int)(((int64_t)bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetupks), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetupks)>>32), ((int)in->pc));
#line 2265 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2889, ((int)in->a), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)bits), (unsigned int)(((int64_t)bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetupki), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetupki)>>32), ((int)in->pc));
#line 2279 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2919, ((int)in->a), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)(bits), (unsigned int)((bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetupkf), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetupkf)>>32), ((int)in->pc));
#line 2294 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2954, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkdb), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkdb)>>32), ((int)in->pc));
#line 2307 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2990, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)in->nval), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkb), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkb)>>32), ((int)in->pc));
#line 2319 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2954, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkt), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkt)>>32), ((int)in->pc));
#line 2331 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2954, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetks), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetks)>>32), ((int)in->pc));
#line 2343 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3024, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)bits), (unsigned int)(((int64_t)bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkks), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkks)>>32), ((int)in->pc));
#line 2357 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3024, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)bits), (unsigned int)(((int64_t)bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkkf), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkkf)>>32), ((int)in->pc));
#line 2372 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2954, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkf), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkf)>>32), ((int)in->pc));
#line 2385 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3024, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)in->nval), (unsigned int)(((int64_t)in->nval)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkk)>>32), ((int)in->pc));
#line 2397 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2954, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetk)>>32), ((int)in->pc));
#line 2409 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3060, ((int)LUAR_SLOT(in->b)), in->ival, ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgeti), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgeti)>>32), ((int)in->pc));
#line 2419 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2954, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)fn), (unsigned int)(((int64_t)(intptr_t)fn)>>32), ((int)in->pc));
#line 2441 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3089, ((int)LUAR_SLOT(in->a)), in->ival, ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tseti), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tseti)>>32), ((int)in->pc));
#line 2454 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3125, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2ii), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2ii)>>32), ((int)in->pc));
#line 2465 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3156, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2id), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2id)>>32), ((int)in->pc));
#line 2476 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3191, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2di), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2di)>>32), ((int)in->pc));
#line 2487 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3226, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2df), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2df)>>32), ((int)in->pc));
#line 2498 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3226, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2fd), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2fd)>>32), ((int)in->pc));
#line 2509 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3156, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2if), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2if)>>32), ((int)in->pc));
#line 2520 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3263, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2fi), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2fi)>>32), ((int)in->pc));
#line 2531 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3298, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2ff), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2ff)>>32), ((int)in->pc));
#line 2542 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3226, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2dd), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2dd)>>32), ((int)in->pc));
#line 2553 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3335, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(a2)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(a3)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall3idd), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall3idd)>>32), ((int)in->pc));
#line 2567 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3376, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(a2)), ((int)LUAR_SLOT(a3)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall3did), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall3did)>>32), ((int)in->pc));
#line 2582 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3417, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(a2)), ((int)LUAR_SLOT(a3)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall3ddi), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall3ddi)>>32), ((int)in->pc));
#line 2597 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3458, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(a2)), ((int)LUAR_SLOT(a3)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall3dd), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall3dd)>>32), ((int)in->pc));
#line 2612 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3501, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall1i), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall1i)>>32), ((int)in->pc), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 2624 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3534, ((int)LUAR_SLOT(in->a)), ((int64_t)in->ival), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)setfn), (unsigned int)(((int64_t)(intptr_t)setfn)>>32), ((int)in->pc));
#line 2640 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3534, ((int)LUAR_SLOT(in->b)), ((int64_t)in->ival), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)getfn), (unsigned int)(((int64_t)(intptr_t)getfn)>>32), ((int)in->pc));
#line 2659 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3567, ((int)LUAR_SLOT(in->a)), ((int64_t)in->ival), ((int64_t)in->b));
#line 2671 "src/luar_emit_x64.dasc"
        if (isf) {
          //| mov64 rax, bits
          //| movq xmm0, rax
          dasm_put(Dst, 89, (unsigned int)(bits), (unsigned int)((bits)>>32));
#line 2674 "src/luar_emit_x64.dasc"
        }
        else {
          //| mov rcx, ((int64_t)in->nval)
          dasm_put(Dst, 3580, ((int64_t)in->nval));
#line 2677 "src/luar_emit_x64.dasc"
        }
        //| mov64 rax, (int64_t)(intptr_t)cfn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2332, (unsigned int)((int64_t)(intptr_t)cfn), (unsigned int)(((int64_t)(intptr_t)cfn)>>32), ((int)in->pc));
#line 2683 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3585, ((int)LUAR_SLOT(in->a)), ((int64_t)in->ival), ((int64_t)in->nval), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)setfn), (unsigned int)(((int64_t)(intptr_t)setfn)>>32), ((int)in->pc));
#line 2698 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3585, ((int)LUAR_SLOT(in->b)), ((int64_t)in->ival), ((int64_t)in->nval), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)getfn), (unsigned int)(((int64_t)(intptr_t)getfn)>>32), ((int)in->pc));
#line 2713 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2407, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_cset_double), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_cset_double)>>32), ((int)in->pc));
#line 2724 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2407, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_cset_int), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_cset_int)>>32), ((int)in->pc));
#line 2734 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2407, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_set_i64), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_set_i64)>>32), ((int)in->pc));
#line 2744 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3618, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(divisor), (unsigned int)((divisor)>>32), (unsigned int)((int64_t)(intptr_t)fn), (unsigned int)(((int64_t)(intptr_t)fn)>>32), ((int)in->pc));
#line 2759 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3618, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(factor), (unsigned int)((factor)>>32), (unsigned int)((int64_t)(intptr_t)fn), (unsigned int)(((int64_t)(intptr_t)fn)>>32), ((int)in->pc));
#line 2778 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3618, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(count), (unsigned int)((count)>>32), (unsigned int)((int64_t)(intptr_t)fn), (unsigned int)(((int64_t)(intptr_t)fn)>>32), ((int)in->pc));
#line 2794 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2436, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)fn), (unsigned int)(((int64_t)(intptr_t)fn)>>32), ((int)in->pc));
#line 2808 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3618, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(divisor), (unsigned int)((divisor)>>32), (unsigned int)((int64_t)(intptr_t)fn), (unsigned int)(((int64_t)(intptr_t)fn)>>32), ((int)in->pc));
#line 2824 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3618, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(factor), (unsigned int)((factor)>>32), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_mul_i64), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_mul_i64)>>32), ((int)in->pc));
#line 2836 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3618, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(delta), (unsigned int)((delta)>>32), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_add_i64), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_add_i64)>>32), ((int)in->pc));
#line 2848 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2407, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_copy_i64), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_copy_i64)>>32), ((int)in->pc));
#line 2859 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2407, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_cget_int), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_cget_int)>>32), ((int)in->pc));
#line 2869 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TGET:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rsi, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        dasm_put(Dst, 3647, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)));
#line 2874 "src/luar_emit_x64.dasc"
        if (side) {
          //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tget_generic
          dasm_put(Dst, 3660, (unsigned int)((int64_t)(intptr_t)&luar_trace_tget_generic), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tget_generic)>>32));
#line 2876 "src/luar_emit_x64.dasc"
        }
        else {
          //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tget_int
          dasm_put(Dst, 3660, (unsigned int)((int64_t)(intptr_t)&luar_trace_tget_int), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tget_int)>>32));
#line 2879 "src/luar_emit_x64.dasc"
        }
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMINT
        dasm_put(Dst, 3517, ((int)in->pc), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 2885 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3665, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)tsetfn), (unsigned int)(((int64_t)(intptr_t)tsetfn)>>32), ((int)in->pc));
#line 2898 "src/luar_emit_x64.dasc"
        /* GC may have relocated the stack. */
        break;
      }
      case LUAR_IR_ADDK:
      case LUAR_IR_SUBK:
      case LUAR_IR_MULK:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(in->b))]
        dasm_put(Dst, 185, ((int)LUAR_SLOT(in->b)));
#line 2905 "src/luar_emit_x64.dasc"
        if (in->kind == LUAR_IR_ADDK) {
          //| add rax, in->ival
          dasm_put(Dst, 3701, in->ival);
#line 2907 "src/luar_emit_x64.dasc"
        }
        else if (in->kind == LUAR_IR_SUBK) {
          //| sub rax, in->ival
          dasm_put(Dst, 3706, in->ival);
#line 2910 "src/luar_emit_x64.dasc"
        }
        else {
          //| imul rax, in->ival
          dasm_put(Dst, 3711, in->ival);
#line 2913 "src/luar_emit_x64.dasc"
        }
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMINT
        dasm_put(Dst, 57, ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 2916 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CGUARD:
        //| mov r11d, ((int)in->pc)
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rcx, (int64_t)(in->ival)
        //| cmp rax, rcx
        //| jne >8
        dasm_put(Dst, 3716, ((int)in->pc), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(in->ival)), (unsigned int)(((int64_t)(in->ival))>>32));
#line 2923 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3501, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetf_generic), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetf_generic)>>32), ((int)in->pc), ((int)LUAR_TAG(in->a)), LUA_VNUMFLT);
#line 2934 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_ITOF:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_itof
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2436, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_itof), (unsigned int)(((int64_t)(intptr_t)&luar_trace_itof)>>32), ((int)in->pc));
#line 2943 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3735, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(bits), (unsigned int)((bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_faddk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_faddk)>>32), ((int)in->pc));
#line 2956 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3735, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(bits), (unsigned int)((bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_fmulk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_fmulk)>>32), ((int)in->pc));
#line 2970 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2407, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_trace_fadd), (unsigned int)(((int64_t)(intptr_t)&luar_trace_fadd)>>32), ((int)in->pc));
#line 2981 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 2407, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_trace_fsub), (unsigned int)(((int64_t)(intptr_t)&luar_trace_fsub)>>32), ((int)in->pc));
#line 2991 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3735, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(bits), (unsigned int)((bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_fdivk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_fdivk)>>32), ((int)in->pc));
#line 3004 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CALLEXIT:
        //| mov r11d, ((int)in->pc)
        //| jmp >8
        dasm_put(Dst, 3769, ((int)in->pc));
#line 3009 "src/luar_emit_x64.dasc"
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
          dasm_put(Dst, 3777, ((int)LUAR_SLOT(forslot)), ((int)LUAR_SLOT(forslot)), ((int)LUAR_SLOT(forslot + 2)), ((int)LUAR_SLOT(forslot + 1)), ((int)LUAR_SLOT(forslot + 2)));
#line 3036 "src/luar_emit_x64.dasc"
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
        dasm_put(Dst, 3838, ((int)LUAR_SLOT(forslot)), ((int)LUAR_SLOT(forslot)), ((int)LUAR_SLOT(forslot + 2)), ((int)LUAR_SLOT(forslot + 1)), ((int)LUAR_SLOT(forslot + 2)));
#line 3049 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 3882);
#line 3065 "src/luar_emit_x64.dasc"
  if (side) {
    //| mov eax, r11d
    //| neg eax
    //| sub eax, 2
    dasm_put(Dst, 3902);
#line 3069 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov eax, r11d
    //| neg eax
    //| sub eax, 2
    dasm_put(Dst, 3902);
#line 3074 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 3912);
#line 3096 "src/luar_emit_x64.dasc"
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
      if (op->kind == LUAR_OP_SHL || op->kind == LUAR_OP_SHR ||
          op->kind == LUAR_OP_SHLI) {
        unsigned need = 4096u + (unsigned)op->pc * 4u + 3u;
        if (need > maxpc)
          maxpc = need;
      }
      if (op->kind == LUAR_OP_MOD || op->kind == LUAR_OP_IDIV ||
          op->kind == LUAR_OP_MODK || op->kind == LUAR_OP_IDIVK) {
        unsigned need = 6000u + (unsigned)op->pc * 8u + 2u;
        if (need > maxpc)
          maxpc = need;
      }
    }
    if (d->for_float && maxpc < 9004)
      maxpc = 9004;
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
    dasm_put(Dst, 3701, imm);
#line 3215 "src/luar_emit_x64.dasc"
  else {
    //| mov64 rcx, imm
    //| add rax, rcx
    dasm_put(Dst, 3961, (unsigned int)(imm), (unsigned int)((imm)>>32));
#line 3218 "src/luar_emit_x64.dasc"
  }
}

static void emit_sub_imm (dasm_State **Dst, int64_t imm)
{
  if (imm32(imm))
    //| sub rax, imm
    dasm_put(Dst, 3706, imm);
#line 3225 "src/luar_emit_x64.dasc"
  else {
    //| mov64 rcx, imm
    //| sub rax, rcx
    dasm_put(Dst, 3969, (unsigned int)(imm), (unsigned int)((imm)>>32));
#line 3228 "src/luar_emit_x64.dasc"
  }
}

static void emit_mul_imm (dasm_State **Dst, int64_t imm)
{
  if (imm32(imm))
    //| imul rax, imm
    dasm_put(Dst, 3711, imm);
#line 3235 "src/luar_emit_x64.dasc"
  else {
    //| mov64 rcx, imm
    //| imul rax, rcx
    dasm_put(Dst, 3977, (unsigned int)(imm), (unsigned int)((imm)>>32));
#line 3238 "src/luar_emit_x64.dasc"
  }
}
