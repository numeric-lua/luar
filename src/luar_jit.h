/*
** luar_jit.h -- internal LuaR JIT/trace-recording interface.
**
** J1 (this stage) records hot loop backedges and snapshots the bytecode of
** the loop into a trace buffer.  It does not generate machine code: the
** interpreter remains the only execution engine and jit.status() stays
** false.  See doc/jit-design.md sections 7 and 10.
**
** See Copyright Notice in lua.h and COPYRIGHT.
*/

#ifndef luar_jit_h
#define luar_jit_h

#include <stddef.h>
#include <stdint.h>

#include "lua.h"

#include "lstate.h"
#include "lobject.h"


/* Backedge executions before a loop is considered hot (LuaJIT's default). */
#define LUAR_JIT_HOTCOUNT	56

/* Trace storage bounds.  J1 keeps bytecode snapshots, not Proto pointers,
** so a trace never keeps a collectable object alive or dangles. */
#define LUAR_JIT_MAXTRACES	64
#define LUAR_JIT_MAXINS		192

#define LUAR_JIT_MAXCODE	64

typedef struct LuaR_Code {
  void *code;
  size_t size;
} LuaR_Code;

typedef struct LuaR_Trace {
  uint32_t startpc;			/* first instruction (1-based) */
  uint32_t endpc;			/* backedge instruction */
  uint32_t nins;
  Instruction code[LUAR_JIT_MAXINS];	/* snapshot of the loop */
} LuaR_Trace;

typedef struct LuaR_JIT {
  uint32_t epoch;		/* bumped by jit.flush(): stale counters */
  uint32_t hotloops;		/* loops that reached the threshold */
  uint32_t traces;		/* recorded bytecode snapshots */
  uint32_t aborts;		/* loops too large / not decodable */
  int enabled;			/* jit.off() clears this */
  uint32_t compiled;		/* native loops emitted */
  size_t mcode;			/* executable bytes */
  int ncode;
  LuaR_Code code[LUAR_JIT_MAXCODE];
  LuaR_Trace trace[LUAR_JIT_MAXTRACES];
} LuaR_JIT;


/* Lifecycle (luar_jit.c). */
LUAI_FUNC void luari_jit_init (lua_State *L);
LUAI_FUNC void luari_jit_shutdown (lua_State *L);

/* Called once per loop when its counter reaches LUAR_JIT_HOTCOUNT. */
LUAI_FUNC int luari_jit_hotloop (lua_State *L, Proto *p,
                                const Instruction *backedge, void *base);

/* Try to run the compiled loop for 'p' on the given stack base.  Returns 1
** if the loop was completed natively, 0 to keep interpreting. */
LUAI_FUNC int luari_jit_run (lua_State *L, Proto *p, void *base);


/*
** Per-backedge fast path.  Once a Proto is hot the branch below is the only
** cost (one load and compare); the recorder is called once, at the
** threshold.  jit.flush() clears the trace list and stats but does not walk
** the heap to reset already-hot prototypes (J1 limitation; J2 keeps a
** prototype list).
*/
static inline int luari_jit_hot (lua_State *L, Proto *p,
                                const Instruction *backedge, void *base) {
  if (p->luar_hot >= LUAR_JIT_HOTCOUNT)
    return 0;
  if (++p->luar_hot < LUAR_JIT_HOTCOUNT)
    return 0;
  return luari_jit_hotloop(L, p, backedge, base);
}

#if defined(LUAR_JIT_DISABLE)
#define LUAR_JIT_HOT(L, p, pc, base)	((void)(base), (void)0)
#else
#define LUAR_JIT_HOT(L, p, pc, base)	luari_jit_hot((L), (p), (pc), (base))
#endif

#endif
