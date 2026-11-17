/*
** luar_ffi_call.c -- C library namespaces and libffi calls.
**
** ffi.C and ffi.load() resolve symbols with dlopen()/dlsym().  Calls
** go through libffi, including variadic prototypes (ffi_prep_cif_var)
** with the default argument promotions documented in doc/ffi.md.
** Callbacks from C into Lua and by-value unions are deliberately
** rejected.
**
** See Copyright Notice in lua.h and COPYRIGHT.
*/

#define LUA_LIB
#define LUAR_LIB

#include "lprefix.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>

#include "lua.h"

#include "lauxlib.h"
#include "luar_ffi.h"
#include "luar_ffi_internal.h"


typedef struct CLibrary {
  void *handle;
  int is_default;
} CLibrary;


/*
** libffi is loaded dynamically.  Keeping the library calls behind
** dlsym() means libluar.a has no unresolved libffi symbols, so embedding
** LuaR does not need to add -lffi to every host link line.  The FFI still
** needs libffi to be installed at run time.
*/
static void *libffi_handle = NULL;
static ffi_status (*libffi_prep_cif_p) (ffi_cif *, ffi_abi, unsigned,
                                        ffi_type *, ffi_type **);
static ffi_status (*libffi_prep_cif_var_p) (ffi_cif *, ffi_abi, unsigned,
                                            unsigned, ffi_type *,
                                            ffi_type **);
static void (*libffi_call_p) (ffi_cif *, void (*)(void), void *, void **);
static ffi_status (*libffi_get_struct_offsets_p) (ffi_abi, ffi_type *,
                                                  size_t *);
static ffi_type *libffi_type_void_p, *libffi_type_uint8_p;
static ffi_type *libffi_type_sint8_p, *libffi_type_uint16_p;
static ffi_type *libffi_type_sint16_p, *libffi_type_uint32_p;
static ffi_type *libffi_type_sint32_p, *libffi_type_uint64_p;
static ffi_type *libffi_type_sint64_p, *libffi_type_float_p;
static ffi_type *libffi_type_double_p, *libffi_type_pointer_p;
static void *(*libffi_closure_alloc_p) (size_t, void **);
static ffi_status (*libffi_prep_closure_loc_p) (ffi_closure *, ffi_cif *,
    void (*) (ffi_cif *, void *, void **, void *), void *, void *);
static void (*libffi_closure_free_p) (void *);

#define ffi_prep_cif		(*libffi_prep_cif_p)
#define ffi_prep_cif_var	(*libffi_prep_cif_var_p)
#define ffi_call		(*libffi_call_p)
#define ffi_get_struct_offsets	(*libffi_get_struct_offsets_p)
#define ffi_type_void		(*libffi_type_void_p)
#define ffi_type_uint8		(*libffi_type_uint8_p)
#define ffi_type_sint8		(*libffi_type_sint8_p)
#define ffi_type_uint16		(*libffi_type_uint16_p)
#define ffi_type_sint16		(*libffi_type_sint16_p)
#define ffi_type_uint32		(*libffi_type_uint32_p)
#define ffi_type_sint32		(*libffi_type_sint32_p)
#define ffi_type_uint64		(*libffi_type_uint64_p)
#define ffi_type_sint64		(*libffi_type_sint64_p)
#define ffi_type_float		(*libffi_type_float_p)
#define ffi_type_double		(*libffi_type_double_p)
#define ffi_type_pointer	(*libffi_type_pointer_p)
#define ffi_closure_alloc	(*libffi_closure_alloc_p)
#define ffi_prep_closure_loc	(*libffi_prep_closure_loc_p)
#define ffi_closure_free	(*libffi_closure_free_p)


static void *libffi_sym (lua_State *L, const char *name) {
  void *p;
  const char *err;
  dlerror();
  p = dlsym(libffi_handle, name);
  err = dlerror();
  if (err != NULL)
    luaL_error(L, "ffi: libffi is missing symbol '%s': %s", name, err);
  if (p == NULL)
    luaL_error(L, "ffi: libffi is missing symbol '%s'", name);
  return p;
}


void ffi_libffi_init (lua_State *L) {
  static const char *names[] = {
    "libffi.so.8", "libffi.so.7", "libffi.so",
    "libffi.dylib", NULL
  };
  const char *last_error = "not found";
  int i;
  if (libffi_handle != NULL)
    return;
  for (i = 0; names[i] != NULL; i++) {
    libffi_handle = dlopen(names[i], RTLD_LAZY | RTLD_LOCAL);
    if (libffi_handle != NULL)
      break;
    last_error = dlerror();
  }
  if (libffi_handle == NULL)
    luaL_error(L, "ffi: cannot load libffi (%s); install libffi 3.8+",
               last_error != NULL ? last_error : "not found");

  libffi_prep_cif_p = (ffi_status (*)(ffi_cif *, ffi_abi, unsigned,
                                      ffi_type *, ffi_type **))
      libffi_sym(L, "ffi_prep_cif");
  libffi_prep_cif_var_p = (ffi_status (*)(ffi_cif *, ffi_abi, unsigned,
                                          unsigned, ffi_type *,
                                          ffi_type **))
      libffi_sym(L, "ffi_prep_cif_var");
  libffi_call_p = (void (*)(ffi_cif *, void (*)(void), void *, void **))
      libffi_sym(L, "ffi_call");
  libffi_get_struct_offsets_p =
      (ffi_status (*)(ffi_abi, ffi_type *, size_t *))
      libffi_sym(L, "ffi_get_struct_offsets");
  libffi_type_void_p = (ffi_type *)libffi_sym(L, "ffi_type_void");
  libffi_type_uint8_p = (ffi_type *)libffi_sym(L, "ffi_type_uint8");
  libffi_type_sint8_p = (ffi_type *)libffi_sym(L, "ffi_type_sint8");
  libffi_type_uint16_p = (ffi_type *)libffi_sym(L, "ffi_type_uint16");
  libffi_type_sint16_p = (ffi_type *)libffi_sym(L, "ffi_type_sint16");
  libffi_type_uint32_p = (ffi_type *)libffi_sym(L, "ffi_type_uint32");
  libffi_type_sint32_p = (ffi_type *)libffi_sym(L, "ffi_type_sint32");
  libffi_type_uint64_p = (ffi_type *)libffi_sym(L, "ffi_type_uint64");
  libffi_type_sint64_p = (ffi_type *)libffi_sym(L, "ffi_type_sint64");
  libffi_type_float_p = (ffi_type *)libffi_sym(L, "ffi_type_float");
  libffi_type_double_p = (ffi_type *)libffi_sym(L, "ffi_type_double");
  libffi_type_pointer_p = (ffi_type *)libffi_sym(L, "ffi_type_pointer");
  libffi_closure_alloc_p =
      (void *(*)(size_t, void **))libffi_sym(L, "ffi_closure_alloc");
  libffi_prep_closure_loc_p =
      (ffi_status (*)(ffi_closure *, ffi_cif *,
                      void (*)(ffi_cif *, void *, void **, void *),
                      void *, void *))
          libffi_sym(L, "ffi_prep_closure_loc");
  libffi_closure_free_p =
      (void (*)(void *))libffi_sym(L, "ffi_closure_free");
}


static void check_clib (lua_State *L, int idx) {
  (void)luaL_checkudata(L, idx, LUAR_FFI_CLIB_MT);
}


static void *clib_getsym (lua_State *L, CLibrary *cl, const char *name,
                          int required) {
  void *p;
  const char *err;
  dlerror();
  p = dlsym(cl->handle, name);
  err = dlerror();
  if (err != NULL)
    luaL_error(L, "ffi: cannot resolve symbol '%s': %s", name, err);
  if (p == NULL && required)
    luaL_error(L, "ffi: cannot resolve symbol '%s'", name);
  return p;
}


static void push_symbol (lua_State *L, FFIState *S, CLibrary *cl,
                         const char *name) {
  CType *ct = ffi_table_get_type(L, S, FFI_UV_SYMS, name);
  if (ct == NULL) {
    int64_t v;
    if (ffi_table_get_int(L, S, FFI_UV_CONSTS, name, &v)) {
      lua_pushinteger(L, (lua_Integer)v);
      return;
    }
    luaL_error(L, "ffi.C: missing declaration for symbol '%s' "
                  "(declare it with ffi.cdef)", name);
  }
  if (ct->kind == CT_FUNC) {
    void *p = clib_getsym(L, cl, name, 1);
    ffi_push_func(L, S, ct, p);
  }
  else {
    void *p = clib_getsym(L, cl, name, 1);
    if (ct->kind == CT_STRUCT || ct->kind == CT_UNION ||
        ct->kind == CT_ARRAY) {
      size_t len = (ct->kind == CT_ARRAY && !ct->vla) ? ct->length : 0;
      ffi_push_ref(L, S, ct, p, 0, len);
    }
    else {
      ffi_push_value(L, S, ct, p, 0);
    }
  }
  /* Keep the library loaded while any symbol cdata derived from this
  ** namespace is alive (function cdata must not outlive its dlopen). */
  if (ffi_tocdata(L, -1) != NULL) {
    lua_pushvalue(L, 1);
    lua_setiuservalue(L, -2, 1);
  }
}


static int clib_index (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  CLibrary *cl;
  const char *name;
  check_clib(L, 1);
  cl = (CLibrary *)lua_touserdata(L, 1);
  name = luaL_checkstring(L, 2);
  push_symbol(L, S, cl, name);
  return 1;
}


static int clib_newindex (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  CLibrary *cl;
  const char *name;
  CType *ct;
  void *p;
  check_clib(L, 1);
  cl = (CLibrary *)lua_touserdata(L, 1);
  name = luaL_checkstring(L, 2);
  luaL_checkany(L, 3);
  ct = ffi_table_get_type(L, S, FFI_UV_SYMS, name);
  if (ct == NULL || ct->kind == CT_FUNC) {
    int64_t v;
    if (ffi_table_get_int(L, S, FFI_UV_CONSTS, name, &v))
      luaL_error(L, "ffi.C: cannot assign to constant '%s'", name);
    luaL_error(L, "ffi.C: cannot assign to '%s' (no writable "
                  "declaration)", name);
  }
  p = clib_getsym(L, cl, name, 1);
  ffi_convert(L, S, ct, p, 3, "C variable assignment");
  return 0;
}


static int clib_gc (lua_State *L) {
  if (lua_type(L, 1) == LUA_TUSERDATA) {
    CLibrary *cl = (CLibrary *)lua_touserdata(L, 1);
    if (cl != NULL && !cl->is_default && cl->handle != NULL) {
      dlclose(cl->handle);
      cl->handle = NULL;
    }
  }
  return 0;
}


void ffi_clib_initmt (lua_State *L) {
  if (luaL_newmetatable(L, LUAR_FFI_CLIB_MT)) {
    lua_pushcfunction(L, clib_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, clib_newindex);
    lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, clib_gc);
    lua_setfield(L, -2, "__gc");
    lua_pushliteral(L, "ffi C library");
    lua_setfield(L, -2, "__name");
  }
  lua_pop(L, 1);
}


void ffi_push_clib (lua_State *L, FFIState *S, void *handle,
                    int is_default) {
  CLibrary *cl;
  (void)S;
  cl = (CLibrary *)lua_newuserdatauv(L, sizeof(CLibrary), 0);
  cl->handle = handle;
  cl->is_default = is_default;
  luaL_setmetatable(L, LUAR_FFI_CLIB_MT);
}


void ffi_push_default_clib (lua_State *L, FFIState *S) {
  ffi_push_clib(L, S, RTLD_DEFAULT, 1);
}


/* ------------------------------------------------------------------ */
/* dlopen name resolution                                            */
/* ------------------------------------------------------------------ */

static int read_ld_script (const char *path, char *out, size_t outsz) {
  FILE *f = fopen(path, "r");
  char line[1024];
  if (f == NULL)
    return 0;
  while (fgets(line, sizeof(line), f) != NULL) {
    char *p;
    if (strncmp(line, "GROUP", 5) != 0 && strncmp(line, "INPUT", 5) != 0)
      continue;
    p = strchr(line, '(');
    if (p == NULL)
      continue;
    p++;
    while (*p == ' ' || *p == '\t')
      p++;
    {
      char *e = p;
      while (*e != '\0' && *e != ' ' && *e != '\t' && *e != ')' &&
             *e != '\n')
        e++;
      if (e > p && (size_t)(e - p) < outsz) {
        memcpy(out, p, (size_t)(e - p));
        out[e - p] = '\0';
        fclose(f);
        return 1;
      }
    }
  }
  fclose(f);
  return 0;
}


static void *try_dlopen (const char *name, int global) {
  int flags = RTLD_LAZY | (global ? RTLD_GLOBAL : RTLD_LOCAL);
  return dlopen(name, flags);
}


static void *load_library (lua_State *L, const char *name, int global) {
  char candidate[1024];
  void *h;
  const char *err;
  if (strchr(name, '/') != NULL) {
    h = try_dlopen(name, global);
    if (h != NULL)
      return h;
    luaL_error(L, "ffi.load: %s", dlerror());
  }
  if (strchr(name, '.') != NULL)
    snprintf(candidate, sizeof(candidate), "%s", name);
  else {
    const char *base = name;
    if (strncmp(name, "lib", 3) == 0)
      base = name + 3;
    snprintf(candidate, sizeof(candidate), "lib%s.so", base);
  }
  h = try_dlopen(candidate, global);
  if (h != NULL)
    return h;
  err = dlerror();
  /* glibc installs many libraries as GNU ld scripts (libc.so is one). */
  if (err != NULL && err[0] == '/') {
    char path[1024];
    const char *colon = strchr(err, ':');
    size_t n = colon != NULL ? (size_t)(colon - err) : strlen(err);
    if (n < sizeof(path)) {
      char real[1024];
      memcpy(path, err, n);
      path[n] = '\0';
      if (read_ld_script(path, real, sizeof(real))) {
        h = try_dlopen(real, global);
        if (h != NULL)
          return h;
      }
    }
  }
  luaL_error(L, "ffi.load: cannot load '%s': %s", name,
             err != NULL ? err : "dlopen failed");
  return NULL;
}


int ffi_load (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  if (lua_isnoneornil(L, 1)) {
    ffi_push_default_clib(L, S);
    return 1;
  }
  {
    const char *name = luaL_checkstring(L, 1);
    int global = lua_toboolean(L, 2);
    void *h = load_library(L, name, global);
    ffi_push_clib(L, S, h, 0);
    return 1;
  }
}


/* ------------------------------------------------------------------ */
/* libffi type construction                                          */
/* ------------------------------------------------------------------ */

typedef struct FTAlloc {
  struct FTAlloc *next;
  void *p;
} FTAlloc;


static void *ft_alloc (lua_State *L, FTAlloc **list, size_t sz) {
  FTAlloc *a = (FTAlloc *)malloc(sizeof(FTAlloc));
  void *p = malloc(sz);
  if (a == NULL || p == NULL) {
    free(a);
    free(p);
    luaL_error(L, "ffi: out of memory while preparing a call");
  }
  a->next = *list;
  a->p = p;
  *list = a;
  return p;
}


static void ft_free_all (FTAlloc *list) {
  while (list != NULL) {
    FTAlloc *next = list->next;
    free(list->p);
    free(list);
    list = next;
  }
}


static ffi_type *build_ffi_type (lua_State *L, FFIState *S, CType *ct,
                                 FTAlloc **allocs);


static ffi_type *build_array_type (lua_State *L, FFIState *S, CType *ct,
                                   FTAlloc **allocs) {
  ffi_type *ft, *elem;
  ffi_type **elems;
  size_t i, n = ct->length;
  if (ct->vla || n == 0)
    luaL_error(L, "ffi: variable/empty array '%s' cannot be passed by value",
               ffi_type_repr(S, ct));
  if (n > 65535)
    luaL_error(L, "ffi: array '%s' is too large to pass by value",
               ffi_type_repr(S, ct));
  elem = build_ffi_type(L, S, ct->child, allocs);
  ft = (ffi_type *)ft_alloc(L, allocs, sizeof(ffi_type));
  elems = (ffi_type **)ft_alloc(L, allocs, sizeof(ffi_type *) * (n + 1));
  for (i = 0; i < n; i++)
    elems[i] = elem;
  elems[n] = NULL;
  memset(ft, 0, sizeof(*ft));
  ft->type = FFI_TYPE_STRUCT;
  ft->elements = elems;
  ft->size = 0;
  ft->alignment = 0;
  return ft;
}


/* 0 = no member, 1 = INTEGER, 2 = SSE (SysV eightbyte classes). */
static void classify_union (CType *ct, size_t base, unsigned char *cls,
                            size_t nchunks) {
  uint16_t i;
  if (ct->incomplete)
    return;
  switch (ct->kind) {
    case CT_FLOAT: {
      size_t b = base / 8, e = (base + ct->size + 7) / 8;
      if (b < nchunks) {
        if (cls[b] == 0 || cls[b] == 2) cls[b] = 2;
        else cls[b] = 1;
      }
      (void)e;
      break;
    }
    case CT_INT:
    case CT_BOOL:
    case CT_ENUM:
    case CT_POINTER:
    case CT_FUNC: {
      size_t b = base / 8, e = (base + ct->size + 7) / 8;
      for (; b < e && b < nchunks; b++) cls[b] = 1;
      break;
    }
    case CT_ARRAY: {
      size_t esz = ct->child->size;
      size_t n = ct->length;
      size_t k;
      if (esz == 0)
        break;
      for (k = 0; k < n && base + k * esz < nchunks * 8; k++)
        classify_union(ct->child, base + k * esz, cls, nchunks);
      break;
    }
    case CT_STRUCT:
      for (i = 0; i < ct->nfields; i++) {
        CField *f = &ct->fields[i];
        if (f->bitwidth != 0 || f->zero_width) {
          size_t b = (base + f->offset) / 8;
          if (b < nchunks) cls[b] = 1;
        }
        else
          classify_union(f->type, base + f->offset, cls, nchunks);
      }
      break;
    case CT_UNION:
      for (i = 0; i < ct->nfields; i++)
        classify_union(ct->fields[i].type, base, cls, nchunks);
      break;
    default:
      break;
  }
}


static ffi_type *build_union_type (lua_State *L, FFIState *S, CType *ct,
                                   FTAlloc **allocs) {
  unsigned char cls[2];
  size_t nchunks;
  uint16_t i;
  if (ct->incomplete || ct->size == 0 || ct->size == FFI_SIZE_UNKNOWN)
    luaL_error(L, "ffi: incomplete union '%s' passed by value",
               ffi_type_repr(S, ct));
  if (ct->nfields == 0)
    luaL_error(L, "ffi: empty union '%s' cannot be passed by value",
               ffi_type_repr(S, ct));
  if (ct->size > 8 && ct->size < 16)
    luaL_error(L, "ffi: union '%s' has an unsupported size for by-value "
                  "passing", ffi_type_repr(S, ct));
  if (ct->size > 16) {
    size_t n, k;
    ffi_type *elem;
    ffi_type *ft;
    ffi_type **elems;
    if (ct->align != 1 && ct->align != 2 && ct->align != 4 &&
        ct->align != 8)
      luaL_error(L, "ffi: union '%s' has an unsupported alignment",
                 ffi_type_repr(S, ct));
    if (ct->size % ct->align != 0 ||
        ct->size / ct->align > 4096)
      luaL_error(L, "ffi: union '%s' is too large for by-value passing",
                 ffi_type_repr(S, ct));
    n = ct->size / ct->align;
    elem = (ct->align == 1) ? &ffi_type_uint8 :
           (ct->align == 2) ? &ffi_type_uint16 :
           (ct->align == 4) ? &ffi_type_uint32 : &ffi_type_uint64;
    ft = (ffi_type *)ft_alloc(L, allocs, sizeof(ffi_type));
    elems = (ffi_type **)ft_alloc(L, allocs, sizeof(ffi_type *) * (n + 1));
    for (k = 0; k < n; k++)
      elems[k] = elem;
    elems[n] = NULL;
    memset(ft, 0, sizeof(*ft));
    ft->type = FFI_TYPE_STRUCT;
    ft->elements = elems;
    ft->size = 0;
    ft->alignment = 0;
    return ft;
  }
  nchunks = ct->size > 8 ? 2 : 1;
  cls[0] = cls[1] = 0;
  for (i = 0; i < ct->nfields; i++)
    classify_union(ct->fields[i].type, 0, cls, nchunks);
  if (ct->size <= 8) {
    if (cls[0] == 2) {
      if (ct->size == 4) return &ffi_type_float;
      if (ct->size == 8) return &ffi_type_double;
    }
    switch (ct->size) {
      case 1: return &ffi_type_uint8;
      case 2: return &ffi_type_uint16;
      case 4: return &ffi_type_uint32;
      case 8: return &ffi_type_uint64;
      default: break;
    }
  }
  else {
    ffi_type *ft = (ffi_type *)ft_alloc(L, allocs, sizeof(ffi_type));
    ffi_type **elems = (ffi_type **)ft_alloc(L, allocs,
                                             sizeof(ffi_type *) * 3);
    elems[0] = (cls[0] == 2) ? &ffi_type_double : &ffi_type_uint64;
    elems[1] = (cls[1] == 2) ? &ffi_type_double : &ffi_type_uint64;
    elems[2] = NULL;
    memset(ft, 0, sizeof(*ft));
    ft->type = FFI_TYPE_STRUCT;
    ft->elements = elems;
    ft->size = 0;
    ft->alignment = 0;
    return ft;
  }
  luaL_error(L, "ffi: union '%s' cannot be passed by value",
             ffi_type_repr(S, ct));
  return NULL;
}


static ffi_type *build_struct_type (lua_State *L, FFIState *S, CType *ct,
                                    FTAlloc **allocs) {
  ffi_type *ft;
  ffi_type **elems;
  size_t *offs;
  ffi_status st;
  uint16_t i;
  if (ct->incomplete)
    luaL_error(L, "ffi: incomplete struct '%s' passed by value",
               ffi_type_repr(S, ct));
  if (ct->nfields == 0)
    luaL_error(L, "ffi: empty struct '%s' cannot be passed by value",
               ffi_type_repr(S, ct));
  ft = (ffi_type *)ft_alloc(L, allocs, sizeof(ffi_type));
  elems = (ffi_type **)ft_alloc(L, allocs,
                                sizeof(ffi_type *) * (ct->nfields + 1));
  memset(ft, 0, sizeof(*ft));
  ft->type = FFI_TYPE_STRUCT;
  for (i = 0; i < ct->nfields; i++) {
    CField *f = &ct->fields[i];
    if (f->bitwidth != 0 || f->zero_width)
      luaL_error(L, "ffi: struct '%s' has a bitfield and cannot be passed "
                    "by value", ffi_type_repr(S, ct));
    if (f->type->kind == CT_ARRAY)
      elems[i] = build_array_type(L, S, f->type, allocs);
    else
      elems[i] = build_ffi_type(L, S, f->type, allocs);
  }
  elems[ct->nfields] = NULL;
  ft->elements = elems;
  ft->size = 0;
  ft->alignment = 0;
  offs = (size_t *)malloc(sizeof(size_t) * (ct->nfields + 1));
  if (offs == NULL)
    luaL_error(L, "ffi: out of memory");
  st = ffi_get_struct_offsets(FFI_DEFAULT_ABI, ft, offs);
  if (st != FFI_OK) {
    free(offs);
    luaL_error(L, "ffi: libffi cannot represent struct '%s' by value",
               ffi_type_repr(S, ct));
  }
  for (i = 0; i < ct->nfields; i++) {
    if (offs[i] != ct->fields[i].offset) {
      free(offs);
      luaL_error(L, "ffi: struct '%s' layout is not supported by value "
                    "(field %s offset mismatch)",
                 ffi_type_repr(S, ct),
                 ct->fields[i].name != NULL ? ct->fields[i].name : "<anon>");
    }
  }
  if (ft->size != ct->size) {
    free(offs);
    luaL_error(L, "ffi: struct '%s' layout is not supported by value "
                  "(size mismatch)", ffi_type_repr(S, ct));
  }
  free(offs);
  return ft;
}


static ffi_type *build_ffi_type (lua_State *L, FFIState *S, CType *ct,
                                 FTAlloc **allocs) {
  switch (ct->kind) {
    case CT_VOID:
      return &ffi_type_void;
    case CT_BOOL:
      return &ffi_type_uint8;
    case CT_ENUM:
      return &ffi_type_sint32;
    case CT_INT:
      if (ct->size == 1)
        return ct->is_unsigned ? &ffi_type_uint8 : &ffi_type_sint8;
      if (ct->size == 2)
        return ct->is_unsigned ? &ffi_type_uint16 : &ffi_type_sint16;
      if (ct->size == 4)
        return ct->is_unsigned ? &ffi_type_uint32 : &ffi_type_sint32;
      if (ct->size == 8)
        return ct->is_unsigned ? &ffi_type_uint64 : &ffi_type_sint64;
      luaL_error(L, "ffi: unsupported integer size in '%s'",
                 ffi_type_repr(S, ct));
      return NULL;
    case CT_UNION:
      return build_union_type(L, S, ct, allocs);
    case CT_FLOAT:
      if (ct->size == 4)
        return &ffi_type_float;
      if (ct->size == 8)
        return &ffi_type_double;
      luaL_error(L, "ffi: unsupported floating-point size in '%s'",
                 ffi_type_repr(S, ct));
      return NULL;
    case CT_POINTER:
    case CT_FUNC:
      return &ffi_type_pointer;
    case CT_STRUCT:
      return build_struct_type(L, S, ct, allocs);
    case CT_ARRAY:
      luaL_error(L, "ffi: array '%s' cannot be passed by value",
                 ffi_type_repr(S, ct));
      return NULL;
    default:
      luaL_error(L, "ffi: unsupported C type '%s'",
                 ffi_type_repr(S, ct));
      return NULL;
  }
}


/* ------------------------------------------------------------------ */
/* Calls                                                             */
/* ------------------------------------------------------------------ */

typedef union ArgSlot {
  ffi_arg a;
  int64_t i;
  uint64_t u;
  double d;
  float f;
  void *p;
  long double ld;
  unsigned char bytes[32];
} ArgSlot;


static void *cdata_value_addr (CData *cd) {
  if ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0)
    return cd->v.p;
  return (void *)&cd->v;
}


static void convert_call_arg (lua_State *L, FFIState *S, CType *at,
                              int argidx, ArgSlot *slot, void **avalue,
                              ffi_type **argtype_override) {
  if (at->kind == CT_ARRAY || at->kind == CT_FUNC) {
    CType *pt = (at->kind == CT_ARRAY) ? ffi_type_ptr(L, S, at->child)
                                       : ffi_type_ptr(L, S, at);
    memset(slot, 0, sizeof(*slot));
    ffi_convert(L, S, pt, &slot->p, argidx, "function argument");
    *avalue = &slot->p;
    *argtype_override = &ffi_type_pointer;
    return;
  }
  if (at->kind == CT_STRUCT || at->kind == CT_UNION) {
    CData *cd = ffi_tocdata(L, argidx);
    if (cd == NULL || (cd->flags & CDATA_CTYPE) != 0 ||
        cd->ct != at)
      luaL_error(L, "ffi: argument %d: cannot convert %s to '%s'",
                 argidx, luaL_typename(L, argidx),
                 ffi_type_repr(S, at));
    *avalue = cdata_value_addr(cd);
    *argtype_override = NULL;
    return;
  }
  memset(slot, 0, sizeof(*slot));
  ffi_convert(L, S, at, slot, argidx, "function argument");
  switch (at->kind) {
    case CT_INT:
    case CT_ENUM:
      if (at->is_unsigned)
        slot->u = ffi_load_uint(slot->bytes, at->size);
      else
        slot->i = ffi_load_int(slot->bytes, at->size);
      break;
    case CT_BOOL:
      slot->u = ffi_load_uint(slot->bytes, at->size);
      break;
    case CT_FLOAT:
      /* ffi_convert wrote the exact float/double at the start. */
      break;
    case CT_POINTER:
    case CT_FUNC:
      break;
    default:
      luaL_error(L, "ffi: unsupported argument type '%s'",
                 ffi_type_repr(S, at));
  }
  *avalue = slot;
  *argtype_override = NULL;
}


/*
** Infer the C type of one argument in the variable part of a variadic
** call, following the promotion rules documented in doc/ffi.md: Lua
** integers become int when they fit and long long otherwise, Lua floats
** become double, strings become const char *, booleans become int, nil
** becomes a NULL void * and cdata scalars are promoted like C default
** arguments (small integers to int, float to double) or keep their
** declared 4/8-byte integer or pointer type.  libffi itself rejects
** smaller integer types and float in the variadic part.
** The converted value is stored in 'slot' and '*avalue' points at it.
** Strings are anchored on the Lua stack until the call returns.
*/
static ffi_type *convert_vararg (lua_State *L, FFIState *S, int argidx,
                                 ArgSlot *slot, void **avalue,
                                 FTAlloc **allocs) {
  int t = lua_type(L, argidx);
  memset(slot, 0, sizeof(*slot));
  switch (t) {
    case LUA_TNUMBER:
      if (lua_isinteger(L, argidx)) {
        lua_Integer v = lua_tointeger(L, argidx);
        if (v >= (lua_Integer)INT_MIN && v <= (lua_Integer)INT_MAX) {
          int32_t iv = (int32_t)v;
          memcpy(slot->bytes, &iv, sizeof(iv));
          *avalue = slot;
          return &ffi_type_sint32;
        }
        else {
          int64_t lv = (int64_t)v;
          memcpy(slot->bytes, &lv, sizeof(lv));
          *avalue = slot;
          return &ffi_type_sint64;
        }
      }
      else {
        double d = (double)lua_tonumber(L, argidx);
        memcpy(slot->bytes, &d, sizeof(d));
        *avalue = slot;
        return &ffi_type_double;
      }
    case LUA_TSTRING: {
      const char *s = lua_tolstring(L, argidx, NULL);
      slot->p = (void *)s;
      lua_pushvalue(L, argidx);	/* anchor until the call returns */
      *avalue = slot;
      return &ffi_type_pointer;
    }
    case LUA_TBOOLEAN: {
      int32_t iv = lua_toboolean(L, argidx) ? 1 : 0;
      memcpy(slot->bytes, &iv, sizeof(iv));
      *avalue = slot;
      return &ffi_type_sint32;
    }
    case LUA_TNIL:
      slot->p = NULL;
      *avalue = slot;
      return &ffi_type_pointer;
    case LUA_TLIGHTUSERDATA:
      slot->p = lua_touserdata(L, argidx);
      *avalue = slot;
      return &ffi_type_pointer;
    case LUA_TUSERDATA: {
      CData *cd = ffi_tocdata(L, argidx);
      CType *ct;
      if (cd == NULL)
        luaL_error(L, "ffi: cannot pass userdata as a variadic argument");
      if ((cd->flags & CDATA_CTYPE) != 0)
        luaL_error(L, "ffi: cannot pass a C type object as a variadic "
                      "argument");
      ct = cd->ct;
      switch (ct->kind) {
        case CT_STRUCT:
        case CT_UNION:
          luaL_error(L, "ffi: cannot pass %s '%s' as a variadic argument "
                        "(aggregate varargs are not portable)",
                     ct->kind == CT_STRUCT ? "struct" : "union",
                     ffi_type_repr(S, ct));
          return NULL;
        case CT_ARRAY:
          slot->p = cd->v.p;	/* arrays decay to a pointer */
          *avalue = slot;
          return &ffi_type_pointer;
        case CT_INT: {
          size_t sz = ct->size;
          void *p = cdata_value_addr(cd);
          if (sz < sizeof(int)) {
            /* C default argument promotion: int8/uint8/int16/uint16
            ** are passed as int. */
            int32_t iv = ct->is_unsigned
                ? (int32_t)ffi_load_uint(p, sz)
                : (int32_t)ffi_load_int(p, sz);
            memcpy(slot->bytes, &iv, sizeof(iv));
            *avalue = slot;
            return &ffi_type_sint32;
          }
          if (sz == 0 || sz > sizeof(slot->bytes))
            luaL_error(L, "ffi: unsupported variadic argument type '%s'",
                       ffi_type_repr(S, ct));
          memcpy(slot->bytes, p, sz);
          *avalue = slot;
          return build_ffi_type(L, S, ct, allocs);
        }
        case CT_ENUM: {
          int32_t iv = (int32_t)ffi_load_int(cdata_value_addr(cd),
                                             ct->size);
          memcpy(slot->bytes, &iv, sizeof(iv));
          *avalue = slot;
          return &ffi_type_sint32;
        }
        case CT_BOOL: {
          int32_t iv = ffi_load_uint(cdata_value_addr(cd), ct->size) ? 1 : 0;
          memcpy(slot->bytes, &iv, sizeof(iv));
          *avalue = slot;
          return &ffi_type_sint32;
        }
        case CT_FLOAT: {
          void *p = cdata_value_addr(cd);
          double d;
          if (ct->size == 4) {
            float f;
            memcpy(&f, p, sizeof(f));
            d = (double)f;
          }
          else if (ct->size == 8)
            memcpy(&d, p, sizeof(d));
          else
            luaL_error(L, "ffi: unsupported variadic argument type '%s'",
                       ffi_type_repr(S, ct));
          memcpy(slot->bytes, &d, sizeof(d));
          *avalue = slot;
          return &ffi_type_double;
        }
        case CT_POINTER:
        case CT_FUNC:
          slot->p = cd->v.p;
          *avalue = slot;
          return &ffi_type_pointer;
        default:
          luaL_error(L, "ffi: cannot pass '%s' as a variadic argument",
                     ffi_type_repr(S, ct));
          return NULL;
      }
    }
    default:
      luaL_error(L, "ffi: cannot pass %s as a variadic argument",
                 luaL_typename(L, argidx));
      return NULL;
  }
}


int ffi_call_cdata (lua_State *L, FFIState *S, CData *fn, int argstart) {
  CType *ft = (fn->ct->kind == CT_FUNC) ? fn->ct : fn->ct->child;
  int nargs;
  int provided;
  int ntotal;
  int i;
  ffi_cif cif;
  ffi_type **atypes = NULL;
  ffi_type *rtype;
  ArgSlot *slots = NULL;
  void **avalue = NULL;
  FTAlloc *allocs = NULL;
  int retkind;
  CType *rt;
  ArgSlot retslot;
  CData *retcd = NULL;

  nargs = (int)ft->nargs;
  provided = lua_gettop(L) - argstart + 1;
  if (ft->vararg) {
    if (nargs == 0)
      luaL_error(L, "ffi: cannot call variadic C function '%s' without "
                    "a fixed parameter", ffi_type_repr(S, ft));
    if (provided < nargs)
      luaL_error(L, "ffi: wrong number of arguments for '%s' "
                    "(expected at least %d, got %d)",
                 ffi_type_repr(S, ft), nargs, provided);
  }
  else if (provided != nargs)
    luaL_error(L, "ffi: wrong number of arguments for '%s' "
                  "(expected %d, got %d)",
               ffi_type_repr(S, ft), nargs, provided);
  ntotal = provided;

  atypes = (ffi_type **)malloc(sizeof(ffi_type *) *
                                (size_t)(ntotal > 0 ? ntotal : 1));
  slots = (ArgSlot *)malloc(sizeof(ArgSlot) *
                            (size_t)(ntotal > 0 ? ntotal : 1));
  avalue = (void **)malloc(sizeof(void *) *
                           (size_t)(ntotal > 0 ? ntotal : 1));
  if (atypes == NULL || slots == NULL || avalue == NULL) {
    free(atypes); free(slots); free(avalue);
    luaL_error(L, "ffi: out of memory while preparing a call");
  }
  memset(&retslot, 0, sizeof(retslot));

  for (i = 0; i < nargs; i++) {
    CType *at = ft->args[i];
    ffi_type *override = NULL;
    convert_call_arg(L, S, at, argstart + i, &slots[i], &avalue[i],
                     &override);
    if (override != NULL)
      atypes[i] = override;
    else if (at->kind == CT_ARRAY || at->kind == CT_FUNC)
      atypes[i] = &ffi_type_pointer;
    else
      atypes[i] = build_ffi_type(L, S, at, &allocs);
  }
  for (i = nargs; i < ntotal; i++)
    atypes[i] = convert_vararg(L, S, argstart + i, &slots[i], &avalue[i],
                               &allocs);
  rt = ft->ret;
  rtype = build_ffi_type(L, S, rt, &allocs);
  {
    ffi_status st = ft->vararg
      ? ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, (unsigned)nargs,
                         (unsigned)ntotal, rtype, atypes)
      : ffi_prep_cif(&cif, FFI_DEFAULT_ABI, (unsigned)ntotal, rtype, atypes);
    if (st != FFI_OK) {
      ft_free_all(allocs);
      free(atypes); free(slots); free(avalue);
      luaL_error(L, "ffi: libffi cannot prepare a call to '%s'",
                 ffi_type_repr(S, ft));
    }
  }

  retkind = rt->kind;
  if (retkind == CT_STRUCT || retkind == CT_UNION) {
    if (rt->incomplete || rt->size == FFI_SIZE_UNKNOWN)
      luaL_error(L, "ffi: incomplete return aggregate type");
    retcd = ffi_push_owned(L, S, rt, rt->size);
  }
  else if (retkind == CT_VOID) {
    retcd = NULL;
  }
  else if (retkind == CT_ARRAY) {
    luaL_error(L, "ffi: unsupported return type '%s'",
               ffi_type_repr(S, rt));
  }

  ffi_call(&cif, FFI_FN(fn->v.p),
           (retkind == CT_STRUCT || retkind == CT_UNION) ? retcd->v.p
                                                         : (void *)&retslot,
           avalue);
  {
    int saved_errno = errno;
    ft_free_all(allocs);
    free(atypes);
    free(slots);
    free(avalue);
    errno = saved_errno;	/* ffi.errno must see the callee's errno */
  }

  if (retkind == CT_VOID)
    return 0;
  if (retkind == CT_STRUCT || retkind == CT_UNION)
    return 1;
  ffi_push_value(L, S, rt, &retslot, 0);
  return 1;
}


/* ------------------------------------------------------------------ */
/* Callbacks from C into Lua (ffi.callback)                          */
/* ------------------------------------------------------------------ */

typedef struct FCallback {
  lua_State *L;
  FFIState *S;
  CType *ft;			/* function type (not the pointer) */
  ffi_cif cif;
  ffi_type **args;
  FTAlloc *allocs;
  ffi_closure *closure;
  void *code;
  int fnref;			/* registry ref to the Lua function */
} FCallback;


static void callback_set_error (lua_State *L, const char *msg) {
  lua_pushstring(L, msg != NULL ? msg : "callback error");
  lua_setfield(L, LUA_REGISTRYINDEX, "luar.ffi.callback_error");
}


static void callback_ret (lua_State *L, FFIState *S, CType *rt, int idx,
                          void *ret) {
  int ok = 1;
  switch (rt->kind) {
    case CT_INT:
    case CT_BOOL:
    case CT_ENUM: {
      int64_t v = ffi_to_int(L, S, idx, &ok);
      if (!ok) v = 0;
      ffi_store_int(ret, rt->size, v);
      break;
    }
    case CT_FLOAT: {
      double d = ffi_to_double(L, S, idx, &ok);
      if (!ok) d = 0;
      if (rt->size == 4) {
        float f = (float)d;
        memcpy(ret, &f, sizeof(f));
      }
      else
        memcpy(ret, &d, sizeof(d));
      break;
    }
    case CT_POINTER:
    case CT_FUNC: {
      void *p = ffi_to_ptr(L, S, idx, &ok);
      if (!ok) p = NULL;
      memcpy(ret, &p, sizeof(p));
      break;
    }
    case CT_STRUCT:
    case CT_UNION: {
      CData *cd = ffi_tocdata(L, idx);
      if (cd != NULL && (cd->flags & CDATA_CTYPE) == 0 &&
          cd->ct->kind == rt->kind && cd->ct->size == rt->size) {
        memcpy(ret, cd->v.p, rt->size);
      }
      else {
        memset(ret, 0, rt->size);
        callback_set_error(L, "callback returned an incompatible aggregate");
      }
      break;
    }
    default:
      if (rt->size != FFI_SIZE_UNKNOWN)
        memset(ret, 0, rt->size);
      break;
  }
}


static void callback_handler (ffi_cif *cif, void *ret, void **args,
                              void *ud) {
  FCallback *cb = (FCallback *)ud;
  lua_State *L = cb->L;
  int top = lua_gettop(L);
  int i, nargs = (int)cif->nargs;
  if (!lua_checkstack(L, nargs + 4)) {
    if (ret != NULL && cif->rtype->size != 0)
      memset(ret, 0, cif->rtype->size);
    return;
  }
  lua_rawgeti(L, LUA_REGISTRYINDEX, cb->fnref);
  for (i = 0; i < nargs; i++)
    ffi_push_value(L, cb->S, cb->ft->args[i], args[i], 0);
  if (lua_pcall(L, nargs, cb->ft->ret->kind == CT_VOID ? 0 : 1, 0) !=
      LUA_OK) {
    const char *msg = lua_tostring(L, -1);
    callback_set_error(L, msg);
    if (ret != NULL && cif->rtype->size != 0)
      memset(ret, 0, cif->rtype->size);
    lua_settop(L, top);
    return;
  }
  if (cb->ft->ret->kind != CT_VOID && ret != NULL)
    callback_ret(L, cb->S, cb->ft->ret, -1, ret);
  lua_settop(L, top);
}


int ffi_callback_new (lua_State *L, FFIState *S, CType *ct, int funcidx) {
  CType *ft = ct;
  FCallback *cb;
  FTAlloc *allocs = NULL;
  ffi_status st;
  int i, nargs;
  if (ft->kind == CT_POINTER && ft->child != NULL)
    ft = ft->child;
  if (ft->kind != CT_FUNC)
    return luaL_error(L, "ffi.callback: function type expected");
  if (ft->vararg)
    return luaL_error(L, "ffi.callback: variadic callbacks are not supported");
  if (ft->ret->kind == CT_ARRAY)
    return luaL_error(L, "ffi.callback: return type '%s' is not supported",
                      ffi_type_repr(S, ft->ret));
  nargs = ft->nargs;
  cb = (FCallback *)calloc(1, sizeof(FCallback));
  if (cb == NULL)
    return luaL_error(L, "ffi.callback: out of memory");
  cb->L = L;
  cb->S = S;
  cb->ft = ft;
  cb->fnref = LUA_NOREF;
  cb->args = (ffi_type **)calloc((size_t)(nargs > 0 ? nargs : 1),
                                 sizeof(ffi_type *));
  if (cb->args == NULL) {
    free(cb);
    return luaL_error(L, "ffi.callback: out of memory");
  }
  for (i = 0; i < nargs; i++)
    cb->args[i] = build_ffi_type(L, S, ft->args[i], &allocs);
  {
    ffi_type *rtype = build_ffi_type(L, S, ft->ret, &allocs);
    st = ffi_prep_cif(&cb->cif, FFI_DEFAULT_ABI, (unsigned)nargs, rtype,
                      cb->args);
    if (st != FFI_OK) {
      ft_free_all(allocs);
      free(cb->args);
      free(cb);
      return luaL_error(L, "ffi.callback: ffi_prep_cif failed (%d)", (int)st);
    }
  }
  cb->allocs = allocs;
  cb->closure = (ffi_closure *)ffi_closure_alloc(sizeof(ffi_closure),
                                                 &cb->code);
  if (cb->closure == NULL) {
    ft_free_all(cb->allocs);
    free(cb->args);
    free(cb);
    return luaL_error(L, "ffi.callback: cannot allocate a libffi closure");
  }
  lua_pushvalue(L, funcidx);
  cb->fnref = luaL_ref(L, LUA_REGISTRYINDEX);
  st = ffi_prep_closure_loc(cb->closure, &cb->cif, callback_handler, cb,
                            cb->code);
  if (st != FFI_OK) {
    luaL_unref(L, LUA_REGISTRYINDEX, cb->fnref);
    ffi_closure_free(cb->closure);
    ft_free_all(cb->allocs);
    free(cb->args);
    free(cb);
    return luaL_error(L, "ffi.callback: ffi_prep_closure_loc failed (%d)",
                      (int)st);
  }
  {
    CData *cd = ffi_push_cdata_object(L, S, ct,
                                      CDATA_FUNC | CDATA_CALLBACK, 0);
    cd->v.p = cb->code;
    cd->aux = cb;
  }
  return 1;
}


void ffi_callback_free (lua_State *L, CData *cd) {
  FCallback *cb = (FCallback *)cd->aux;
  (void)L;
  if (cb == NULL)
    return;
  cd->aux = NULL;
  if (cb->closure != NULL)
    ffi_closure_free(cb->closure);
  ft_free_all(cb->allocs);
  free(cb->args);
  if (cb->fnref != LUA_NOREF)
    luaL_unref(cb->L, LUA_REGISTRYINDEX, cb->fnref);
  free(cb);
}
