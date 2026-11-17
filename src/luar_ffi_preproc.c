/*
** luar_ffi_preproc.c -- a small C preprocessor for ffi.cdef.
**
** Supports object-like #define/#undef, #ifdef/#ifndef/#if/#else/#endif
** and macro expansion outside strings/comments.  Function-like macros,
** #include and #pragma are rejected with clear errors rather than guessed.
**
** See Copyright Notice in lua.h and COPYRIGHT.
*/

#define LUA_LIB
#define LUAR_LIB

#include "lprefix.h"

#include <ctype.h>
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

static int eval_if (lua_State *L, FFIState *S, const char *expr, size_t n,
                    int line) {
  const char *p = expr;
  size_t len = n;
  trim(&p, &len);
  if (len == 1 && (p[0] == '0' || p[0] == '1'))
    return p[0] == '1';
  if (len > 8 && memcmp(p, "defined(", 8) == 0 && p[len - 1] == ')') {
    const char *name = p + 8;
    size_t nn = len - 9;
    trim(&name, &nn);
    return macro_find(S, name, nn) != NULL;
  }
  if (len > 9 && memcmp(p, "!defined(", 9) == 0 && p[len - 1] == ')') {
    const char *name = p + 9;
    size_t nn = len - 10;
    trim(&name, &nn);
    return macro_find(S, name, nn) == NULL;
  }
  {
    /* Expand object macros, then accept a decimal integer literal. */
    Buf b;
    char *end = NULL;
    long v;
    int bc = 0, ins = 0;
    memset(&b, 0, sizeof(b));
    expand(L, S, &b, p, len, 0, &bc, &ins);
    buf_char(L, &b, '\0');
    v = strtol(b.p, &end, 0);
    if (end == b.p || *end != '\0')
      luaL_error(L, "ffi.cdef:%d: unsupported #if expression", line);
    free(b.p);
    return v != 0;
  }
}

char *ffi_preprocess (lua_State *L, FFIState *S, const char *src, size_t len,
                      size_t *outlen) {
  Buf b;
  Cond cond[MAX_COND_DEPTH];
  int depth = 0, active = 1;
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
      while (n > 0 && is_space((unsigned char)*q)) {
        q++;
        n--;
      }
      if (n > 0 && *q == '#')
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
          if (bn > 0 && memchr(body, '#', bn) != NULL)
            luaL_error(L, "ffi.cdef:%d: macro stringification/pasting is not "
                           "supported", line);
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
      else if ((rn >= 5 && memcmp(q, "ifdef", 5) == 0 &&
                (rn == 5 || is_space((unsigned char)q[5]))) ||
               (rn >= 6 && memcmp(q, "ifndef", 6) == 0 &&
                (rn == 6 || is_space((unsigned char)q[6])))) {
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
        active = cond[depth].active;
        depth++;
      }
      else if (rn >= 2 && memcmp(q, "if", 2) == 0 &&
               (rn == 2 || is_space((unsigned char)q[2]))) {
        int result;
        q += 2;
        rn -= 2;
        result = eval_if(L, S, q, rn, line);
        if (depth >= MAX_COND_DEPTH)
          luaL_error(L, "ffi.cdef:%d: conditionals nested too deeply", line);
        cond[depth].parent = active;
        cond[depth].seen_else = 0;
        cond[depth].active = active && result;
        active = cond[depth].active;
        depth++;
      }
      else if (rn >= 4 && memcmp(q, "else", 4) == 0 &&
               (rn == 4 || is_space((unsigned char)q[4]))) {
        if (depth == 0)
          luaL_error(L, "ffi.cdef:%d: #else without #if", line);
        if (cond[depth - 1].seen_else)
          luaL_error(L, "ffi.cdef:%d: duplicate #else", line);
        cond[depth - 1].seen_else = 1;
        cond[depth - 1].active = cond[depth - 1].parent &&
                                 !cond[depth - 1].active;
        active = cond[depth - 1].active;
      }
      else if (rn >= 5 && memcmp(q, "endif", 5) == 0 &&
               (rn == 5 || is_space((unsigned char)q[5]))) {
        if (depth == 0)
          luaL_error(L, "ffi.cdef:%d: #endif without #if", line);
        depth--;
        active = (depth == 0) ? 1 : cond[depth - 1].active;
      }
      else if (rn >= 5 && memcmp(q, "error", 5) == 0) {
        if (active) {
          const char *msg = q + 5;
          size_t mn = rn - 5;
          trim(&msg, &mn);
          luaL_error(L, "ffi.cdef:%d: #error %.*s", line, (int)mn, msg);
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
