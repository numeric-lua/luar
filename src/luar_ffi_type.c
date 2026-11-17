/*
** luar_ffi_type.c -- C type objects, layout and representation.
**
** Every CType is interned by a canonical id string.  Derived types
** (pointers, arrays, functions) are shared; struct/union/enum types get
** a unique id (the tag, or an anonymous counter).
**
** See Copyright Notice in lua.h and COPYRIGHT.
*/

#define LUA_LIB
#define LUAR_LIB

#include "lprefix.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "luar_ffi.h"
#include "luar_ffi_internal.h"


/* ------------------------------------------------------------------ */
/* Tracked arena                                                      */
/* ------------------------------------------------------------------ */

void *ffi_alloc (FFIState *S, size_t sz) {
  FAlloc *a;
  void *p;
  if (sz > (size_t)-1 - FFI_FALLOC_HDR)
    luaL_error(S->L, "ffi: allocation size overflow");
  a = (FAlloc *)calloc(1, FFI_FALLOC_HDR + sz);
  if (a == NULL)
    luaL_error(S->L, "ffi: out of memory");
  a->next = S->allocs;
  S->allocs = a;
  p = (char *)a + FFI_FALLOC_HDR;
  return p;
}


char *ffi_strdup (FFIState *S, const char *s) {
  size_t n = strlen(s) + 1;
  char *p = (char *)ffi_alloc(S, n);
  memcpy(p, s, n);
  return p;
}


char *ffi_asprintf (FFIState *S, const char *fmt, ...) {
  va_list ap;
  char *buf;
  int n;
  va_start(ap, fmt);
  n = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  if (n < 0)
    luaL_error(S->L, "ffi: internal formatting error");
  buf = (char *)ffi_alloc(S, (size_t)n + 1);
  va_start(ap, fmt);
  vsnprintf(buf, (size_t)n + 1, fmt, ap);
  va_end(ap);
  return buf;
}


void ffi_state_free (FFIState *S) {
  FAlloc *a = S->allocs;
  while (a != NULL) {
    FAlloc *next = a->next;
    free(a);
    a = next;
  }
  S->allocs = NULL;
}


/* ------------------------------------------------------------------ */
/* State and namespace tables                                        */
/* ------------------------------------------------------------------ */

FFIState *ffi_getstate (lua_State *L) {
  FFIState *S;
  lua_getfield(L, LUA_REGISTRYINDEX, LUAR_FFI_REGISTRY);
  S = (FFIState *)lua_touserdata(L, -1);
  lua_pop(L, 1);
  if (S == NULL)
    luaL_error(L, "ffi library is not initialized");
  return S;
}


void ffi_push_uv (lua_State *L, FFIState *S, int uv) {
  (void)S;
  lua_getfield(L, LUA_REGISTRYINDEX, LUAR_FFI_REGISTRY);
  if (lua_getiuservalue(L, -1, uv) == LUA_TNONE) {
    lua_pop(L, 1);
    lua_newtable(L);
  }
  lua_remove(L, -2);
}


static int is_ctype_object (lua_State *L, int idx) {
  CData *cd;
  if (lua_type(L, idx) != LUA_TUSERDATA)
    return 0;
  if ((size_t)lua_rawlen(L, idx) < sizeof(CData))
    return 0;
  cd = (CData *)lua_touserdata(L, idx);
  return cd != NULL && cd->magic == LUAR_FFI_MAGIC &&
         (cd->flags & CDATA_CTYPE) != 0;
}


CType *ffi_table_get_type (lua_State *L, FFIState *S, int uv,
                           const char *name) {
  CType *ct = NULL;
  ffi_push_uv(L, S, uv);
  lua_getfield(L, -1, name);
  if (is_ctype_object(L, -1))
    ct = ((CData *)lua_touserdata(L, -1))->ct;
  lua_pop(L, 2);
  return ct;
}


void ffi_table_set_type (lua_State *L, FFIState *S, int uv,
                         const char *name, CType *ct) {
  ffi_push_uv(L, S, uv);
  ffi_push_ctype(L, S, ct);
  lua_setfield(L, -2, name);
  lua_pop(L, 1);
}


int ffi_table_get_int (lua_State *L, FFIState *S, int uv,
                       const char *name, int64_t *out) {
  int found = 0;
  ffi_push_uv(L, S, uv);
  lua_getfield(L, -1, name);
  if (lua_isinteger(L, -1)) {
    *out = (int64_t)lua_tointeger(L, -1);
    found = 1;
  }
  lua_pop(L, 2);
  return found;
}


void ffi_table_set_int (lua_State *L, FFIState *S, int uv,
                        const char *name, int64_t v) {
  ffi_push_uv(L, S, uv);
  lua_pushinteger(L, (lua_Integer)v);
  lua_setfield(L, -2, name);
  lua_pop(L, 1);
}


/* ------------------------------------------------------------------ */
/* Ctype userdata objects                                            */
/* ------------------------------------------------------------------ */

static void push_ctype_object (lua_State *L, FFIState *S, CType *ct) {
  CData *cd = (CData *)lua_newuserdatauv(L, sizeof(CData), 2);
  memset(cd, 0, sizeof(CData));
  cd->ct = ct;
  cd->S = S;
  cd->flags = CDATA_CTYPE;
  cd->magic = LUAR_FFI_MAGIC;
  luaL_setmetatable(L, LUAR_FFI_CDATA_MT);
}


CType *ffi_type_intern (lua_State *L, FFIState *S, CType *ct) {
  CType *found = NULL;
  if (ct->id == NULL)
    luaL_error(L, "ffi: internal error: type without identity");
  ffi_push_uv(L, S, FFI_UV_INTERN);
  lua_getfield(L, -1, ct->id);
  if (is_ctype_object(L, -1)) {
    found = ((CData *)lua_touserdata(L, -1))->ct;
    lua_pop(L, 2);
    return found;
  }
  lua_pop(L, 1);		/* drop nil, keep intern table */
  push_ctype_object(L, S, ct);
  lua_pushvalue(L, -1);
  lua_setfield(L, -3, ct->id);	/* table[ct->id] = object */
  lua_remove(L, -2);		/* drop intern table */
  lua_pop(L, 1);		/* drop the temporary ctype object */
  return ct;
}


void ffi_push_ctype (lua_State *L, FFIState *S, CType *ct) {
  ffi_push_uv(L, S, FFI_UV_INTERN);
  lua_getfield(L, -1, ct->id);
  if (!is_ctype_object(L, -1)) {
    lua_pop(L, 1);		/* drop nil, keep table */
    push_ctype_object(L, S, ct);
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, ct->id);	/* table[ct->id] = object */
    lua_remove(L, -2);		/* drop table, keep object */
  }
  else {
    lua_remove(L, -2);		/* drop table, keep object */
  }
}


/* ------------------------------------------------------------------ */
/* Type construction                                                 */
/* ------------------------------------------------------------------ */

CType *ffi_type_new (FFIState *S, CTypeKind kind) {
  CType *ct = (CType *)ffi_alloc(S, sizeof(CType));
  ct->kind = kind;
  ct->size = FFI_SIZE_UNKNOWN;
  ct->align = 1;
  ct->metatype_ref = LUA_NOREF;
  return ct;
}


static CType *new_primitive (lua_State *L, FFIState *S, CTypeKind kind,
                             const char *name, size_t size, size_t align,
                             int isunsigned) {
  CType *ct = ffi_type_new(S, kind);
  ct->name = ffi_strdup(S, name);
  ct->id = ct->name;
  ct->size = size;
  ct->align = align;
  ct->is_unsigned = (uint8_t)(isunsigned != 0);
  return ffi_type_intern(L, S, ct);
}


CType *ffi_type_ptr (lua_State *L, FFIState *S, CType *child) {
  CType *ct = ffi_type_new(S, CT_POINTER);
  ct->child = child;
  ct->size = sizeof(void *);
  ct->align = offsetof(struct { char c; void *p; }, p);
  ct->id = ffi_asprintf(S, "p<%s>", child->id);
  return ffi_type_intern(L, S, ct);
}


CType *ffi_type_arr (lua_State *L, FFIState *S, CType *child,
                       size_t len, int vla) {
  CType *ct;
  size_t esz;
  if (child->kind == CT_VOID)
    luaL_error(L, "ffi: array of void is not a valid C type");
  if (child->kind == CT_FUNC)
    luaL_error(L, "ffi: array of functions is not a valid C type");
  if (child->incomplete)
    luaL_error(L, "ffi: array of incomplete type is not a valid C type");
  esz = ffi_type_size(child);
  if (!vla && esz == FFI_SIZE_UNKNOWN)
    luaL_error(L, "ffi: array element has unknown size");
  ct = ffi_type_new(S, CT_ARRAY);
  ct->child = child;
  ct->align = child->align;
  ct->vla = (uint8_t)(vla != 0);
  ct->length = vla ? 0 : len;
  if (vla) {
    ct->size = FFI_SIZE_UNKNOWN;
    ct->id = ffi_asprintf(S, "a?<%s>", child->id);
  }
  else {
    if (len != 0 && esz > FFI_SIZE_UNKNOWN / len)
      luaL_error(L, "ffi: array size overflow");
    ct->size = esz * len;
    ct->id = ffi_asprintf(S, "a%lu<%s>", (unsigned long)len, child->id);
  }
  return ffi_type_intern(L, S, ct);
}


CType *ffi_type_func (lua_State *L, FFIState *S, CType *ret,
                          CType **args, int nargs, int vararg) {
  CType *ct = ffi_type_new(S, CT_FUNC);
  int i;
  size_t need = strlen(ret->id) + 8;   /* "f<" ret ">(" ... ")" NUL */
  size_t used = 0;
  char *id;
  ct->ret = ret;
  ct->vararg = (uint8_t)(vararg != 0);
  ct->nargs = (uint16_t)nargs;
  ct->argcap = (uint16_t)nargs;
  ct->align = 1;
  if (nargs > 0) {
    ct->args = (CType **)ffi_alloc(S, sizeof(CType *) * (size_t)nargs);
    for (i = 0; i < nargs; i++) {
      ct->args[i] = args[i];
      need += strlen(args[i]->id) + 1;
    }
  }
  if (vararg)
    need += 4;                         /* optional ',' + "..." */
  id = (char *)ffi_alloc(S, need);
  id[used++] = 'f';
  id[used++] = '<';
  memcpy(id + used, ret->id, strlen(ret->id));
  used += strlen(ret->id);
  id[used++] = '>';
  id[used++] = '(';
  for (i = 0; i < nargs; i++) {
    size_t n = strlen(args[i]->id);
    if (i > 0)
      id[used++] = ',';
    memcpy(id + used, args[i]->id, n);
    used += n;
  }
  if (vararg) {
    if (nargs > 0)
      id[used++] = ',';
    memcpy(id + used, "...", 3);
    used += 3;
  }
  id[used++] = ')';
  id[used] = '\0';
  ct->id = id;
  return ffi_type_intern(L, S, ct);
}


CType *ffi_type_new_struct (lua_State *L, FFIState *S, int is_union,
                            const char *tag) {
  CType *ct = ffi_type_new(S, is_union ? CT_UNION : CT_STRUCT);
  ct->incomplete = 1;
  ct->size = FFI_SIZE_UNKNOWN;
  ct->align = 1;
  if (tag != NULL && tag[0] != '\0') {
    ct->name = ffi_strdup(S, tag);
    ct->id = ffi_asprintf(S, "%s %s", is_union ? "union" : "struct", tag);
  }
  else {
    ct->id = ffi_asprintf(S, "%s:#%llu", is_union ? "union" : "struct",
                          S->anon++);
  }
  return ffi_type_intern(L, S, ct);
}


CType *ffi_type_new_enum (lua_State *L, FFIState *S, const char *tag) {
  CType *ct = ffi_type_new(S, CT_ENUM);
  ct->child = S->t_int;
  ct->size = S->t_int->size;
  ct->align = S->t_int->align;
  ct->is_unsigned = 0;
  if (tag != NULL && tag[0] != '\0') {
    ct->name = ffi_strdup(S, tag);
    ct->id = ffi_asprintf(S, "enum %s", tag);
  }
  else {
    ct->id = ffi_asprintf(S, "enum:#%llu", S->anon++);
  }
  return ffi_type_intern(L, S, ct);
}


void ffi_type_add_field (FFIState *S, CType *ct, const char *name,
                         CType *type) {
  if (ct->nfields == ct->fieldcap) {
    CField *nf;
    uint16_t ncap = (uint16_t)(ct->fieldcap == 0 ? 4 : ct->fieldcap * 2);
    nf = (CField *)ffi_alloc(S, sizeof(CField) * (size_t)ncap);
    if (ct->nfields > 0)
      memcpy(nf, ct->fields, sizeof(CField) * ct->nfields);
    ct->fields = nf;
    ct->fieldcap = ncap;
  }
  ct->fields[ct->nfields].name = (name != NULL) ? ffi_strdup(S, name) : NULL;
  ct->fields[ct->nfields].type = type;
  ct->fields[ct->nfields].offset = 0;
  ct->nfields++;
}


static size_t round_up (size_t n, size_t align) {
  size_t rem;
  if (align <= 1)
    return n;
  rem = n % align;
  return (rem == 0) ? n : n + (align - rem);
}


void ffi_type_finish_struct (FFIState *S, CType *ct) {
  size_t off = 0, maxsize = 0, align = 1;
  size_t bitpos = 0;
  uint16_t i;
  for (i = 0; i < ct->nfields; i++) {
    CField *f = &ct->fields[i];
    size_t fsz = ffi_type_size(f->type);
    f->packed = (ct->pack == 1);
    size_t fal = ffi_type_align(f->type);
    if (fsz == FFI_SIZE_UNKNOWN || f->type->incomplete ||
        f->type->kind == CT_VOID || f->type->kind == CT_FUNC)
      luaL_error(S->L, "ffi: field '%s' has incomplete or invalid type",
                 f->name != NULL ? f->name : "<anonymous>");
    if (ct->pack != 0 && fal > ct->pack)
      fal = ct->pack;
    if (!f->zero_width && fal > align)
      align = fal;
    if (ct->kind == CT_UNION) {
      f->offset = 0;
      f->bitoffset = 0;
      if (!f->zero_width && fsz > maxsize)
        maxsize = fsz;
      continue;
    }
    if (f->zero_width) {
      size_t nb = (bitpos + 7) / 8;
      nb = (nb + fal - 1) & ~(fal - 1);
      bitpos = nb * 8;
    }
    else if (f->bitwidth != 0 && ct->pack == 1) {
      f->offset = bitpos / 8;
      f->bitoffset = (uint8_t)(bitpos & 7);
      bitpos += f->bitwidth;
    }
    else if (f->bitwidth != 0) {
      size_t ustart = (bitpos / 8) & ~(fal - 1);
      if ((bitpos - ustart * 8) + f->bitwidth > fsz * 8) {
        size_t nb = (bitpos + 7) / 8;
        nb = (nb + fal - 1) & ~(fal - 1);
        bitpos = nb * 8;
      }
      f->offset = bitpos / 8;
      f->bitoffset = (uint8_t)(bitpos & 7);
      bitpos += f->bitwidth;
    }
    else {
      size_t nb = (bitpos + 7) / 8;
      off = (nb + fal - 1) & ~(fal - 1);
      f->offset = off;
      if (fsz > FFI_SIZE_UNKNOWN - off)
        luaL_error(S->L, "ffi: struct layout overflow");
      off += fsz;
      bitpos = off * 8;
    }
  }
  if (ct->align_override > align)
    align = ct->align_override;
  ct->align = align;
  if (ct->kind == CT_UNION)
    ct->size = round_up(maxsize, align);
  else
    ct->size = round_up((bitpos + 7) / 8, align);
  ct->incomplete = 0;
}


CField *ffi_type_find_field (CType *ct, const char *name,
                             size_t *offset, CType **fieldtype) {
  uint16_t i;
  if (ct->kind != CT_STRUCT && ct->kind != CT_UNION)
    return NULL;
  for (i = 0; i < ct->nfields; i++) {
    CField *f = &ct->fields[i];
    if (f->name != NULL) {
      if (strcmp(f->name, name) == 0) {
        if (fieldtype != NULL)
          *fieldtype = f->type;
        *offset += f->offset;
        return f;
      }
    }
    else if (f->type->kind == CT_STRUCT || f->type->kind == CT_UNION) {
      CField *sub;
      size_t suboff = 0;
      sub = ffi_type_find_field(f->type, name, &suboff, fieldtype);
      if (sub != NULL) {
        *offset += f->offset + suboff;
        return sub;
      }
    }
  }
  return NULL;
}


/* ------------------------------------------------------------------ */
/* Size, alignment and representation                                */
/* ------------------------------------------------------------------ */

size_t ffi_type_size (CType *ct) {
  if (ct == NULL)
    return FFI_SIZE_UNKNOWN;
  switch (ct->kind) {
    case CT_VOID:
    case CT_FUNC:
      return FFI_SIZE_UNKNOWN;
    case CT_ARRAY:
      if (ct->vla)
        return FFI_SIZE_UNKNOWN;
      return ct->size;
    case CT_STRUCT:
    case CT_UNION:
      if (ct->incomplete)
        return FFI_SIZE_UNKNOWN;
      return ct->size;
    default:
      return ct->size;
  }
}


size_t ffi_type_align (CType *ct) {
  if (ct == NULL)
    return 1;
  switch (ct->kind) {
    case CT_VOID:
    case CT_FUNC:
      return 1;
    case CT_ARRAY:
      return ffi_type_align(ct->child);
    case CT_STRUCT:
    case CT_UNION:
      return ct->incomplete ? 1 : ct->align;
    default:
      return ct->align;
  }
}


/* A small growable buffer used only while formatting type names. */
typedef struct ReprBuf {
  char *p;
  size_t len, cap;
} ReprBuf;


static void rb_reserve (ReprBuf *b, size_t extra) {
  if (b->len + extra + 1 > b->cap) {
    size_t ncap = (b->cap == 0) ? 128 : b->cap;
    char *np;
    while (b->len + extra + 1 > ncap)
      ncap *= 2;
    np = (char *)realloc(b->p, ncap);
    if (np == NULL)
      return;		/* keep old buffer; output is truncated */
    b->p = np;
    b->cap = ncap;
  }
}


static void rb_put (ReprBuf *b, const char *s) {
  size_t n = strlen(s);
  rb_reserve(b, n);
  if (b->p != NULL && b->len + n + 1 <= b->cap) {
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
  }
}


static void rb_putf (ReprBuf *b, const char *fmt, ...) {
  char tmp[64];
  va_list ap;
  int n;
  va_start(ap, fmt);
  n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  if (n > 0)
    rb_put(b, tmp);
}


static void repr_func_args (ReprBuf *b, CType *ct) {
  int i;
  if (ct->nargs == 0 && !ct->vararg) {
    rb_put(b, "void");
    return;
  }
  for (i = 0; i < ct->nargs; i++) {
    if (i > 0)
      rb_put(b, ", ");
    rb_put(b, ffi_type_repr(NULL, ct->args[i]));
  }
  if (ct->vararg) {
    if (ct->nargs > 0)
      rb_put(b, ", ");
    rb_put(b, "...");
  }
}


static void repr_type (ReprBuf *b, CType *ct) {
  if (ct == NULL) {
    rb_put(b, "?");
    return;
  }
  switch (ct->kind) {
    case CT_VOID:
      rb_put(b, "void");
      break;
    case CT_BOOL:
      rb_put(b, "bool");
      break;
    case CT_INT:
    case CT_FLOAT:
      rb_put(b, ct->name != NULL ? ct->name : "int");
      break;
    case CT_ENUM:
      rb_put(b, "enum ");
      rb_put(b, ct->name != NULL ? ct->name : "<anonymous>");
      break;
    case CT_STRUCT:
      rb_put(b, "struct ");
      rb_put(b, ct->name != NULL ? ct->name : "<anonymous>");
      break;
    case CT_UNION:
      rb_put(b, "union ");
      rb_put(b, ct->name != NULL ? ct->name : "<anonymous>");
      break;
    case CT_POINTER:
      if (ct->child != NULL && ct->child->kind == CT_FUNC) {
        CType *fn = ct->child;
        rb_put(b, ffi_type_repr(NULL, fn->ret));
        rb_put(b, " (*)(");
        repr_func_args(b, fn);
        rb_put(b, ")");
      }
      else if (ct->child != NULL && ct->child->kind == CT_ARRAY) {
        CType *ar = ct->child;
        rb_put(b, ffi_type_repr(NULL, ar->child));
        rb_put(b, " (*)[");
        if (ar->vla) rb_put(b, "?");
        else rb_putf(b, "%lu", (unsigned long)ar->length);
        rb_put(b, "]");
      }
      else {
        rb_put(b, ffi_type_repr(NULL, ct->child));
        rb_put(b, " *");
      }
      break;
    case CT_ARRAY:
      rb_put(b, ffi_type_repr(NULL, ct->child));
      rb_put(b, "[");
      if (ct->vla) rb_put(b, "?");
      else rb_putf(b, "%lu", (unsigned long)ct->length);
      rb_put(b, "]");
      break;
    case CT_FUNC:
      rb_put(b, ffi_type_repr(NULL, ct->ret));
      rb_put(b, " (");
      repr_func_args(b, ct);
      rb_put(b, ")");
      break;
  }
}


/*
** Note: ffi_type_repr() is called recursively with a NULL state in the
** helpers above; those calls only pass already-cached child reprs, so
** they hit the cache.  Build the cache bottom-up from the children.
*/


const char *ffi_type_repr (FFIState *S, CType *ct) {
  ReprBuf b;
  if (ct == NULL)
    return "?";
  if (ct->repr != NULL)
    return ct->repr;
  if (S == NULL)
    return "?";
  /* Make sure children have cached reprs first (avoids use-after-state
  ** surprises and lets the recursive helper hit the cache). */
  if (ct->child != NULL && ct->child->repr == NULL)
    (void)ffi_type_repr(S, ct->child);
  if (ct->kind == CT_FUNC) {
    if (ct->ret != NULL && ct->ret->repr == NULL)
      (void)ffi_type_repr(S, ct->ret);
  }
  if (ct->kind == CT_FUNC || (ct->kind == CT_POINTER && ct->child != NULL &&
                              ct->child->kind == CT_FUNC)) {
    CType *fn = (ct->kind == CT_FUNC) ? ct : ct->child;
    int i;
    if (fn->ret != NULL && fn->ret->repr == NULL)
      (void)ffi_type_repr(S, fn->ret);
    for (i = 0; i < fn->nargs; i++)
      if (fn->args[i]->repr == NULL)
        (void)ffi_type_repr(S, fn->args[i]);
  }
  if (ct->kind == CT_POINTER && ct->child != NULL &&
      ct->child->kind == CT_ARRAY && ct->child->child != NULL &&
      ct->child->child->repr == NULL)
    (void)ffi_type_repr(S, ct->child->child);

  memset(&b, 0, sizeof(b));
  repr_type(&b, ct);
  if (b.p == NULL)
    b.p = (char *)ffi_alloc(S, 1);
  ct->repr = ffi_strdup(S, b.p);
  free(b.p);
  return ct->repr;
}


/* ------------------------------------------------------------------ */
/* Builtin types                                                     */
/* ------------------------------------------------------------------ */

#define ALIGNOF_C(T)	offsetof(struct { char c; T x; }, x)

void ffi_type_init_builtins (lua_State *L, FFIState *S) {
  S->t_void = new_primitive(L, S, CT_VOID, "void", FFI_SIZE_UNKNOWN, 1, 0);
  S->t_bool = new_primitive(L, S, CT_BOOL, "bool", sizeof(_Bool),
                            ALIGNOF_C(_Bool), 0);
  S->t_char = new_primitive(L, S, CT_INT, "char", sizeof(char),
                            ALIGNOF_C(char), 0);
  S->t_schar = new_primitive(L, S, CT_INT, "signed char", sizeof(signed char),
                             ALIGNOF_C(signed char), 0);
  S->t_uchar = new_primitive(L, S, CT_INT, "unsigned char",
                             sizeof(unsigned char), ALIGNOF_C(unsigned char), 1);
  S->t_short = new_primitive(L, S, CT_INT, "short", sizeof(short),
                             ALIGNOF_C(short), 0);
  S->t_ushort = new_primitive(L, S, CT_INT, "unsigned short",
                              sizeof(unsigned short),
                              ALIGNOF_C(unsigned short), 1);
  S->t_int = new_primitive(L, S, CT_INT, "int", sizeof(int),
                           ALIGNOF_C(int), 0);
  S->t_uint = new_primitive(L, S, CT_INT, "unsigned int",
                            sizeof(unsigned int), ALIGNOF_C(unsigned int), 1);
  S->t_long = new_primitive(L, S, CT_INT, "long", sizeof(long),
                            ALIGNOF_C(long), 0);
  S->t_ulong = new_primitive(L, S, CT_INT, "unsigned long",
                             sizeof(unsigned long), ALIGNOF_C(unsigned long), 1);
  S->t_llong = new_primitive(L, S, CT_INT, "long long", sizeof(long long),
                             ALIGNOF_C(long long), 0);
  S->t_ullong = new_primitive(L, S, CT_INT, "unsigned long long",
                              sizeof(unsigned long long),
                              ALIGNOF_C(unsigned long long), 1);
  S->t_float = new_primitive(L, S, CT_FLOAT, "float", sizeof(float),
                             ALIGNOF_C(float), 0);
  S->t_double = new_primitive(L, S, CT_FLOAT, "double", sizeof(double),
                              ALIGNOF_C(double), 0);
  S->t_voidptr = ffi_type_ptr(L, S, S->t_void);
  S->t_charptr = ffi_type_ptr(L, S, S->t_char);

  /* Fixed-width and common typedef names.  They resolve to the
  ** underlying primitive type; see doc/ffi.md for the representation
  ** deviation this implies. */
  ffi_table_set_type(L, S, FFI_UV_TYPEDEFS, "int8_t", S->t_schar);
  ffi_table_set_type(L, S, FFI_UV_TYPEDEFS, "uint8_t", S->t_uchar);
  if (sizeof(int16_t) == sizeof(short)) {
    ffi_table_set_type(L, S, FFI_UV_TYPEDEFS, "int16_t", S->t_short);
    ffi_table_set_type(L, S, FFI_UV_TYPEDEFS, "uint16_t", S->t_ushort);
  }
  if (sizeof(int32_t) == sizeof(int)) {
    ffi_table_set_type(L, S, FFI_UV_TYPEDEFS, "int32_t", S->t_int);
    ffi_table_set_type(L, S, FFI_UV_TYPEDEFS, "uint32_t", S->t_uint);
  }
  {
    CType *i64 = (sizeof(int64_t) == sizeof(long)) ? S->t_long : S->t_llong;
    CType *u64 = (sizeof(uint64_t) == sizeof(unsigned long))
                     ? S->t_ulong : S->t_ullong;
    ffi_table_set_type(L, S, FFI_UV_TYPEDEFS, "int64_t", i64);
    ffi_table_set_type(L, S, FFI_UV_TYPEDEFS, "uint64_t", u64);
    ffi_table_set_type(L, S, FFI_UV_TYPEDEFS, "intmax_t", i64);
    ffi_table_set_type(L, S, FFI_UV_TYPEDEFS, "uintmax_t", u64);
    ffi_table_set_type(L, S, FFI_UV_TYPEDEFS, "intptr_t", i64);
    ffi_table_set_type(L, S, FFI_UV_TYPEDEFS, "uintptr_t", u64);
    ffi_table_set_type(L, S, FFI_UV_TYPEDEFS, "ptrdiff_t",
                       (sizeof(ptrdiff_t) == sizeof(long)) ? S->t_long : i64);
    ffi_table_set_type(L, S, FFI_UV_TYPEDEFS, "size_t", u64);
    ffi_table_set_type(L, S, FFI_UV_TYPEDEFS, "ssize_t", i64);
    ffi_table_set_type(L, S, FFI_UV_TYPEDEFS, "time_t", i64);
  }
}
