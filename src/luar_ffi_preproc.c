/*
** luar_ffi_preproc.c -- a small C preprocessor for ffi.cdef.
**
** Supports object-like #define/#undef, #ifdef/#ifndef/#if/#elif/#else/#endif
** and macro expansion outside strings/comments.  Function-like macros are
** supported; #include is resolved locally, and #pragma pack(n/push/pop)
** maps to a parser marker.  Other pragmas are ignored like a C compiler
** (they carry diagnostics/optimisation hints this parser cannot use).
**
** See Copyright Notice in lua.h and COPYRIGHT.
*/

#define LUA_LIB
#define LUAR_LIB

#include "lprefix.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"
#include "luar_ffi.h"
#include "luar_ffi_internal.h"

typedef struct Macro {
  struct Macro *next;
  char *name;
  char *body;
  char **params;
  int nparams;
  int is_func;
  int variadic;
} Macro;

#define MAX_MACRO_DEPTH 16
#define MAX_COND_DEPTH 32

typedef struct Cond {
  int parent;
  int active;
  int taken;      /* some branch in this group has been selected */
  int seen_else;
} Cond;

typedef struct Buf {
  char *p;
  size_t n;
  size_t cap;
} Buf;

static void buf_reserve (lua_State *L, Buf *b, size_t extra) {
  if (b->n + extra + 1 > b->cap) {
    size_t ncap = (b->cap == 0) ? 256 : b->cap;
    while (b->n + extra + 1 > ncap)
      ncap *= 2;
    b->p = (char *)realloc(b->p, ncap);
    if (b->p == NULL)
      luaL_error(L, "ffi: out of memory in preprocessor");
    b->cap = ncap;
  }
}

static void buf_char (lua_State *L, Buf *b, char c) {
  buf_reserve(L, b, 1);
  b->p[b->n++] = c;
}

static void buf_mem (lua_State *L, Buf *b, const char *s, size_t n) {
  buf_reserve(L, b, n);
  memcpy(b->p + b->n, s, n);
  b->n += n;
}

static char *preprocess_impl (lua_State *L, FFIState *S, const char *src,
                              size_t len, size_t *outlen, int incdepth,
                              const char *base);

/* Match a conditional directive name.  C permits '#if(expr)' without a
** space after the name and comments directly after '#else'/'#endif';
** recognising those forms keeps nested-group tracking exact when a skipped
** group contains them.  A name must not be followed by another identifier
** character, so '#ifdefX' is not '#ifdef'. */
static int dir_name_is (const char *q, size_t rn, const char *name,
                        int paren_ok, int comment_ok) {
  size_t n = strlen(name);
  if (rn < n || memcmp(q, name, n) != 0)
    return 0;
  if (rn == n)
    return 1;
  return isspace((unsigned char)q[n]) ||
         (paren_ok && q[n] == '(') ||
         (comment_ok && q[n] == '/');
}

/* Directives that must still be tracked while the surrounding group is
** inactive.  Everything else in a skipped group is discarded without
** parsing, exactly as a C preprocessor does. */
static int directive_is_conditional (const char *q, size_t rn) {
  return dir_name_is(q, rn, "if", 1, 0) ||
         dir_name_is(q, rn, "ifdef", 0, 0) ||
         dir_name_is(q, rn, "ifndef", 0, 0) ||
         dir_name_is(q, rn, "elif", 1, 0) ||
         dir_name_is(q, rn, "else", 0, 1) ||
         dir_name_is(q, rn, "endif", 0, 1);
}

static Macro *macro_find (FFIState *S, const char *name, size_t n) {
  Macro *m = (Macro *)S->macros;
  while (m != NULL) {
    if (strlen(m->name) == n && memcmp(m->name, name, n) == 0)
      return m;
    m = m->next;
  }
  return NULL;
}

static void macro_init (lua_State *L, FFIState *S, Macro *m,
                        const char *name, size_t n,
                        const char *body, size_t blen) {
  m->name = (char *)ffi_alloc(S, n + 1);
  memcpy(m->name, name, n);
  m->name[n] = '\0';
  m->body = (char *)ffi_alloc(S, blen + 1);
  memcpy(m->body, body, blen);
  m->body[blen] = '\0';
  m->params = NULL;
  m->nparams = 0;
  m->is_func = 0;
  m->variadic = 0;
  (void)L;
}

static void macro_set (lua_State *L, FFIState *S, const char *name, size_t n,
                       const char *body, size_t blen) {
  Macro *m = macro_find(S, name, n);
  if (m == NULL) {
    m = (Macro *)ffi_alloc(S, sizeof(Macro));
    macro_init(L, S, m, name, n, body, blen);
    m->next = (Macro *)S->macros;
    S->macros = m;
  }
  else {
    m->body = (char *)ffi_alloc(S, blen + 1);
    memcpy(m->body, body, blen);
    m->body[blen] = '\0';
    m->is_func = 0;
    m->nparams = 0;
    m->variadic = 0;
  }
}

static void macro_set_func (lua_State *L, FFIState *S, const char *name,
                            size_t n, char **params, int nparams, int variadic,
                            const char *body, size_t blen) {
  Macro *m = macro_find(S, name, n);
  if (m == NULL) {
    m = (Macro *)ffi_alloc(S, sizeof(Macro));
    macro_init(L, S, m, name, n, body, blen);
    m->next = (Macro *)S->macros;
    S->macros = m;
  }
  else {
    m->body = (char *)ffi_alloc(S, blen + 1);
    memcpy(m->body, body, blen);
    m->body[blen] = '\0';
  }
  {
    char **pcopy = NULL;
    int i;
    if (nparams > 0) {
      pcopy = (char **)ffi_alloc(S, sizeof(char *) * (size_t)nparams);
      for (i = 0; i < nparams; i++)
        pcopy[i] = ffi_strdup(S, params[i]);
    }
    m->params = pcopy;
  }
  m->nparams = nparams;
  m->is_func = 1;
  m->variadic = variadic;
}

static void macro_del (FFIState *S, const char *name, size_t n) {
  Macro **link = (Macro **)&S->macros;
  while (*link != NULL) {
    Macro *m = *link;
    if (strlen(m->name) == n && memcmp(m->name, name, n) == 0) {
      *link = m->next;
      return;
    }
    link = &m->next;
  }
}

static int ident_start (int c) {
  return isalpha((unsigned char)c) || c == '_';
}

static int ident_char (int c) {
  return isalnum((unsigned char)c) || c == '_';
}

static void expand (lua_State *L, FFIState *S, Buf *out, const char *s,
                    size_t n, int depth, int *block_comment,
                    int *in_string);

typedef struct MacroArg {
  const char *p;
  size_t n;
} MacroArg;

static size_t skip_spaces (const char *s, size_t n, size_t i) {
  while (i < n && (s[i] == ' ' || s[i] == '\t'))
    i++;
  return i;
}

static int match_close (lua_State *L, const char *s, size_t n, size_t open) {
  int depth = 1;
  size_t i = open + 1;
  while (i < n) {
    if (s[i] == '"' || s[i] == '\'') {
      char q = s[i++];
      while (i < n) {
        if (s[i] == '\\' && i + 1 < n) { i += 2; continue; }
        if (s[i++] == q) break;
      }
      continue;
    }
    if (s[i] == '/' && i + 1 < n && s[i + 1] == '/')
      break;
    if (s[i] == '/' && i + 1 < n && s[i + 1] == '*') {
      i += 2;
      while (i + 1 < n && !(s[i] == '*' && s[i + 1] == '/')) i++;
      i = (i + 1 < n) ? i + 2 : n;
      continue;
    }
    if (s[i] == '(') depth++;
    else if (s[i] == ')') {
      depth--;
      if (depth == 0) return (int)i;
    }
    i++;
  }
  luaL_error(L, "ffi.cdef: unterminated macro invocation");
  return -1;
}

static int split_args (lua_State *L, const char *s, size_t n,
                       int open, int close, MacroArg *args, int max) {
  (void)n;
  int argc = 0, depth = 0;
  size_t start = (size_t)open + 1;
  size_t i;
  size_t end = (size_t)close;
  if (skip_spaces(s, end, start) >= end) {
    args[0].p = s + start;
    args[0].n = 0;
    return 0;
  }
  for (i = start; i <= end; i++) {
    if (i == end || (s[i] == ',' && depth == 0)) {
      if (argc >= max)
        luaL_error(L, "ffi.cdef: too many macro arguments");
      args[argc].p = s + start;
      args[argc].n = i - start;
      while (args[argc].n > 0 &&
             (args[argc].p[0] == ' ' || args[argc].p[0] == '\t')) {
        args[argc].p++;
        args[argc].n--;
      }
      while (args[argc].n > 0 &&
             (args[argc].p[args[argc].n - 1] == ' ' ||
              args[argc].p[args[argc].n - 1] == '\t'))
        args[argc].n--;
      argc++;
      start = i + 1;
      if (i == end) break;
      continue;
    }
    if (s[i] == '"' || s[i] == '\'') {
      char q = s[i++];
      while (i < end) {
        if (s[i] == '\\' && i + 1 < end) { i += 2; continue; }
        if (s[i] == q) break;
        i++;
      }
      continue;
    }
    if (s[i] == '(') depth++;
    else if (s[i] == ')') depth--;
  }
  return argc;
}

static int param_index (Macro *m, const char *p, size_t n) {
  int i;
  for (i = 0; i < m->nparams; i++) {
    if (strlen(m->params[i]) == n && memcmp(m->params[i], p, n) == 0)
      return i;
  }
  return -1;
}

static void expand_func (lua_State *L, FFIState *S, Buf *out, Macro *m,
                         MacroArg *args, int nargs, int depth,
                         int *block_comment, int *in_string) {
  Buf sub, va;
  size_t i = 0, bn = strlen(m->body);
  int k;
  memset(&sub, 0, sizeof(sub));
  memset(&va, 0, sizeof(va));
  if ((m->variadic && nargs < m->nparams) ||
      (!m->variadic && nargs != m->nparams))
    luaL_error(L, "ffi.cdef: macro '%s' expects %d argument(s), got %d",
               m->name, m->nparams, nargs);
  for (k = m->nparams; k < nargs; k++) {
    if (k > m->nparams)
      buf_char(L, &va, ',');
    buf_char(L, &va, ' ');
    buf_mem(L, &va, args[k].p, args[k].n);
  }
  while (i < bn) {
    char c = m->body[i];
    if (c == '#' && i + 1 < bn && m->body[i + 1] == '#') {
      i += 2;
      while (i < bn && (m->body[i] == ' ' || m->body[i] == '\t'))
        i++;
      while (sub.n > 0 &&
             (sub.p[sub.n - 1] == ' ' || sub.p[sub.n - 1] == '\t'))
        sub.n--;
      continue;
    }
    if (c == '#') {
      size_t j = i + 1;
      int pi;
      while (j < bn && (m->body[j] == ' ' || m->body[j] == '\t'))
        j++;
      i = j;
      while (j < bn && ident_char((unsigned char)m->body[j]))
        j++;
      pi = param_index(m, m->body + i, j - i);
      if (pi < 0)
        luaL_error(L, "ffi.cdef: '#' must precede a macro parameter");
      buf_char(L, &sub, '"');
      {
        size_t k;
        for (k = 0; k < args[pi].n; k++) {
          char ac = args[pi].p[k];
          if (ac == '"' || ac == '\\')
            buf_char(L, &sub, '\\');
          buf_char(L, &sub, ac);
        }
      }
      buf_char(L, &sub, '"');
      i = j;
      continue;
    }
    if (c == '"' || c == '\'') {
      char q = c;
      buf_char(L, &sub, c);
      i++;
      while (i < bn) {
        c = m->body[i++];
        buf_char(L, &sub, c);
        if (c == '\\' && i < bn) {
          buf_char(L, &sub, m->body[i++]);
          continue;
        }
        if (c == q) break;
      }
      continue;
    }
    if (ident_start((unsigned char)c)) {
      size_t j = i + 1;
      int pi;
      while (j < bn && ident_char((unsigned char)m->body[j]))
        j++;
      if (m->variadic && j - i == 11 &&
          memcmp(m->body + i, "__VA_ARGS__", 11) == 0) {
        buf_mem(L, &sub, va.p != NULL ? va.p : "", va.n);
      }
      else {
        pi = param_index(m, m->body + i, j - i);
        if (pi >= 0)
          buf_mem(L, &sub, args[pi].p, args[pi].n);
        else
          buf_mem(L, &sub, m->body + i, j - i);
      }
      i = j;
      continue;
    }
    buf_char(L, &sub, c);
    i++;
  }
  if (depth >= MAX_MACRO_DEPTH)
    luaL_error(L, "ffi: macro expansion is too deep");
  expand(L, S, out, sub.p != NULL ? sub.p : "", sub.n, depth + 1,
         block_comment, in_string);
  free(sub.p);
  free(va.p);
}

static void expand (lua_State *L, FFIState *S, Buf *out, const char *s,
                    size_t n, int depth, int *block_comment,
                    int *in_string) {
  size_t i = 0;
  while (i < n) {
    char c = s[i];
    if (*block_comment) {
      buf_char(L, out, c);
      if (c == '*' && i + 1 < n && s[i + 1] == '/') {
        buf_char(L, out, '/');
        i += 2;
        *block_comment = 0;
      }
      else i++;
      continue;
    }
    if (*in_string) {
      buf_char(L, out, c);
      if (c == '\\' && i + 1 < n) {
        buf_char(L, out, s[i + 1]);
        i += 2;
        continue;
      }
      if (c == '"' || c == '\'')
        *in_string = 0;
      i++;
      continue;
    }
    if (c == '/' && i + 1 < n && s[i + 1] == '*') {
      buf_mem(L, out, "/*", 2);
      i += 2;
      *block_comment = 1;
      continue;
    }
    if (c == '/' && i + 1 < n && s[i + 1] == '/') {
      buf_mem(L, out, s + i, n - i);
      return;
    }
    if (c == '"' || c == '\'') {
      buf_char(L, out, c);
      *in_string = 1;
      i++;
      continue;
    }
    if (ident_start((unsigned char)c)) {
      size_t j = i + 1;
      Macro *m;
      while (j < n && ident_char((unsigned char)s[j]))
        j++;
      m = macro_find(S, s + i, j - i);
      if (m == NULL) {
        buf_mem(L, out, s + i, j - i);
        i = j;
        continue;
      }
      if (m->is_func) {
        size_t k = skip_spaces(s, n, j);
        if (k < n && s[k] == '(') {
          int close = match_close(L, s, n, k);
          MacroArg args[16];
          int nargs = split_args(L, s, n, (int)k, close, args, 16);
          expand_func(L, S, out, m, args, nargs, depth,
                      block_comment, in_string);
          i = (size_t)close + 1;
          continue;
        }
        buf_mem(L, out, s + i, j - i);
        i = j;
        continue;
      }
      if (depth >= MAX_MACRO_DEPTH)
        luaL_error(L, "ffi: macro expansion is too deep");
      expand(L, S, out, m->body, strlen(m->body), depth + 1,
             block_comment, in_string);
      i = j;
      continue;
    }
    buf_char(L, out, c);
    i++;
  }
}

static int is_space (int c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v';
}

static void trim (const char **p, size_t *n) {
  while (*n > 0 && is_space((unsigned char)(*p)[0])) {
    (*p)++;
    (*n)--;
  }
  while (*n > 0 && is_space((unsigned char)(*p)[*n - 1]))
    (*n)--;
}

static int parse_pack_num (const char *p, size_t n, int *out) {
  char buf[32];
  char *end = NULL;
  long v;
  trim(&p, &n);
  if (n == 0 || n >= sizeof(buf))
    return 0;
  memcpy(buf, p, n);
  buf[n] = '\0';
  v = strtol(buf, &end, 10);
  if (end == buf || *end != '\0' || v < 0 || v > 16 ||
      (v != 0 && (v & (v - 1)) != 0))
    return 0;
  *out = (int)v;
  return 1;
}


typedef struct IfExpr {
  lua_State *L;
  FFIState *S;
  const char *p;
  const char *end;
  int line;
} IfExpr;

static void if_error (IfExpr *E, const char *msg) {
  luaL_error(E->L, "ffi.cdef:%d: %s", E->line, msg);
}

static void if_skip (IfExpr *E) {
  while (E->p < E->end && isspace((unsigned char)*E->p))
    E->p++;
}

static int if_ident_char (int c) {
  return isalnum((unsigned char)c) || c == '_';
}

/* A preprocessor integer and its C signedness.  C evaluates #if operands
** as intmax_t/uintmax_t, and the usual arithmetic conversions make a mixed
** expression unsigned.  Keeping the flag is what makes
** '0xffffffffffffffff > 0' true instead of comparing it as -1. */
typedef struct IfNum {
  uint64_t u;
  int uns;
} IfNum;

static IfNum if_signed (int64_t v) {
  IfNum r;
  r.u = (uint64_t)v;
  r.uns = 0;
  return r;
}

static IfNum if_unsigned (uint64_t v) {
  IfNum r;
  r.u = v;
  r.uns = 1;
  return r;
}

/* C literal typing: decimal constants are signed unless suffixed 'u';
** hex/octal/binary constants pick the first type in
** int, unsigned int, long, unsigned long, long long, unsigned long long
** (restricted by the 'l'/'ll' suffix) that can represent the value. */
static IfNum if_number (uint64_t v, int base, int suffix_u, int suffix_l) {
  if (suffix_u)
    return if_unsigned(v);
  if (base == 10) {
    if (v <= (uint64_t)LLONG_MAX)
      return if_signed((int64_t)v);
    return if_unsigned(v);   /* GNU extension for oversized decimals */
  }
  if (suffix_l == 0) {
    if (v <= (uint64_t)INT_MAX) return if_signed((int64_t)v);
    if (v <= (uint64_t)UINT_MAX) return if_unsigned(v);
    if (v <= (uint64_t)LONG_MAX) return if_signed((int64_t)v);
    if (v <= (uint64_t)ULONG_MAX) return if_unsigned(v);
    if (v <= (uint64_t)LLONG_MAX) return if_signed((int64_t)v);
    return if_unsigned(v);
  }
  if (suffix_l == 1) {
    if (v <= (uint64_t)LONG_MAX) return if_signed((int64_t)v);
    if (v <= (uint64_t)ULONG_MAX) return if_unsigned(v);
    if (v <= (uint64_t)LLONG_MAX) return if_signed((int64_t)v);
    return if_unsigned(v);
  }
  if (v <= (uint64_t)LLONG_MAX)
    return if_signed((int64_t)v);
  return if_unsigned(v);
}

static IfNum if_parse_number (IfExpr *E) {
  const char *s = E->p;
  const char *q = s;
  uint64_t v = 0;
  int base = 10;
  int suffix_u = 0, suffix_l = 0;
  if (q + 1 < E->end && q[0] == '0' &&
      (q[1] == 'x' || q[1] == 'X')) {
    q += 2;
    base = 16;
    if (q >= E->end || !isxdigit((unsigned char)*q))
      if_error(E, "malformed hexadecimal constant in #if");
  }
  else if (q + 1 < E->end && q[0] == '0' &&
           (q[1] == 'b' || q[1] == 'B')) {
    q += 2;
    base = 2;
    if (q >= E->end || (*q != '0' && *q != '1'))
      if_error(E, "malformed binary constant in #if");
  }
  else if (q[0] == '0' && q + 1 < E->end && isdigit((unsigned char)q[1])) {
    base = 8;
  }
  {
    int nd = 0;
    while (q < E->end) {
      int d;
      if (*q >= '0' && *q <= '9') d = *q - '0';
      else if (*q >= 'a' && *q <= 'f') d = *q - 'a' + 10;
      else if (*q >= 'A' && *q <= 'F') d = *q - 'A' + 10;
      else break;
      if (d >= base)
        if_error(E, "malformed integer constant in #if");
      v = v * (uint64_t)base + (uint64_t)d;
      q++;
      nd++;
    }
    if (nd == 0)
      if_error(E, "expected an integer constant in #if");
  }
  while (q < E->end && (*q == 'u' || *q == 'U' ||
                        *q == 'l' || *q == 'L')) {
    if (*q == 'u' || *q == 'U')
      suffix_u = 1;
    else
      suffix_l++;
    q++;
  }
  if (q < E->end && if_ident_char((unsigned char)*q))
    if_error(E, "malformed integer constant in #if");
  E->p = q;
  return if_number(v, base, suffix_u, suffix_l);
}

static IfNum if_parse_char (IfExpr *E) {
  const char *q = E->p + 1;
  unsigned int value = 0;
  int nchars = 0;
  while (q < E->end && *q != '\'') {
    unsigned int ch;
    if (*q == '\n')
      if_error(E, "unterminated character constant in #if");
    if (*q == '\\') {
      q++;
      if (q >= E->end)
        if_error(E, "unterminated character constant in #if");
      switch (*q) {
        case 'a': ch = 7; q++; break;
        case 'b': ch = 8; q++; break;
        case 'f': ch = 12; q++; break;
        case 'n': ch = 10; q++; break;
        case 'r': ch = 13; q++; break;
        case 't': ch = 9; q++; break;
        case 'v': ch = 11; q++; break;
        case '\\': ch = '\\'; q++; break;
        case '\'': ch = '\''; q++; break;
        case '"': ch = '"'; q++; break;
        case '?': ch = '?'; q++; break;
        case 'x': {
          unsigned int hv = 0;
          int nd = 0;
          q++;
          while (q < E->end && isxdigit((unsigned char)*q)) {
            int d = (*q <= '9') ? *q - '0'
                  : (tolower((unsigned char)*q) - 'a' + 10);
            hv = (hv << 4) | (unsigned int)d;
            q++;
            nd++;
          }
          if (nd == 0)
            if_error(E, "malformed hexadecimal escape in #if");
          ch = hv & 0xffu;
          break;
        }
        case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7': {
          unsigned int ov = 0;
          int nd = 0;
          while (q < E->end && nd < 3 && *q >= '0' && *q <= '7') {
            ov = (ov << 3) | (unsigned int)(*q - '0');
            q++;
            nd++;
          }
          ch = ov & 0xffu;
          break;
        }
        default:
          if_error(E, "unsupported escape in #if character constant");
          ch = 0;
          break;
      }
    }
    else {
      ch = (unsigned char)*q;
      q++;
    }
    if (nchars == 0)
      value = ch;
    nchars++;
  }
  if (q >= E->end || *q != '\'')
    if_error(E, "unterminated character constant in #if");
  if (nchars == 0)
    if_error(E, "empty character constant in #if");
  if (nchars > 1)
    if_error(E, "multi-character constants are not supported in #if");
  E->p = q + 1;
  return if_signed((int64_t)(signed char)value);
}

static IfNum if_parse_expr (IfExpr *E, int minprec);

static IfNum if_parse_unary (IfExpr *E) {
  IfNum v;
  if_skip(E);
  if (E->p < E->end && *E->p == '!') {
    E->p++;
    return if_signed(!(if_parse_unary(E).u != 0));
  }
  if (E->p < E->end && *E->p == '~') {
    E->p++;
    v = if_parse_unary(E);
    v.u = ~v.u;
    return v;
  }
  if (E->p < E->end && *E->p == '-') {
    E->p++;
    v = if_parse_unary(E);
    v.u = 0 - v.u;   /* C wraps both signed and unsigned unary minus */
    return v;
  }
  if (E->p < E->end && *E->p == '+') {
    E->p++;
    return if_parse_unary(E);
  }
  if (E->p < E->end && *E->p == '(') {
    E->p++;
    v = if_parse_expr(E, 1);
    if_skip(E);
    if (E->p >= E->end || *E->p != ')')
      if_error(E, "unbalanced parentheses in #if");
    E->p++;
    return v;
  }
  if (E->p < E->end && *E->p == '\'')
    return if_parse_char(E);
  if (E->p < E->end && isdigit((unsigned char)*E->p))
    return if_parse_number(E);
  if (E->p < E->end && (isalpha((unsigned char)*E->p) || *E->p == '_')) {
    /* C evaluates an unknown identifier in #if as zero. */
    while (E->p < E->end && if_ident_char((unsigned char)*E->p))
      E->p++;
    return if_signed(0);
  }
  if_error(E, "unsupported #if expression");
  return if_signed(0);
}

static int if_binop (IfExpr *E, int *op, int *olen) {
  const char *p = E->p;
  const char *end = E->end;
  if (p + 1 < end) {
    if (p[0] == '|' && p[1] == '|') { *op = 1; *olen = 2; return 1; }
    if (p[0] == '&' && p[1] == '&') { *op = 2; *olen = 2; return 1; }
    if (p[0] == '=' && p[1] == '=') { *op = 6; *olen = 2; return 1; }
    if (p[0] == '!' && p[1] == '=') { *op = 7; *olen = 2; return 1; }
    if (p[0] == '<' && p[1] == '=') { *op = 9; *olen = 2; return 1; }
    if (p[0] == '>' && p[1] == '=') { *op = 10; *olen = 2; return 1; }
    if (p[0] == '<' && p[1] == '<') { *op = 11; *olen = 2; return 1; }
    if (p[0] == '>' && p[1] == '>') { *op = 12; *olen = 2; return 1; }
  }
  switch (p < end ? *p : '\0') {
    case '|': *op = 3; *olen = 1; return 1;
    case '^': *op = 4; *olen = 1; return 1;
    case '&': *op = 5; *olen = 1; return 1;
    case '<': *op = 8; *olen = 1; return 1;
    case '>': *op = 13; *olen = 1; return 1;
    case '+': *op = 14; *olen = 1; return 1;
    case '-': *op = 15; *olen = 1; return 1;
    case '*': *op = 16; *olen = 1; return 1;
    case '/': *op = 17; *olen = 1; return 1;
    case '%': *op = 18; *olen = 1; return 1;
    default: return 0;
  }
}

static int if_prec (int op) {
  switch (op) {
    case 1: return 1;
    case 2: return 2;
    case 3: return 3;
    case 4: return 4;
    case 5: return 5;
    case 6: case 7: return 6;
    case 8: case 9: case 10: case 13: return 7;
    case 11: case 12: return 8;
    case 14: case 15: return 9;
    case 16: case 17: case 18: return 10;
    default: return 0;
  }
}

static IfNum if_parse_expr (IfExpr *E, int minprec) {
  IfNum lhs = if_parse_unary(E);
  for (;;) {
    int op, olen, prec, uns;
    IfNum rhs;
    if_skip(E);
    if (!if_binop(E, &op, &olen))
      break;
    prec = if_prec(op);
    if (prec < minprec)
      break;
    E->p += olen;
    rhs = if_parse_expr(E, prec + 1);
    uns = lhs.uns || rhs.uns;
    switch (op) {
      case 1: lhs = if_signed(lhs.u != 0 || rhs.u != 0); break;
      case 2: lhs = if_signed(lhs.u != 0 && rhs.u != 0); break;
      case 3: lhs.u = lhs.u | rhs.u; lhs.uns = uns; break;
      case 4: lhs.u = lhs.u ^ rhs.u; lhs.uns = uns; break;
      case 5: lhs.u = lhs.u & rhs.u; lhs.uns = uns; break;
      case 6: lhs = if_signed(lhs.u == rhs.u); break;
      case 7: lhs = if_signed(lhs.u != rhs.u); break;
      case 8:
        lhs = if_signed(uns ? (lhs.u < rhs.u)
                            : ((int64_t)lhs.u < (int64_t)rhs.u));
        break;
      case 9:
        lhs = if_signed(uns ? (lhs.u <= rhs.u)
                            : ((int64_t)lhs.u <= (int64_t)rhs.u));
        break;
      case 10:
        lhs = if_signed(uns ? (lhs.u >= rhs.u)
                            : ((int64_t)lhs.u >= (int64_t)rhs.u));
        break;
      case 11:
        if (rhs.uns ? (rhs.u >= 64)
                    : ((int64_t)rhs.u < 0 || (int64_t)rhs.u >= 64))
          if_error(E, "shift count out of range in #if");
        lhs.u = lhs.u << (unsigned)rhs.u;
        break;
      case 12:
        if (rhs.uns ? (rhs.u >= 64)
                    : ((int64_t)rhs.u < 0 || (int64_t)rhs.u >= 64))
          if_error(E, "shift count out of range in #if");
        if (lhs.uns)
          lhs.u = lhs.u >> (unsigned)rhs.u;
        else
          lhs.u = (uint64_t)((int64_t)lhs.u >> (unsigned)rhs.u);
        break;
      case 13:
        lhs = if_signed(uns ? (lhs.u > rhs.u)
                            : ((int64_t)lhs.u > (int64_t)rhs.u));
        break;
      case 14: lhs.u = lhs.u + rhs.u; lhs.uns = uns; break;
      case 15: lhs.u = lhs.u - rhs.u; lhs.uns = uns; break;
      case 16: lhs.u = lhs.u * rhs.u; lhs.uns = uns; break;
      case 17:
        if (rhs.u == 0)
          if_error(E, "division by zero in #if");
        if (uns)
          lhs.u = lhs.u / rhs.u;
        else {
          int64_t a = (int64_t)lhs.u, b = (int64_t)rhs.u;
          lhs = if_signed((a == INT64_MIN && b == -1) ? INT64_MIN : a / b);
        }
        break;
      case 18:
        if (rhs.u == 0)
          if_error(E, "division by zero in #if");
        if (uns)
          lhs.u = lhs.u % rhs.u;
        else {
          int64_t a = (int64_t)lhs.u, b = (int64_t)rhs.u;
          lhs = if_signed((a == INT64_MIN && b == -1) ? 0 : a % b);
        }
        break;
      default: break;
    }
  }
  return lhs;
}

/* Replace defined NAME / defined(NAME) before macro expansion, so the
** operand is never expanded as a macro. */
static void if_replace_defined (lua_State *L, FFIState *S, const char *p,
                                size_t n, Buf *out) {
  size_t i = 0;
  while (i < n) {
    char c = p[i];
    if (c == '"' || c == '\'') {
      char quote = c;
      buf_char(L, out, c);
      i++;
      while (i < n) {
        buf_char(L, out, p[i]);
        if (p[i] == '\\' && i + 1 < n) {
          buf_char(L, out, p[i + 1]);
          i += 2;
          continue;
        }
        if (p[i] == quote) { i++; break; }
        i++;
      }
      continue;
    }
    if (isalpha((unsigned char)c) || c == '_') {
      size_t j = i + 1;
      size_t k;
      while (j < n && (isalnum((unsigned char)p[j]) || p[j] == '_'))
        j++;
      if (j - i == 7 && memcmp(p + i, "defined", 7) == 0) {
        const char *name;
        size_t nn;
        k = j;
        while (k < n && isspace((unsigned char)p[k]))
          k++;
        if (k < n && p[k] == '(') {
          k++;
          while (k < n && isspace((unsigned char)p[k]))
            k++;
        }
        if (k >= n || !(isalpha((unsigned char)p[k]) || p[k] == '_'))
          luaL_error(L, "ffi.cdef: expected a macro name after 'defined'");
        name = p + k;
        k++;
        while (k < n && (isalnum((unsigned char)p[k]) || p[k] == '_'))
          k++;
        nn = (size_t)(p + k - name);
        while (k < n && isspace((unsigned char)p[k]))
          k++;
        if (k < n && p[k] == ')')
          k++;
        buf_char(L, out, macro_find(S, name, nn) != NULL ? '1' : '0');
        i = k;
        continue;
      }
      buf_mem(L, out, p + i, j - i);
      i = j;
      continue;
    }
    buf_char(L, out, c);
    i++;
  }
}

static int eval_if (lua_State *L, FFIState *S, const char *expr, size_t n,
                    int line) {
  Buf def, ex;
  IfExpr E;
  IfNum v;
  int bc = 0, ins = 0;
  memset(&def, 0, sizeof(def));
  memset(&ex, 0, sizeof(ex));
  if_replace_defined(L, S, expr, n, &def);
  expand(L, S, &ex, def.p != NULL ? def.p : "", def.n, 0, &bc, &ins);
  buf_char(L, &ex, '\0');
  E.L = L;
  E.S = S;
  E.p = ex.p;
  E.end = ex.p + ex.n - 1;
  E.line = line;
  v = if_parse_expr(&E, 1);
  if_skip(&E);
  if (E.p != E.end)
    luaL_error(L, "ffi.cdef:%d: unsupported #if expression", line);
  free(def.p);
  free(ex.p);
  return v.u != 0;
}

static char *preprocess_impl (lua_State *L, FFIState *S, const char *src,
                              size_t len, size_t *outlen, int incdepth,
                              const char *base) {
  Buf b;
  Cond cond[MAX_COND_DEPTH];
  int depth = 0, active = 1;
  int pack = 0, pack_stack[16], pack_depth = 0;
  int block_comment = 0, in_string = 0;
  size_t pos = 0;
  int line = 1;
  int has_directive = (memchr(src, '#', len) != NULL);
  if (!has_directive && S->macros == NULL)
    return NULL;
  memset(&b, 0, sizeof(b));
  while (pos < len) {
    size_t end = pos;
    const char *p;
    size_t n;
    int directive = 0;
    while (end < len && src[end] != '\n')
      end++;
    p = src + pos;
    n = end - pos;
    if (!block_comment && !in_string) {
      const char *q = p;
      size_t qn = n;
      while (qn > 0 && is_space((unsigned char)*q)) {
        q++;
        qn--;
      }
      if (qn > 0 && *q == '#')
        directive = 1;
    }
    if (directive) {
      const char *q = p;
      size_t rn = n;
      while (rn > 0 && is_space((unsigned char)*q)) {
        q++;
        rn--;
      }
      q++;                       /* '#' */
      rn--;
      while (rn > 0 && is_space((unsigned char)*q)) {
        q++;
        rn--;
      }
      if (!active && !directive_is_conditional(q, rn)) {
        /* A skipped group must not parse #define/#include/#pragma/#error
        ** bodies (or unknown directives); only the conditional stack is
        ** maintained until its #endif. */
        buf_char(L, &b, '\n');
        pos = (end < len) ? end + 1 : end;
        line++;
        continue;
      }
      if (rn >= 6 && memcmp(q, "define", 6) == 0 &&
          (rn == 6 || is_space((unsigned char)q[6]))) {
        const char *name, *body;
        size_t nn, bn;
        q += 6;
        rn -= 6;
        while (rn > 0 && is_space((unsigned char)*q)) { q++; rn--; }
        name = q;
        while (rn > 0 && ident_char((unsigned char)*q)) { q++; rn--; }
        nn = (size_t)(q - name);
        if (nn == 0)
          luaL_error(L, "ffi.cdef:%d: #define needs a name", line);
        if (rn > 0 && *q == '(') {
          char *params[16];
          int np = 0, variadic = 0;
          q++;
          rn--;
          for (;;) {
            while (rn > 0 && is_space((unsigned char)*q)) { q++; rn--; }
            if (rn > 0 && *q == ')') { q++; rn--; break; }
            if (rn >= 3 && memcmp(q, "...", 3) == 0) {
              variadic = 1;
              q += 3;
              rn -= 3;
              while (rn > 0 && is_space((unsigned char)*q)) { q++; rn--; }
              if (rn == 0 || *q != ')')
                luaL_error(L, "ffi.cdef:%d: '...' must be the last macro "
                               "parameter", line);
              q++;
              rn--;
              break;
            }
            if (rn == 0 || !ident_start((unsigned char)*q))
              luaL_error(L, "ffi.cdef:%d: expected macro parameter", line);
            {
              const char *pn = q;
              size_t pl;
              while (rn > 0 && ident_char((unsigned char)*q)) { q++; rn--; }
              pl = (size_t)(q - pn);
              if (np >= 16)
                luaL_error(L, "ffi.cdef:%d: too many macro parameters", line);
              params[np] = (char *)ffi_alloc(S, pl + 1);
              memcpy(params[np], pn, pl);
              params[np][pl] = '\0';
              np++;
            }
            while (rn > 0 && is_space((unsigned char)*q)) { q++; rn--; }
            if (rn > 0 && *q == ',') { q++; rn--; continue; }
            if (rn > 0 && *q == ')') { q++; rn--; break; }
            luaL_error(L, "ffi.cdef:%d: expected ',' or ')' in macro", line);
          }
          body = q;
          bn = rn;
          trim(&body, &bn);
          if (active)
            macro_set_func(L, S, name, nn, params, np, variadic, body, bn);
        }
        else {
          body = q;
          bn = rn;
          trim(&body, &bn);
          if (active)
            macro_set(L, S, name, nn, body, bn);
        }
      }
      else if (rn >= 5 && memcmp(q, "undef", 5) == 0 &&
               (rn == 5 || is_space((unsigned char)q[5]))) {
        const char *name;
        size_t nn;
        q += 5;
        rn -= 5;
        while (rn > 0 && is_space((unsigned char)*q)) { q++; rn--; }
        name = q;
        while (rn > 0 && ident_char((unsigned char)*q)) { q++; rn--; }
        nn = (size_t)(q - name);
        if (nn == 0)
          luaL_error(L, "ffi.cdef:%d: #undef needs a name", line);
        if (active)
          macro_del(S, name, nn);
      }
      else if (dir_name_is(q, rn, "ifdef", 0, 0) ||
               dir_name_is(q, rn, "ifndef", 0, 0)) {
        int neg = (q[2] == 'n');
        const char *name;
        size_t nn;
        int has;
        q += neg ? 6 : 5;
        rn -= neg ? 6 : 5;
        while (rn > 0 && is_space((unsigned char)*q)) { q++; rn--; }
        name = q;
        while (rn > 0 && ident_char((unsigned char)*q)) { q++; rn--; }
        nn = (size_t)(q - name);
        if (nn == 0)
          luaL_error(L, "ffi.cdef:%d: conditional needs a name", line);
        has = macro_find(S, name, nn) != NULL;
        if (depth >= MAX_COND_DEPTH)
          luaL_error(L, "ffi.cdef:%d: conditionals nested too deeply", line);
        cond[depth].parent = active;
        cond[depth].seen_else = 0;
        cond[depth].active = active && (neg ? !has : has);
        cond[depth].taken = cond[depth].active;
        active = cond[depth].active;
        depth++;
      }
      else if (dir_name_is(q, rn, "if", 1, 0)) {
        int result = 0;
        q += 2;
        rn -= 2;
        if (active)
          result = eval_if(L, S, q, rn, line);
        if (depth >= MAX_COND_DEPTH)
          luaL_error(L, "ffi.cdef:%d: conditionals nested too deeply", line);
        cond[depth].parent = active;
        cond[depth].seen_else = 0;
        cond[depth].active = active && result;
        cond[depth].taken = cond[depth].active;
        active = cond[depth].active;
        depth++;
      }
      else if (dir_name_is(q, rn, "elif", 1, 0)) {
        Cond *c;
        if (depth == 0)
          luaL_error(L, "ffi.cdef:%d: #elif without #if", line);
        c = &cond[depth - 1];
        if (c->seen_else)
          luaL_error(L, "ffi.cdef:%d: #elif after #else", line);
        if (c->parent && !c->taken) {
          q += 4;
          rn -= 4;
          c->active = eval_if(L, S, q, rn, line);
          c->taken = c->active;
        }
        else {
          /* A skipped group's expression is not evaluated, so a malformed
          ** #elif in a dead branch cannot fail the whole cdef. */
          c->active = 0;
        }
        active = c->active;
      }
      else if (dir_name_is(q, rn, "else", 0, 1)) {
        Cond *c;
        if (depth == 0)
          luaL_error(L, "ffi.cdef:%d: #else without #if", line);
        c = &cond[depth - 1];
        if (c->seen_else)
          luaL_error(L, "ffi.cdef:%d: duplicate #else", line);
        c->seen_else = 1;
        c->active = c->parent && !c->taken;
        c->taken = 1;
        active = c->active;
      }
      else if (dir_name_is(q, rn, "endif", 0, 1)) {
        if (depth == 0)
          luaL_error(L, "ffi.cdef:%d: #endif without #if", line);
        depth--;
        active = (depth == 0) ? 1 : cond[depth - 1].active;
      }
      else if (rn >= 6 && memcmp(q, "pragma", 6) == 0 &&
               (rn == 6 || is_space((unsigned char)q[6]))) {
        const char *pp = q + 6;
        size_t pn = rn - 6;
        int newpack = pack;
        trim(&pp, &pn);
        if (pn < 4 || memcmp(pp, "pack", 4) != 0 ||
            (pn > 4 && ident_char((unsigned char)pp[4]))) {
          /* LUAR: non-pack pragmas are diagnostics/optimisation hints that
          ** this parser cannot act on.  Ignore them as a C compiler does,
          ** instead of rejecting an otherwise usable header. */
        }
        else {
        pp += 4;
        pn -= 4;
        trim(&pp, &pn);
        if (pn == 0 || *pp != '(')
          luaL_error(L, "ffi.cdef:%d: expected #pragma pack(...)", line);
        pp++;
        pn--;
        trim(&pp, &pn);
        if (pn > 0 && pp[pn - 1] == ')') {
          pn--;
          trim(&pp, &pn);
        }
        else
          luaL_error(L, "ffi.cdef:%d: unterminated #pragma pack", line);
        if (pn == 0)
          newpack = 0;
        else if (pn >= 4 && memcmp(pp, "push", 4) == 0 &&
                 (pn == 4 || is_space((unsigned char)pp[4]) ||
                  pp[4] == ',')) {
          const char *arg = pp + 4;
          size_t an = pn - 4;
          int v;
          trim(&arg, &an);
          if (an > 0) {
            if (*arg != ',')
              luaL_error(L, "ffi.cdef:%d: expected ',' after pack push", line);
            arg++; an--;
            if (!parse_pack_num(arg, an, &v))
              luaL_error(L, "ffi.cdef:%d: bad #pragma pack push value", line);
            newpack = v;
          }
          if (active) {
            if (pack_depth >= 16)
              luaL_error(L, "ffi.cdef:%d: #pragma pack nested too deeply", line);
            pack_stack[pack_depth++] = pack;
            pack = newpack;
          }
        }
        else if (pn == 3 && memcmp(pp, "pop", 3) == 0) {
          if (active) {
            if (pack_depth == 0)
              luaL_error(L, "ffi.cdef:%d: #pragma pack(pop) without push",
                         line);
            newpack = pack_stack[--pack_depth];
            pack = newpack;
          }
        }
        else {
          int v;
          if (!parse_pack_num(pp, pn, &v))
            luaL_error(L, "ffi.cdef:%d: bad #pragma pack value", line);
          newpack = v;
          if (active)
            pack = newpack;
        }
        if (active) {
          char marker[48];
          snprintf(marker, sizeof(marker), "__luar_pack(%d);\n", pack);
          buf_mem(L, &b, marker, strlen(marker));
        }
        }   /* pack pragma */
      }
      else if (rn >= 7 && memcmp(q, "include", 7) == 0 &&
               (rn == 7 || is_space((unsigned char)q[7]))) {
        const char *pp = q + 7;
        size_t pn = rn - 7;
        const char *path;
        size_t plen;
        FILE *f;
        char *filebuf;
        long fsz = 0;
        size_t got;
        trim(&pp, &pn);
        {
          char close;
          if (pn < 2 || (pp[0] != '"' && pp[0] != '<'))
            luaL_error(L, "ffi.cdef:%d: expected #include \"file\" or "
                          "<file>", line);
          close = (pp[0] == '<') ? '>' : '"';
          pp++;
          pn--;
          path = pp;
          plen = 0;
          while (plen < pn && pp[plen] != close)
            plen++;
          if (plen == pn)
            luaL_error(L, "ffi.cdef:%d: unterminated #include path", line);
        }
        if (active) {
          char *fname;
          char *incout;
          size_t inclen = 0;
          if (incdepth >= 16)
            luaL_error(L, "ffi.cdef:%d: #include nesting too deep", line);
          {
            size_t bl = (base != NULL) ? strlen(base) : 0;
            int joined = (bl > 0 && path[0] != '/');
            fname = (char *)malloc((joined ? bl + 1 : 0) + plen + 1);
            if (fname == NULL)
              luaL_error(L, "ffi.cdef: out of memory");
            if (joined) {
              memcpy(fname, base, bl);
              fname[bl] = '/';
              memcpy(fname + bl + 1, path, plen);
              fname[bl + 1 + plen] = '\0';
            }
            else {
              memcpy(fname, path, plen);
              fname[plen] = '\0';
            }
          }
          f = fopen(fname, "rb");
          if (f == NULL && base != NULL && path[0] != '/') {
            /* fall back to the working-directory form */
            free(fname);
            fname = (char *)malloc(plen + 1);
            if (fname == NULL)
              luaL_error(L, "ffi.cdef: out of memory");
            memcpy(fname, path, plen);
            fname[plen] = '\0';
            f = fopen(fname, "rb");
          }
          if (f == NULL && path[0] != '/') {
            static const char *const envs[] = {
              "LUAR_FFI_INCLUDE", "CPATH", "C_INCLUDE_PATH"
            };
            size_t ei;
            for (ei = 0; ei < sizeof(envs) / sizeof(envs[0]) && f == NULL;
                 ei++) {
              const char *dirs = getenv(envs[ei]);
              while (dirs != NULL && *dirs != '\0') {
                const char *colon = strchr(dirs, ':');
                size_t dl = (colon != NULL) ? (size_t)(colon - dirs)
                                            : strlen(dirs);
                if (dl > 0) {
                  char *cand = (char *)malloc(dl + 1 + plen + 1);
                  if (cand != NULL) {
                    memcpy(cand, dirs, dl);
                    cand[dl] = '/';
                    memcpy(cand + dl + 1, path, plen);
                    cand[dl + 1 + plen] = '\0';
                    f = fopen(cand, "rb");
                    if (f != NULL) {
                      free(fname);
                      fname = cand;
                      break;
                    }
                    free(cand);
                  }
                }
                dirs = (colon != NULL) ? colon + 1 : NULL;
              }
            }
          }
          if (f == NULL && path[0] != '/') {
            static const char *const system_dirs[] = {
              "/usr/local/include", "/usr/include"
            };
            size_t si;
            for (si = 0; si < sizeof(system_dirs) / sizeof(system_dirs[0]) &&
                         f == NULL; si++) {
              const char *d = system_dirs[si];
              size_t dl = strlen(d);
              char *cand = (char *)malloc(dl + 1 + plen + 1);
              if (cand != NULL) {
                memcpy(cand, d, dl);
                cand[dl] = '/';
                memcpy(cand + dl + 1, path, plen);
                cand[dl + 1 + plen] = '\0';
                f = fopen(cand, "rb");
                if (f != NULL) {
                  free(fname);
                  fname = cand;
                }
                else
                  free(cand);
              }
            }
          }
          if (f == NULL)
            luaL_error(L, "ffi.cdef:%d: cannot open include '%s'", line,
                       fname);
          if (fseek(f, 0, SEEK_END) != 0 || (fsz = ftell(f)) < 0 ||
              fsz > (1 << 20)) {
            fclose(f);
            luaL_error(L, "ffi.cdef:%d: include '%s' is too large or "
                          "unreadable", line, fname);
          }
          rewind(f);
          filebuf = (char *)malloc((size_t)fsz + 1);
          if (filebuf == NULL) {
            fclose(f);
            free(fname);
            luaL_error(L, "ffi.cdef: out of memory");
          }
          got = fread(filebuf, 1, (size_t)fsz, f);
          fclose(f);
          if (got != (size_t)fsz) {
            free(filebuf);
            luaL_error(L, "ffi.cdef:%d: short read from include '%s'", line,
                       fname);
          }
          filebuf[got] = '\0';
          {
            char *newbase = NULL;
            char *slash = strrchr(fname, '/');
            if (slash != NULL) {
              size_t blen = (size_t)(slash - fname);
              newbase = (char *)malloc(blen + 2);
              if (newbase != NULL) {
                if (blen == 0) {
                  newbase[0] = '/';
                  newbase[1] = '\0';
                }
                else {
                  memcpy(newbase, fname, blen);
                  newbase[blen] = '\0';
                }
              }
            }
            incout = preprocess_impl(L, S, filebuf, got, &inclen,
                                     incdepth + 1, newbase);
            free(newbase);
          }
          if (incout != NULL)
            buf_mem(L, &b, incout, inclen);
          else
            buf_mem(L, &b, filebuf, got);
          free(incout);
          free(filebuf);
          free(fname);
        }
      }
      else if (rn >= 5 && memcmp(q, "error", 5) == 0) {
        if (active) {
          const char *msg = q + 5;
          size_t mn = rn - 5;
          Buf eb;
          int bc = 0, ins = 0;
          trim(&msg, &mn);
          memset(&eb, 0, sizeof(eb));
          expand(L, S, &eb, msg, mn, 0, &bc, &ins);
          buf_char(L, &eb, '\0');
          luaL_error(L, "ffi.cdef:%d: #error %s", line, eb.p);
        }
      }
      else {
        luaL_error(L, "ffi.cdef:%d: unsupported preprocessor directive", line);
      }
      buf_char(L, &b, '\n');
    }
    else if (active) {
      expand(L, S, &b, p, n, 0, &block_comment, &in_string);
      buf_char(L, &b, '\n');
    }
    else {
      buf_char(L, &b, '\n');
    }
    pos = (end < len) ? end + 1 : end;
    line++;
  }
  if (depth != 0)
    luaL_error(L, "ffi.cdef: unterminated conditional");
  buf_char(L, &b, '\0');
  *outlen = b.n - 1;
  return b.p;
}


char *ffi_preprocess (lua_State *L, FFIState *S, const char *src, size_t len,
                      size_t *outlen) {
  return preprocess_impl(L, S, src, len, outlen, 0, NULL);
}
