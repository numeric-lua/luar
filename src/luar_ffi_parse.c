/*
** luar_ffi_parse.c -- C declaration lexer and recursive-descent parser.
**
** Implements the subset of C declarations needed by the FFI:
**   - primitive types and qualifiers
**   - typedef, struct, union, enum (named/anonymous/nested)
**   - pointers, fixed and variable arrays, function types
**   - multiple declarators and multiple cdef declarations
**
** Deliberately unsupported (clear errors): preprocessor directives,
** __attribute__/__declspec, bitfields, long double, _Complex, _Atomic.
**
** See Copyright Notice in lua.h and COPYRIGHT.
*/

#define LUA_LIB
#define LUAR_LIB

#include "lprefix.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "luar_ffi.h"
#include "luar_ffi_internal.h"


/* ------------------------------------------------------------------ */
/* Lexer                                                              */
/* ------------------------------------------------------------------ */

enum TokenKind {
  TK_EOF,
  TK_IDENT,
  TK_INT,
  TK_CHAR,
  TK_FLOAT,
  TK_PUNCT
};

/* Multi-character punctuators start above the ASCII range. */
enum Punct {
  PT_ELLIPSIS = 256,
  PT_SHL,
  PT_SHR,
  PT_LE,
  PT_GE,
  PT_EQ,
  PT_NE,
  PT_ANDAND,
  PT_OROR,
  PT_ARROW
};

typedef struct Token {
  enum TokenKind kind;
  const char *start;
  size_t len;
  int64_t ival;
  double dval;
  int punct;
} Token;

typedef struct Parser {
  lua_State *L;
  FFIState *S;
  const char *src;
  const char *p;
  const char *end;
  const char *fname;
  int line;
  int mode;			/* 0 = cdef declarations, 1 = type expression */
  int depth;			/* recursion guard for nested declarations */
  Token tok;
} Parser;


static void parser_error (Parser *P, const char *fmt, ...) {
  char msg[400];
  char near[80];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);
  if (P->tok.kind == TK_EOF) {
    snprintf(near, sizeof(near), "end of input");
  }
  else {
    size_t n = P->tok.len < 40 ? P->tok.len : 40;
    snprintf(near, sizeof(near), "'%.*s'", (int)n, P->tok.start);
  }
  luaL_error(P->L, "%s:%d: %s near %s",
             P->fname, P->line, msg, near);
}


static void lex_error (Parser *P, const char *msg) {
  char near[80];
  if (P->p >= P->end) {
    snprintf(near, sizeof(near), "end of input");
  }
  else {
    size_t n = 0;
    while (P->p + n < P->end && P->p[n] != '\n' && n < 32)
      n++;
    snprintf(near, sizeof(near), "'%.*s'", (int)n, P->p);
  }
  luaL_error(P->L, "%s:%d: %s near %s",
             P->fname, P->line, msg, near);
}


static void skip_space (Parser *P) {
  for (;;) {
    while (P->p < P->end && isspace((unsigned char)*P->p)) {
      if (*P->p == '\n')
        P->line++;
      P->p++;
    }
    if (P->p + 1 < P->end && P->p[0] == '/' && P->p[1] == '/') {
      P->p += 2;
      while (P->p < P->end && *P->p != '\n')
        P->p++;
      continue;
    }
    if (P->p + 1 < P->end && P->p[0] == '/' && P->p[1] == '*') {
      int startline = P->line;
      P->p += 2;
      while (P->p + 1 < P->end &&
             !(P->p[0] == '*' && P->p[1] == '/')) {
        if (*P->p == '\n')
          P->line++;
        P->p++;
      }
      if (P->p + 1 >= P->end) {
        P->line = startline;
        lex_error(P, "unterminated comment");
      }
      P->p += 2;
      continue;
    }
    if (P->p < P->end && *P->p == '#')
      lex_error(P, "C preprocessor directives are not supported "
                   "(ffi.cdef takes declarations, not headers)");
    break;
  }
}


static int ident_start (int c) {
  return isalpha((unsigned char)c) || c == '_';
}

static int ident_char (int c) {
  return isalnum((unsigned char)c) || c == '_';
}


static void lex_next (Parser *P) {
  const char *s;
  skip_space(P);
  P->tok.start = P->p;
  P->tok.len = 0;
  P->tok.punct = 0;
  if (P->p >= P->end) {
    P->tok.kind = TK_EOF;
    return;
  }
  s = P->p;
  if (ident_start((unsigned char)*s)) {
    const char *q = s + 1;
    while (q < P->end && ident_char((unsigned char)*q))
      q++;
    P->tok.kind = TK_IDENT;
    P->tok.start = s;
    P->tok.len = (size_t)(q - s);
    P->p = q;
    return;
  }
  if (*s == '\'') {
    /* C character constant.  Luar keeps LuaJIT's declaration parser
    ** semantics: one character (plain or escaped), sign-extended through
    ** plain 'char'.  Multi-character constants are rejected. */
    const char *q = s + 1;
    unsigned int value = 0;
    int nchars = 0;
    while (q < P->end && *q != '\'') {
      unsigned int ch;
      if (*q == '\n')
        lex_error(P, "unterminated character constant");
      if (*q == '\\') {
        q++;
        if (q >= P->end)
          lex_error(P, "unterminated character constant");
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
            while (q < P->end && isxdigit((unsigned char)*q)) {
              int d = (*q <= '9') ? *q - '0'
                    : (tolower((unsigned char)*q) - 'a' + 10);
              hv = (hv << 4) | (unsigned int)d;
              q++;
              nd++;
            }
            if (nd == 0)
              lex_error(P, "malformed hexadecimal escape");
            ch = hv & 0xffu;
            break;
          }
          case '0': case '1': case '2': case '3':
          case '4': case '5': case '6': case '7': {
            unsigned int ov = 0;
            int nd = 0;
            while (q < P->end && nd < 3 && *q >= '0' && *q <= '7') {
              ov = (ov << 3) | (unsigned int)(*q - '0');
              q++;
              nd++;
            }
            ch = ov & 0xffu;
            break;
          }
          default:
            lex_error(P, "unsupported escape in character constant");
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
    if (q >= P->end || *q != '\'')
      lex_error(P, "unterminated character constant");
    if (nchars == 0)
      lex_error(P, "empty character constant");
    if (nchars > 1)
      lex_error(P, "multi-character constants are not supported");
    q++;
    P->tok.kind = TK_CHAR;
    P->tok.start = s;
    P->tok.len = (size_t)(q - s);
    P->tok.ival = (int64_t)(signed char)value;
    P->p = q;
    return;
  }
  if (isdigit((unsigned char)*s) ||
      (*s == '.' && s + 1 < P->end && isdigit((unsigned char)s[1]))) {
    const char *q = s;
    const char *digits = s;
    int isfloat = 0;
    int base = 10;
    if (s[0] == '0' && s + 1 < P->end && (s[1] == 'x' || s[1] == 'X')) {
      digits = s + 2;
      q = digits;
      base = 16;
      if (q >= P->end || !isxdigit((unsigned char)*q))
        lex_error(P, "malformed hexadecimal constant");
      while (q < P->end && isxdigit((unsigned char)*q))
        q++;
    }
    else if (s[0] == '0' && s + 1 < P->end &&
             (s[1] == 'b' || s[1] == 'B')) {
      digits = s + 2;
      q = digits;
      base = 2;
      if (q >= P->end || (*q != '0' && *q != '1'))
        lex_error(P, "malformed binary constant");
      while (q < P->end && (*q == '0' || *q == '1'))
        q++;
    }
    else {
      if (s[0] == '0' && s + 1 < P->end && isdigit((unsigned char)s[1]))
        base = 8;               /* C: a leading zero introduces octal */
      while (q < P->end && isdigit((unsigned char)*q)) {
        if (base == 8 && (*q == '8' || *q == '9'))
          lex_error(P, "malformed octal constant");
        q++;
      }
      if (q < P->end && *q == '.') {
        isfloat = 1;
        q++;
        while (q < P->end && isdigit((unsigned char)*q))
          q++;
      }
      if (q < P->end && (*q == 'e' || *q == 'E')) {
        const char *e = q + 1;
        if (e < P->end && (*e == '+' || *e == '-'))
          e++;
        if (e < P->end && isdigit((unsigned char)*e)) {
          isfloat = 1;
          q = e;
          while (q < P->end && isdigit((unsigned char)*q))
            q++;
        }
      }
    }
    if (!isfloat) {
      while (q < P->end && (*q == 'u' || *q == 'U' ||
                            *q == 'l' || *q == 'L'))
        q++;
      if (q < P->end && (isalnum((unsigned char)*q) || *q == '_'))
        lex_error(P, "malformed integer constant");
    }
    P->tok.kind = isfloat ? TK_FLOAT : TK_INT;
    P->tok.start = s;
    P->tok.len = (size_t)(q - s);
    if (isfloat) {
      P->tok.dval = strtod(s, NULL);
    }
    else {
      uint64_t v = 0;
      const char *d = digits;
      while (d < q && (isdigit((unsigned char)*d) ||
                       isxdigit((unsigned char)*d))) {
        int dv;
        if (*d >= '0' && *d <= '9') dv = *d - '0';
        else if (*d >= 'a' && *d <= 'f') dv = *d - 'a' + 10;
        else if (*d >= 'A' && *d <= 'F') dv = *d - 'A' + 10;
        else break;
        if (dv >= base) break;
        v = v * (uint64_t)base + (uint64_t)dv;
        d++;
      }
      P->tok.ival = (int64_t)v;
    }
    P->p = q;
    return;
  }
  P->tok.start = s;
  if (s + 2 < P->end && s[0] == '.' && s[1] == '.' && s[2] == '.') {
    P->tok.kind = TK_PUNCT;
    P->tok.punct = PT_ELLIPSIS;
    P->tok.len = 3;
    P->p = s + 3;
    return;
  }
  if (s + 1 < P->end) {
    int p2 = -1;
    if (s[0] == '<' && s[1] == '<') p2 = PT_SHL;
    else if (s[0] == '>' && s[1] == '>') p2 = PT_SHR;
    else if (s[0] == '<' && s[1] == '=') p2 = PT_LE;
    else if (s[0] == '>' && s[1] == '=') p2 = PT_GE;
    else if (s[0] == '=' && s[1] == '=') p2 = PT_EQ;
    else if (s[0] == '!' && s[1] == '=') p2 = PT_NE;
    else if (s[0] == '&' && s[1] == '&') p2 = PT_ANDAND;
    else if (s[0] == '|' && s[1] == '|') p2 = PT_OROR;
    else if (s[0] == '-' && s[1] == '>') p2 = PT_ARROW;
    if (p2 >= 0) {
      P->tok.kind = TK_PUNCT;
      P->tok.punct = p2;
      P->tok.len = 2;
      P->p = s + 2;
      return;
    }
  }
  if (strchr("*()[]{};,:=+-/%&|^~!<>.?", *s) != NULL) {
    P->tok.kind = TK_PUNCT;
    P->tok.punct = (unsigned char)*s;
    P->tok.len = 1;
    P->p = s + 1;
    return;
  }
  {
    char msg[64];
    snprintf(msg, sizeof(msg), "unexpected character '%c'", *s);
    if ((unsigned char)*s < 32)
      snprintf(msg, sizeof(msg), "unexpected control character (0x%02x)",
               (unsigned char)*s);
    lex_error(P, msg);
  }
}


static int tok_is_ident (Parser *P, const char *s) {
  size_t n = strlen(s);
  return P->tok.kind == TK_IDENT && P->tok.len == n &&
         memcmp(P->tok.start, s, n) == 0;
}


static int tok_is_punct (Parser *P, int p) {
  return P->tok.kind == TK_PUNCT && P->tok.punct == p;
}


static int accept_punct (Parser *P, int p) {
  if (tok_is_punct(P, p)) {
    lex_next(P);
    return 1;
  }
  return 0;
}


static void expect_punct (Parser *P, int p, const char *what) {
  if (!accept_punct(P, p))
    parser_error(P, "expected '%s'", what);
}


static int tok_to_buf (Parser *P, char *buf, size_t n) {
  if (P->tok.kind != TK_IDENT || P->tok.len >= n)
    return 0;
  memcpy(buf, P->tok.start, P->tok.len);
  buf[P->tok.len] = '\0';
  return 1;
}


static char *token_dup (Parser *P) {
  char *s;
  if (P->tok.kind != TK_IDENT)
    return NULL;
  s = (char *)ffi_alloc(P->S, P->tok.len + 1);
  memcpy(s, P->tok.start, P->tok.len);
  s[P->tok.len] = '\0';
  return s;
}


/* ------------------------------------------------------------------ */
/* Name lookup helpers                                                */
/* ------------------------------------------------------------------ */

static CType *lookup_typedef (Parser *P) {
  char buf[256];
  if (!tok_to_buf(P, buf, sizeof(buf)))
    return NULL;
  return ffi_table_get_type(P->L, P->S, FFI_UV_TYPEDEFS, buf);
}


static CType *lookup_tag (Parser *P, const char *kind, const char *tag) {
  char buf[320];
  if (tag[0] == '\0')
    return NULL;
  snprintf(buf, sizeof(buf), "%s %s", kind, tag);
  return ffi_table_get_type(P->L, P->S, FFI_UV_TAGS, buf);
}


static void register_tag (Parser *P, const char *kind, const char *tag,
                          CType *ct) {
  char buf[320];
  if (tag[0] == '\0')
    return;
  snprintf(buf, sizeof(buf), "%s %s", kind, tag);
  ffi_table_set_type(P->L, P->S, FFI_UV_TAGS, buf, ct);
}


static int is_qualifier (Parser *P) {
  return tok_is_ident(P, "const") || tok_is_ident(P, "volatile") ||
         tok_is_ident(P, "restrict") || tok_is_ident(P, "__const") ||
         tok_is_ident(P, "__volatile__") || tok_is_ident(P, "__restrict") ||
         tok_is_ident(P, "__restrict__") || tok_is_ident(P, "__extension__");
}


static int is_storage (Parser *P) {
  return tok_is_ident(P, "typedef") || tok_is_ident(P, "extern") ||
         tok_is_ident(P, "static") || tok_is_ident(P, "register") ||
         tok_is_ident(P, "auto") || tok_is_ident(P, "inline") ||
         tok_is_ident(P, "_Thread_local") || tok_is_ident(P, "__thread");
}


static int is_unsupported_keyword (Parser *P) {
  return tok_is_ident(P, "__attribute__") || tok_is_ident(P, "__attribute") ||
         tok_is_ident(P, "__declspec") || tok_is_ident(P, "_Atomic") ||
         tok_is_ident(P, "_Complex") || tok_is_ident(P, "__complex__") ||
         tok_is_ident(P, "__int128") || tok_is_ident(P, "__asm__") ||
         tok_is_ident(P, "asm") || tok_is_ident(P, "_Static_assert");
}


static void reject_unsupported (Parser *P) {
  if (is_unsupported_keyword(P)) {
    char buf[128];
    if (tok_to_buf(P, buf, sizeof(buf)))
      parser_error(P, "unsupported C construct '%s'", buf);
    parser_error(P, "unsupported C construct");
  }
}


static int is_type_word (Parser *P) {
  return tok_is_ident(P, "void") || tok_is_ident(P, "_Bool") ||
         tok_is_ident(P, "bool") || tok_is_ident(P, "char") ||
         tok_is_ident(P, "short") || tok_is_ident(P, "int") ||
         tok_is_ident(P, "long") || tok_is_ident(P, "float") ||
         tok_is_ident(P, "double") || tok_is_ident(P, "signed") ||
         tok_is_ident(P, "unsigned") || tok_is_ident(P, "struct") ||
         tok_is_ident(P, "union") || tok_is_ident(P, "enum") ||
         is_qualifier(P) || is_storage(P);
}


static int is_type_start (Parser *P) {
  if (P->tok.kind != TK_IDENT)
    return 0;
  if (is_type_word(P))
    return 1;
  return lookup_typedef(P) != NULL;
}


/* ------------------------------------------------------------------ */
/* Declarator AST                                                     */
/* ------------------------------------------------------------------ */

typedef enum { DN_IDENT, DN_PAREN } DNodeKind;
typedef enum { SF_ARRAY, SF_FUNC } SuffixKind;

typedef struct Suffix {
  SuffixKind kind;
  int vla;
  int64_t alen;
  int vararg;
  int nargs;
  CType **args;
  struct Suffix *next;
} Suffix;

typedef struct DNode {
  DNodeKind kind;
  char *name;			/* NULL for an abstract declarator */
  int nptr;
  struct DNode *inner;		/* for DN_PAREN */
  Suffix *suffixes;
  Suffix *suffix_tail;
} DNode;


static DNode *node_new (Parser *P) {
  DNode *n = (DNode *)ffi_alloc(P->S, sizeof(DNode));
  n->kind = DN_IDENT;
  return n;
}


static Suffix *suffix_new (Parser *P) {
  return (Suffix *)ffi_alloc(P->S, sizeof(Suffix));
}


static void node_add_suffix (DNode *n, Suffix *s) {
  if (n->suffix_tail != NULL)
    n->suffix_tail->next = s;
  else
    n->suffixes = s;
  n->suffix_tail = s;
}


static void arglist_add (Parser *P, CType ***args, int *n, int *cap,
                         CType *t) {
  if (*n == *cap) {
    int ncap = (*cap == 0) ? 4 : *cap * 2;
    CType **na = (CType **)ffi_alloc(P->S, sizeof(CType *) * (size_t)ncap);
    if (*n > 0)
      memcpy(na, *args, sizeof(CType *) * (size_t)*n);
    *args = na;
    *cap = ncap;
  }
  (*args)[(*n)++] = t;
}


/* Does a parenthesized chunk at the current token introduce a nested
** declarator rather than a function parameter list? */
static int nested_ahead (Parser *P) {
  Parser save = *P;
  int nested = 0;
  lex_next(P);
  if (tok_is_punct(P, '*') || tok_is_punct(P, '('))
    nested = 1;
  else if (P->mode == 0 && P->tok.kind == TK_IDENT &&
           !is_type_start(P))
    nested = 1;
  *P = save;
  return nested;
}


typedef struct DeclSpec {
  CType *base;
  int is_typedef;
  int is_extern;
  int is_static;
  int is_tls;
} DeclSpec;

static DeclSpec parse_declspec (Parser *P);
static DNode *parse_declarator (Parser *P);
static CType *apply_node (Parser *P, DNode *node, CType *base, char **name);
static CType *apply_suffix (Parser *P, Suffix *s, CType *base);
static CType *adjust_param (Parser *P, CType *ct);
static struct CType *parse_struct_or_union (Parser *P, int is_union);
static struct CType *parse_enum (Parser *P);
static int64_t parse_const_expr (Parser *P);


static Suffix *parse_array_suffix (Parser *P) {
  Suffix *s = suffix_new(P);
  s->kind = SF_ARRAY;
  expect_punct(P, '[', "[");
  if (accept_punct(P, ']')) {
    s->vla = 1;
    return s;
  }
  if (accept_punct(P, '?')) {
    expect_punct(P, ']', "]");
    s->vla = 1;
    return s;
  }
  if (tok_is_ident(P, "static")) {
    parser_error(P, "'static' array parameters are not supported");
  }
  {
    int64_t v = parse_const_expr(P);
    if (v <= 0)
      parser_error(P, "array size must be positive");
    s->alen = v;
  }
  expect_punct(P, ']', "]");
  return s;
}


static Suffix *parse_function_suffix (Parser *P) {
  Suffix *s = suffix_new(P);
  CType **args = NULL;
  int nargs = 0, cap = 0, vararg = 0;
  s->kind = SF_FUNC;
  expect_punct(P, '(', "(");
  if (accept_punct(P, ')')) {
    s->nargs = 0;
    return s;
  }
  /* Special case: (void) means no parameters. */
  if (tok_is_ident(P, "void")) {
    Parser save = *P;
    lex_next(P);
    if (tok_is_punct(P, ')')) {
      lex_next(P);
      s->nargs = 0;
      return s;
    }
    *P = save;
  }
  for (;;) {
    if (tok_is_punct(P, PT_ELLIPSIS)) {
      vararg = 1;
      lex_next(P);
      expect_punct(P, ')', ")");
      break;
    }
    {
      DeclSpec ds = parse_declspec(P);
      DNode *node = parse_declarator(P);
      char *pname = NULL;
      CType *pt = apply_node(P, node, ds.base, &pname);
      if (pt->kind == CT_VOID)
        parser_error(P, "parameter has void type");
      pt = adjust_param(P, pt);
      arglist_add(P, &args, &nargs, &cap, pt);
    }
    if (accept_punct(P, ',')) {
      if (tok_is_punct(P, ')'))
        parser_error(P, "expected a parameter after ','");
      continue;
    }
    expect_punct(P, ')', ")");
    break;
  }
  s->nargs = nargs;
  s->args = args;
  s->vararg = vararg;
  return s;
}


static DNode *parse_declarator (Parser *P) {
  DNode *n;
  if (++P->depth > 200)
    parser_error(P, "declarator nesting too deep");
  n = node_new(P);
  reject_unsupported(P);
  while (accept_punct(P, '*')) {
    n->nptr++;
    while (is_qualifier(P))
      lex_next(P);
  }
  if (tok_is_punct(P, '(') && nested_ahead(P)) {
    n->kind = DN_PAREN;
    lex_next(P);
    n->inner = parse_declarator(P);
    expect_punct(P, ')', ")");
  }
  else {
    n->kind = DN_IDENT;
    if (P->tok.kind == TK_IDENT) {
      n->name = token_dup(P);
      lex_next(P);
    }
  }
  for (;;) {
    if (tok_is_punct(P, '['))
      node_add_suffix(n, parse_array_suffix(P));
    else if (tok_is_punct(P, '('))
      node_add_suffix(n, parse_function_suffix(P));
    else
      break;
  }
  P->depth--;
  return n;
}


/* ------------------------------------------------------------------ */
/* Constant expressions (array sizes, enum values, initializers)      */
/* ------------------------------------------------------------------ */

static int64_t parse_primary (Parser *P) {
  if (tok_is_ident(P, "sizeof") || tok_is_ident(P, "_Alignof") ||
      tok_is_ident(P, "__alignof__") || tok_is_ident(P, "alignof")) {
    int is_align = !tok_is_ident(P, "sizeof");
    CType *ct;
    DNode *node;
    DeclSpec ds;
    char *name = NULL;
    size_t sz;
    lex_next(P);
    expect_punct(P, '(', "(");
    ds = parse_declspec(P);
    node = parse_declarator(P);
    ct = apply_node(P, node, ds.base, &name);
    expect_punct(P, ')', ")");
    (void)name;
    if (is_align)
      return (int64_t)ffi_type_align(ct);
    sz = ffi_type_size(ct);
    if (sz == FFI_SIZE_UNKNOWN || ct->incomplete)
      parser_error(P, "sizeof of an incomplete C type");
    return (int64_t)sz;
  }
  if (tok_is_ident(P, "offsetof") || tok_is_ident(P, "__builtin_offsetof")) {
    CType *ct;
    DNode *node;
    DeclSpec ds;
    char *name = NULL;
    char fname[256];
    size_t off = 0;
    CField *f;
    lex_next(P);
    expect_punct(P, '(', "(");
    ds = parse_declspec(P);
    node = parse_declarator(P);
    ct = apply_node(P, node, ds.base, &name);
    expect_punct(P, ',', ",");
    if (P->tok.kind != TK_IDENT)
      parser_error(P, "expected a field name in offsetof");
    if (!tok_to_buf(P, fname, sizeof(fname)))
      parser_error(P, "field name too long in offsetof");
    lex_next(P);
    expect_punct(P, ')', ")");
    f = ffi_type_find_field(ct, fname, &off, NULL);
    if (f == NULL)
      parser_error(P, "offsetof: no field named '%s'", fname);
    if (f->bitwidth != 0)
      parser_error(P, "offsetof of a bitfield is not supported");
    return (int64_t)off;
  }
  if (P->tok.kind == TK_INT || P->tok.kind == TK_CHAR) {
    int64_t v = P->tok.ival;
    lex_next(P);
    return v;
  }
  if (P->tok.kind == TK_IDENT) {
    char buf[256];
    int64_t v;
    if (!tok_to_buf(P, buf, sizeof(buf)))
      parser_error(P, "identifier too long in constant expression");
    if (!ffi_table_get_int(P->L, P->S, FFI_UV_CONSTS, buf, &v))
      parser_error(P, "unknown identifier '%s' in constant expression", buf);
    lex_next(P);
    return v;
  }
  if (accept_punct(P, '(')) {
    int64_t v = parse_const_expr(P);
    expect_punct(P, ')', ")");
    return v;
  }
  parser_error(P, "expected an integer constant expression");
  return 0;
}


static int64_t parse_unary (Parser *P) {
  int64_t v;
  if (++P->depth > 200)
    parser_error(P, "constant expression nested too deeply");
  if (accept_punct(P, '+'))
    v = parse_unary(P);
  else if (accept_punct(P, '-')) {
    v = parse_unary(P);
    v = (int64_t)(0 - (uint64_t)v);   /* wrap, including INT64_MIN */
  }
  else if (accept_punct(P, '~'))
    v = ~parse_unary(P);
  else if (accept_punct(P, '!'))
    v = !parse_unary(P);
  else
    v = parse_primary(P);
  P->depth--;
  return v;
}


static int const_prec (Parser *P) {
  if (P->tok.kind != TK_PUNCT)
    return 0;
  switch (P->tok.punct) {
    case '|': return 1;
    case '^': return 2;
    case '&': return 3;
    case PT_SHL: case PT_SHR: return 4;
    case '+': case '-': return 5;
    case '*': case '/': case '%': return 6;
    default: return 0;
  }
}


static int64_t parse_binary (Parser *P, int minprec) {
  int64_t lhs = parse_unary(P);
  for (;;) {
    int prec = const_prec(P);
    int op;
    int64_t rhs;
    if (prec == 0 || prec < minprec)
      break;
    op = P->tok.punct;
    lex_next(P);
    rhs = parse_binary(P, prec + 1);
    switch (op) {
      case '|': lhs = lhs | rhs; break;
      case '^': lhs = lhs ^ rhs; break;
      case '&': lhs = lhs & rhs; break;
      case PT_SHL:
        if (rhs < 0 || rhs >= 64)
          parser_error(P, "shift count out of range");
        lhs = (int64_t)((uint64_t)lhs << (unsigned)rhs);
        break;
      case PT_SHR:
        if (rhs < 0 || rhs >= 64)
          parser_error(P, "shift count out of range");
        lhs = lhs >> (unsigned)rhs;
        break;
      case '+': lhs = (int64_t)((uint64_t)lhs + (uint64_t)rhs); break;
      case '-': lhs = (int64_t)((uint64_t)lhs - (uint64_t)rhs); break;
      case '*': lhs = (int64_t)((uint64_t)lhs * (uint64_t)rhs); break;
      case '/':
        if (rhs == 0)
          parser_error(P, "division by zero in constant expression");
        lhs = (lhs == INT64_MIN && rhs == -1) ? INT64_MIN : lhs / rhs;
        break;
      case '%':
        if (rhs == 0)
          parser_error(P, "division by zero in constant expression");
        lhs = (lhs == INT64_MIN && rhs == -1) ? 0 : lhs % rhs;
        break;
      default: break;
    }
  }
  return lhs;
}


static int64_t parse_const_expr (Parser *P) {
  return parse_binary(P, 1);
}


/* ------------------------------------------------------------------ */
/* Declarator application                                            */
/* ------------------------------------------------------------------ */

static CType *adjust_param (Parser *P, CType *ct) {
  if (ct->kind == CT_ARRAY)
    return ffi_type_ptr(P->L, P->S, ct->child);
  if (ct->kind == CT_FUNC)
    return ffi_type_ptr(P->L, P->S, ct);
  return ct;
}


static CType *apply_suffix (Parser *P, Suffix *s, CType *base) {
  if (s->kind == SF_ARRAY) {
    if (base->kind == CT_FUNC)
      parser_error(P, "array of functions is not a valid C type");
    return ffi_type_arr(P->L, P->S, base, (size_t)s->alen,
                          s->vla != 0);
  }
  else {
    if (base->kind == CT_FUNC)
      parser_error(P, "function returning a function is not valid C");
    if (base->kind == CT_ARRAY)
      parser_error(P, "function returning an array is not valid C");
    return ffi_type_func(P->L, P->S, base, s->args, s->nargs,
                             s->vararg != 0);
  }
}


static CType *apply_suffixes_rev (Parser *P, Suffix *s, CType *base) {
  if (s == NULL)
    return base;
  base = apply_suffixes_rev(P, s->next, base);
  return apply_suffix(P, s, base);
}


static CType *apply_node (Parser *P, DNode *node, CType *base,
                          char **name) {
  int i;
  for (i = 0; i < node->nptr; i++)
    base = ffi_type_ptr(P->L, P->S, base);
  base = apply_suffixes_rev(P, node->suffixes, base);
  if (node->kind == DN_IDENT) {
    if (name != NULL)
      *name = node->name;
    return base;
  }
  return apply_node(P, node->inner, base, name);
}


/* ------------------------------------------------------------------ */
/* Declaration specifiers                                            */
/* ------------------------------------------------------------------ */

static DeclSpec parse_declspec (Parser *P) {
  DeclSpec ds;
  int seen = 0;
  int nvoid = 0, nbool = 0, nchar = 0, nshort = 0, nint = 0;
  int nlong = 0, nfloat = 0, ndouble = 0;
  int nsigned = 0, nunsigned = 0;
  CType *base = NULL;

  memset(&ds, 0, sizeof(ds));
  ds.base = NULL;

  for (;;) {
    reject_unsupported(P);
    if (P->tok.kind != TK_IDENT)
      break;
    if (is_qualifier(P)) {
      lex_next(P);
      continue;
    }
    if (is_storage(P)) {
      if (tok_is_ident(P, "typedef"))
        ds.is_typedef = 1;
      else if (tok_is_ident(P, "extern"))
        ds.is_extern = 1;
      else if (tok_is_ident(P, "static"))
        ds.is_static = 1;
      else if (tok_is_ident(P, "_Thread_local") ||
               tok_is_ident(P, "__thread"))
        ds.is_tls = 1;
      lex_next(P);
      continue;
    }
    if (tok_is_ident(P, "struct") || tok_is_ident(P, "union")) {
      int is_union = tok_is_ident(P, "union");
      if (seen)
        parser_error(P, "duplicate type specifier");
      base = parse_struct_or_union(P, is_union);
      seen = 1;
      continue;
    }
    if (tok_is_ident(P, "enum")) {
      if (seen)
        parser_error(P, "duplicate type specifier");
      base = parse_enum(P);
      seen = 1;
      continue;
    }
    if (tok_is_ident(P, "void")) {
      nvoid++; seen = 1; lex_next(P); continue;
    }
    if (tok_is_ident(P, "_Bool") || tok_is_ident(P, "bool")) {
      nbool++; seen = 1; lex_next(P); continue;
    }
    if (tok_is_ident(P, "char")) {
      nchar++; seen = 1; lex_next(P); continue;
    }
    if (tok_is_ident(P, "short")) {
      nshort++; seen = 1; lex_next(P); continue;
    }
    if (tok_is_ident(P, "int")) {
      nint++; seen = 1; lex_next(P); continue;
    }
    if (tok_is_ident(P, "long")) {
      nlong++; seen = 1; lex_next(P); continue;
    }
    if (tok_is_ident(P, "float")) {
      nfloat++; seen = 1; lex_next(P); continue;
    }
    if (tok_is_ident(P, "double")) {
      ndouble++; seen = 1; lex_next(P); continue;
    }
    if (tok_is_ident(P, "signed")) {
      nsigned++; seen = 1; lex_next(P); continue;
    }
    if (tok_is_ident(P, "unsigned")) {
      nunsigned++; seen = 1; lex_next(P); continue;
    }
    {
      CType *td = lookup_typedef(P);
      if (td != NULL) {
        if (seen)
          parser_error(P, "duplicate type specifier");
        base = td;
        seen = 1;
        lex_next(P);
        continue;
      }
    }
    break;
  }

  if (!seen)
    parser_error(P, "expected a C type");
  if (nvoid > 1 || nbool > 1 || nchar > 1 || nshort > 1 || nint > 1 ||
      nlong > 2 || nfloat > 1 || ndouble > 1 || nsigned > 1 ||
      nunsigned > 1)
    parser_error(P, "invalid type specifier sequence");
  if (nsigned && nunsigned)
    parser_error(P, "both 'signed' and 'unsigned' in type specifier");

  if (base != NULL) {
    if (nvoid || nbool || nchar || nshort || nint || nlong ||
        nfloat || ndouble || nsigned || nunsigned)
      parser_error(P, "invalid combination of type specifiers");
    ds.base = base;
    return ds;
  }

  if (nvoid) {
    if (nbool || nchar || nshort || nint || nlong || nfloat || ndouble ||
        nsigned || nunsigned)
      parser_error(P, "invalid type specifier for 'void'");
    ds.base = P->S->t_void;
  }
  else if (nbool) {
    if (nchar || nshort || nint || nlong || nfloat || ndouble ||
        nsigned || nunsigned)
      parser_error(P, "invalid type specifier for 'bool'");
    ds.base = P->S->t_bool;
  }
  else if (nfloat) {
    if (nchar || nshort || nint || nlong || ndouble || nsigned || nunsigned)
      parser_error(P, "invalid type specifier for 'float'");
    ds.base = P->S->t_float;
  }
  else if (ndouble) {
    if (nlong)
      parser_error(P, "'long double' is not supported");
    if (nchar || nshort || nint || nsigned || nunsigned || nfloat)
      parser_error(P, "invalid type specifier for 'double'");
    ds.base = P->S->t_double;
  }
  else if (nchar) {
    if (nshort || nint || nlong || nfloat || ndouble)
      parser_error(P, "invalid type specifier for 'char'");
    ds.base = nsigned ? P->S->t_schar :
              nunsigned ? P->S->t_uchar : P->S->t_char;
  }
  else if (nshort) {
    if (nlong || nfloat || ndouble || nchar)
      parser_error(P, "invalid type specifier for 'short'");
    ds.base = nunsigned ? P->S->t_ushort : P->S->t_short;
  }
  else if (nlong) {
    if (nshort || nchar || nfloat || ndouble)
      parser_error(P, "invalid type specifier for 'long'");
    if (nlong == 2)
      ds.base = nunsigned ? P->S->t_ullong : P->S->t_llong;
    else
      ds.base = nunsigned ? P->S->t_ulong : P->S->t_long;
  }
  else if (nint) {
    if (nchar || nshort || nlong || nfloat || ndouble)
      parser_error(P, "invalid type specifier sequence");
    ds.base = nunsigned ? P->S->t_uint : P->S->t_int;
  }
  else {
    /* signed / unsigned alone mean int */
    ds.base = nunsigned ? P->S->t_uint : P->S->t_int;
  }
  return ds;
}


/* ------------------------------------------------------------------ */
/* struct / union / enum definitions                                 */
/* ------------------------------------------------------------------ */

static void parse_struct_attrs (Parser *P, CType *ct) {
  while (tok_is_ident(P, "__attribute__")) {
    lex_next(P);
    expect_punct(P, '(', "(");
    expect_punct(P, '(', "(");
    for (;;) {
      if (tok_is_ident(P, "packed")) {
        lex_next(P);
        ct->pack = 1;
      }
      else if (tok_is_ident(P, "aligned")) {
        int64_t v;
        lex_next(P);
        expect_punct(P, '(', "(");
        v = parse_const_expr(P);
        expect_punct(P, ')', ")");
        if (v < 1 || v > 32 || (v & (v - 1)) != 0)
          parser_error(P, "bad aligned() value");
        ct->align_override = (uint8_t)v;
      }
      else
        parser_error(P, "only packed/aligned attributes are supported");
      if (accept_punct(P, ','))
        continue;
      break;
    }
    expect_punct(P, ')', ")");
    expect_punct(P, ')', ")");
  }
}


static CType *parse_struct_or_union (Parser *P, int is_union) {
  const char *kind = is_union ? "union" : "struct";
  char tag[256];
  CType *ct = NULL;
  CType attrs;
  memset(&attrs, 0, sizeof(attrs));
  tag[0] = '\0';
  lex_next(P);			/* consume struct/union */
  if (tok_is_ident(P, "__attribute__"))
    parse_struct_attrs(P, &attrs);
  if (is_unsupported_keyword(P))
    reject_unsupported(P);
  if (P->tok.kind == TK_IDENT) {
    if (!tok_to_buf(P, tag, sizeof(tag)))
      parser_error(P, "struct/union tag too long");
    lex_next(P);
  }
  if (is_unsupported_keyword(P))
    reject_unsupported(P);
  if (tok_is_punct(P, '{')) {
    if (tag[0] != '\0') {
      ct = lookup_tag(P, kind, tag);
      if (ct != NULL && !ct->incomplete)
        parser_error(P, "redefinition of %s %s", kind, tag);
    }
    if (ct == NULL)
      ct = ffi_type_new_struct(P->L, P->S, is_union,
                               tag[0] != '\0' ? tag : NULL);
    if (attrs.pack != 0 && ct->pack == 0)
      ct->pack = attrs.pack;
    if (attrs.align_override > ct->align_override)
      ct->align_override = attrs.align_override;
    if (tag[0] != '\0')
      register_tag(P, kind, tag, ct);
    lex_next(P);		/* consume '{' */
    while (!accept_punct(P, '}')) {
      DeclSpec ds;
      if (P->tok.kind == TK_EOF)
        parser_error(P, "unterminated %s body", kind);
      ds = parse_declspec(P);
      if (accept_punct(P, ';')) {
        if (ds.base->kind == CT_STRUCT || ds.base->kind == CT_UNION) {
          if (ds.base->name == NULL)
            ffi_type_add_field(P->S, ct, NULL, ds.base);
          /* A named forward declaration/definition declares no member. */
        }
        else if (ds.base->kind == CT_ENUM) {
          /* A bare enum declaration declares no member. */
        }
        else {
          parser_error(P, "declaration does not declare a member");
        }
        continue;
      }
      for (;;) {
        DNode *node = parse_declarator(P);
        char *name = NULL;
        CType *ft = apply_node(P, node, ds.base, &name);
        if (name == NULL && !tok_is_punct(P, ':'))
          parser_error(P, "expected a field name");
        if (accept_punct(P, ':')) {
          int64_t width = parse_const_expr(P);
          size_t bits;
          if (ft->kind != CT_INT && ft->kind != CT_ENUM &&
              ft->kind != CT_BOOL)
            parser_error(P, "bitfield '%s' must have an integer type",
                         name != NULL ? name : "<anonymous>");
          bits = ffi_type_size(ft) * 8;
          if (width < 0 || (uint64_t)width > bits)
            parser_error(P, "bitfield '%s' width %d is out of range",
                         name != NULL ? name : "<anonymous>", (int)width);
          if (width == 0 && name != NULL)
            parser_error(P, "zero-width bitfield must be unnamed");
          ffi_type_add_field(P->S, ct, name, ft);
          if (width == 0)
            ct->fields[ct->nfields - 1].zero_width = 1;
          else
            ct->fields[ct->nfields - 1].bitwidth = (uint8_t)width;
        }
        else {
          reject_unsupported(P);
          ffi_type_add_field(P->S, ct, name, ft);
        }
        if (accept_punct(P, ','))
          continue;
        expect_punct(P, ';', ";");
        break;
      }
    }
    if (P->S->pack_align != 0 && ct->pack == 0)
      ct->pack = (uint8_t)P->S->pack_align;
    parse_struct_attrs(P, ct);
    ffi_type_finish_struct(P->S, ct);
    return ct;
  }
  if (tag[0] != '\0') {
    ct = lookup_tag(P, kind, tag);
    if (ct == NULL) {
      ct = ffi_type_new_struct(P->L, P->S, is_union, tag);
      register_tag(P, kind, tag, ct);
    }
    return ct;
  }
  parser_error(P, "expected '{' or a tag name after '%s'", kind);
  return NULL;
}


static CType *parse_enum (Parser *P) {
  char tag[256];
  CType *ct = NULL;
  int64_t value = 0;
  int first = 1;
  tag[0] = '\0';
  lex_next(P);			/* consume enum */
  if (is_unsupported_keyword(P))
    reject_unsupported(P);
  if (P->tok.kind == TK_IDENT) {
    if (!tok_to_buf(P, tag, sizeof(tag)))
      parser_error(P, "enum tag too long");
    lex_next(P);
  }
  if (is_unsupported_keyword(P))
    reject_unsupported(P);
  if (tok_is_punct(P, '{')) {
    if (tag[0] != '\0' && lookup_tag(P, "enum", tag) != NULL)
      parser_error(P, "redefinition of enum %s", tag);
    ct = ffi_type_new_enum(P->L, P->S, tag[0] != '\0' ? tag : NULL);
    if (tag[0] != '\0')
      register_tag(P, "enum", tag, ct);
    lex_next(P);		/* consume '{' */
    if (accept_punct(P, '}'))
      parser_error(P, "empty enum definition");
    for (;;) {
      char name[256];
      if (!tok_to_buf(P, name, sizeof(name)))
        parser_error(P, "expected an enumerator name");
      lex_next(P);
      if (accept_punct(P, '='))
        value = parse_const_expr(P);
      else if (first)
        value = 0;
      else
        value = value + 1;
      ffi_table_set_int(P->L, P->S, FFI_UV_CONSTS, name, value);
      first = 0;
      if (accept_punct(P, ',')) {
        if (tok_is_punct(P, '}'))
          break;
        continue;
      }
      break;
    }
    expect_punct(P, '}', "}");
    return ct;
  }
  if (tag[0] != '\0') {
    ct = lookup_tag(P, "enum", tag);
    if (ct == NULL)
      parser_error(P, "unknown enum type '%s'", tag);
    return ct;
  }
  parser_error(P, "expected '{' or a tag name after 'enum'");
  return NULL;
}


/* ------------------------------------------------------------------ */
/* Top-level declarations                                             */
/* ------------------------------------------------------------------ */

static void parse_external (Parser *P) {
  DeclSpec ds;
  if (tok_is_ident(P, "__luar_pack")) {
    int64_t v;
    lex_next(P);
    expect_punct(P, '(', "(");
    v = parse_const_expr(P);
    expect_punct(P, ')', ")");
    expect_punct(P, ';', ";");
    if (v < 0 || v > 16 || (v != 0 && (v & (v - 1)) != 0))
      parser_error(P, "bad __luar_pack value");
    P->S->pack_align = (int)v;
    return;
  }
  ds = parse_declspec(P);
  if (accept_punct(P, ';'))
    return;
  for (;;) {
    DNode *node = parse_declarator(P);
    char *name = NULL;
    CType *type = apply_node(P, node, ds.base, &name);
    if (name == NULL)
      parser_error(P, "expected a declarator name");
    if (accept_punct(P, '=')) {
      int64_t v = parse_const_expr(P);
      if (ds.is_typedef)
        parser_error(P, "a typedef cannot have an initializer");
      ffi_table_set_int(P->L, P->S, FFI_UV_CONSTS, name, v);
    }
    reject_unsupported(P);
    if (ds.is_typedef)
      ffi_table_set_type(P->L, P->S, FFI_UV_TYPEDEFS, name, type);
    else {
      ffi_table_set_type(P->L, P->S, FFI_UV_SYMS, name, type);
      if (ds.is_tls)
        ffi_table_set_int(P->L, P->S, FFI_UV_TLS, name, 1);
    }
    if (accept_punct(P, ','))
      continue;
    expect_punct(P, ';', ";");
    break;
  }
}


void ffi_parse_cdef (lua_State *L, FFIState *S, const char *src, size_t len) {
  Parser P;
  size_t plen = 0;
  S->pack_align = 0;
  char *pre = ffi_preprocess(L, S, src, len, &plen);
  if (pre != NULL) {
    src = pre;
    len = plen;
  }
  memset(&P, 0, sizeof(P));
  P.L = L;
  P.S = S;
  P.src = src;
  P.p = src;
  P.end = src + len;
  P.fname = "ffi.cdef";
  P.line = 1;
  P.mode = 0;
  lex_next(&P);
  while (P.tok.kind != TK_EOF)
    parse_external(&P);
  free(pre);
}


CType *ffi_parse_type (lua_State *L, FFIState *S, const char *src,
                       size_t len) {
  Parser P;
  DeclSpec ds;
  DNode *node;
  char *name = NULL;
  CType *ct;
  memset(&P, 0, sizeof(P));
  P.L = L;
  P.S = S;
  P.src = src;
  P.p = src;
  P.end = src + len;
  P.fname = "ffi.typeof";
  P.line = 1;
  P.mode = 1;
  lex_next(&P);
  ds = parse_declspec(&P);
  node = parse_declarator(&P);
  ct = apply_node(&P, node, ds.base, &name);
  (void)name;
  if (accept_punct(&P, ';')) {
    /* tolerate a trailing semicolon */
  }
  if (P.tok.kind != TK_EOF)
    parser_error(&P, "unexpected token after C type");
  return ct;
}

