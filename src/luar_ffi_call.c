#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif
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
#include <sys/mman.h>
#include <unistd.h>

#ifndef RTLD_DEFAULT
#define RTLD_DEFAULT	((void *)-2)
#endif
#ifndef MAP_ANONYMOUS
# ifdef MAP_ANON
#  define MAP_ANONYMOUS	MAP_ANON
# else
#  define MAP_ANONYMOUS	0x1000	/* macOS MAP_ANON */
# endif
#endif

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
** Luar does not need to add -lffi to every host link line.  The FFI still
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


#if defined(__APPLE__)
typedef struct LuaR_TLV {
  void *(*thunk)(struct LuaR_TLV *);
  unsigned long key;
  unsigned long offset;
} LuaR_TLV;


static void *darwin_tls_value (void *desc) {
  LuaR_TLV *tlv = (LuaR_TLV *)desc;
  Dl_info info;
  if (tlv == NULL || tlv->thunk == NULL)
    return desc;
  if (dladdr((void *)tlv->thunk, &info) == 0)
    return desc;  /* not a TLV descriptor: dlsym gave the value address */
  return tlv->thunk(tlv);
}
#endif


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
#if defined(__APPLE__)
    {
      int64_t istls = 0;
      if (ffi_table_get_int(L, S, FFI_UV_TLS, name, &istls) && istls)
        p = darwin_tls_value(p);
    }
#endif
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
#if defined(__APPLE__)
  {
    int64_t istls = 0;
    if (ffi_table_get_int(L, S, FFI_UV_TLS, name, &istls) && istls)
      p = darwin_tls_value(p);
  }
#endif
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
#if defined(__APPLE__)
    snprintf(candidate, sizeof(candidate), "lib%s.dylib", base);
#else
    snprintf(candidate, sizeof(candidate), "lib%s.so", base);
#endif
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


#if defined(__aarch64__)
/* Detect an AAPCS64 homogeneous floating aggregate (1-4 FP members). */
static int union_hfa_info (CType *ct, int *base, int *count) {
  uint16_t i;
  if (ct->incomplete || ct->size == 0 || ct->size == FFI_SIZE_UNKNOWN)
    return 0;
  *base = 0;
  *count = 0;
  for (i = 0; i < ct->nfields; i++) {
    CType *f = ct->fields[i].type;
    int b, n;
    if (f->kind == CT_FLOAT) {
      b = (int)f->size;
      n = 1;
    }
    else if (f->kind == CT_ARRAY && f->child->kind == CT_FLOAT &&
             f->length > 0 && f->length <= 4) {
      b = (int)f->child->size;
      n = (int)f->length;
    }
    else if (f->kind == CT_UNION) {
      if (!union_hfa_info(f, &b, &n))
        return 0;
    }
    else
      return 0;
    if (b != 4 && b != 8)
      return 0;
    if (*base == 0) {
      *base = b;
      *count = n;
    }
    else if (*base != b || *count != n)
      return 0;
  }
  return *count >= 1 && *count <= 4 &&
         ct->size == (size_t)(*base * *count);
}


static ffi_type *build_hfa_type (lua_State *L, CType *ct, int base, int count,
                                 FTAlloc **allocs) {
  (void)ct;
  ffi_type **elems;
  ffi_type *ft;
  int i;
  if (count == 1)
    return (base == 4) ? &ffi_type_float : &ffi_type_double;
  ft = (ffi_type *)ft_alloc(L, allocs, sizeof(ffi_type));
  elems = (ffi_type **)ft_alloc(L, allocs,
                                sizeof(ffi_type *) * (size_t)(count + 1));
  for (i = 0; i < count; i++)
    elems[i] = (base == 4) ? &ffi_type_float : &ffi_type_double;
  elems[count] = NULL;
  memset(ft, 0, sizeof(*ft));
  ft->type = FFI_TYPE_STRUCT;
  ft->elements = elems;
  return ft;
}
#endif


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
#if defined(__aarch64__)
  {
    int hbase, hcount;
    if (union_hfa_info(ct, &hbase, &hcount))
      return build_hfa_type(L, ct, hbase, hcount, allocs);
  }
#endif
  nchunks = ct->size > 8 ? 2 : 1;
  cls[0] = cls[1] = 0;
  for (i = 0; i < ct->nfields; i++)
    classify_union(ct->fields[i].type, 0, cls, nchunks);
  if (ct->size > 8 && ct->size < 16) {
    ffi_type *ft;
    ffi_type **elems;
    size_t n = 0, k;
    if (ct->size % ct->align != 0 ||
        (ct->align != 1 && ct->align != 2 && ct->align != 4))
      luaL_error(L, "ffi: union '%s' has an unsupported size for by-value "
                    "passing", ffi_type_repr(S, ct));
    if (ct->align == 4 && ct->size == 12) {
      n = 3;
      ft = (ffi_type *)ft_alloc(L, allocs, sizeof(ffi_type));
      elems = (ffi_type **)ft_alloc(L, allocs,
                                    sizeof(ffi_type *) * (n + 1));
      elems[0] = (cls[0] == 2) ? &ffi_type_float : &ffi_type_uint32;
      elems[1] = elems[0];
      elems[2] = (cls[1] == 2) ? &ffi_type_float : &ffi_type_uint32;
      elems[3] = NULL;
    }
    else {
      n = ct->size / ct->align;
      ft = (ffi_type *)ft_alloc(L, allocs, sizeof(ffi_type));
      elems = (ffi_type **)ft_alloc(L, allocs,
                                    sizeof(ffi_type *) * (n + 1));
      for (k = 0; k < n; k++)
        elems[k] = (ct->align == 1) ? &ffi_type_uint8 :
                   (ct->align == 2) ? &ffi_type_uint16 : &ffi_type_uint32;
      elems[n] = NULL;
    }
    memset(ft, 0, sizeof(*ft));
    ft->type = FFI_TYPE_STRUCT;
    ft->elements = elems;
    ft->size = 0;
    ft->alignment = 0;
    return ft;
  }
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
        ct->size / ct->align > 1048576)
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





#if defined(__aarch64__)
/* Packed aggregate arguments are byte-addressed on AAPCS64.  A byte-array
** struct preserves the exact memory image for argument-only calls. */
static ffi_type *build_packed_bytes (lua_State *L, CType *ct,
                                     FTAlloc **allocs) {
  ffi_type *ft;
  ffi_type **elems;
  size_t n, i;
  if (ct->size == 0 || ct->size > 65535)
    luaL_error(L, "ffi: packed struct is too large to pass by value");
  n = ct->size;
  ft = (ffi_type *)ft_alloc(L, allocs, sizeof(ffi_type));
  elems = (ffi_type **)ft_alloc(L, allocs,
                                sizeof(ffi_type *) * (n + 1));
  for (i = 0; i < n; i++)
    elems[i] = &ffi_type_uint8;
  elems[n] = NULL;
  memset(ft, 0, sizeof(*ft));
  ft->type = FFI_TYPE_STRUCT;
  ft->elements = elems;
  ft->size = 0;
  ft->alignment = 0;
  return ft;
}
#endif


static ffi_type *build_struct_type (lua_State *L, FFIState *S, CType *ct,
                                    FTAlloc **allocs) {
  ffi_type *ft;
  ffi_type **elems;
  size_t *offs;
  uint16_t *group_field;
  size_t ngroups = 0, g;
  ffi_status st;
  uint16_t i;
  if (ct->incomplete)
    luaL_error(L, "ffi: incomplete struct '%s' passed by value",
               ffi_type_repr(S, ct));
  if (ct->nfields == 0)
    luaL_error(L, "ffi: empty struct '%s' cannot be passed by value",
               ffi_type_repr(S, ct));
#if defined(__aarch64__)
  if (ct->pack == 1 && ct->align_override == 0)
    return build_packed_bytes(L, ct, allocs);
#endif
  ft = (ffi_type *)ft_alloc(L, allocs, sizeof(ffi_type));
  elems = (ffi_type **)ft_alloc(L, allocs,
                                sizeof(ffi_type *) * (ct->nfields + 1));
  group_field = (uint16_t *)malloc(sizeof(uint16_t) * (ct->nfields + 1));
  if (group_field == NULL)
    luaL_error(L, "ffi: out of memory");
  memset(ft, 0, sizeof(*ft));
  ft->type = FFI_TYPE_STRUCT;
  for (i = 0; i < ct->nfields; i++) {
    CField *f = &ct->fields[i];
    if (f->zero_width)
      continue;   /* no storage */
    if (f->bitwidth != 0) {
      /* One synthetic integer field covers a bitfield storage run. */
      CField *prev = (ngroups > 0) ? &ct->fields[group_field[ngroups - 1]]
                                   : NULL;
      if (prev != NULL && prev->bitwidth != 0 &&
          ((prev->offset / ffi_type_size(prev->type)) *
           ffi_type_size(prev->type)) ==
          ((f->offset / ffi_type_size(f->type)) * ffi_type_size(f->type))) {
        continue;
      }
      {
        size_t sz = ffi_type_size(f->type);
        if (sz == 1) elems[ngroups] = &ffi_type_uint8;
        else if (sz == 2) elems[ngroups] = &ffi_type_uint16;
        else if (sz == 4) elems[ngroups] = &ffi_type_uint32;
        else if (sz == 8) elems[ngroups] = &ffi_type_uint64;
        else {
          free(group_field);
          luaL_error(L, "ffi: struct '%s' has an unsupported bitfield "
                        "storage size", ffi_type_repr(S, ct));
        }
      }
    }
    else if (f->type->kind == CT_ARRAY)
      elems[ngroups] = build_array_type(L, S, f->type, allocs);
    else
      elems[ngroups] = build_ffi_type(L, S, f->type, allocs);
    group_field[ngroups] = i;
    ngroups++;
  }
  elems[ngroups] = NULL;
  ft->elements = elems;
  ft->size = 0;
  ft->alignment = 0;
  offs = (size_t *)malloc(sizeof(size_t) * (ngroups + 1));
  if (offs == NULL)
    luaL_error(L, "ffi: out of memory");
  st = ffi_get_struct_offsets(FFI_DEFAULT_ABI, ft, offs);
  if (st != FFI_OK) {
    free(offs);
    luaL_error(L, "ffi: libffi cannot represent struct '%s' by value",
               ffi_type_repr(S, ct));
  }
  for (g = 0; g < ngroups; g++) {
    uint16_t fi = group_field[g];
    CField *ff = &ct->fields[fi];
    size_t expected = ff->offset;
    if (ff->bitwidth != 0) {
      size_t sz = ffi_type_size(ff->type);
      expected = (ff->offset / sz) * sz;
    }
    if (offs[g] != expected) {
      free(offs);
      luaL_error(L, "ffi: struct '%s' layout is not supported by value "
                    "(field %s offset mismatch)",
                 ffi_type_repr(S, ct),
                 ct->fields[fi].name != NULL ? ct->fields[fi].name : "<anon>");
    }
  }
  if (ft->size != ct->size) {
    free(offs);
    luaL_error(L, "ffi: struct '%s' layout is not supported by value "
                  "(size mismatch)", ffi_type_repr(S, ct));
  }
  free(offs);
  free(group_field);
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
#if defined(__aarch64__)
    /* AAPCS64 passes non-HFA composites larger than 16 bytes by reference.
    ** HFA unions keep their natural double/float struct type. */
    {
      int hbase, hcount;
      if (at->kind == CT_UNION && at->size > 16 &&
          !union_hfa_info(at, &hbase, &hcount))
        *argtype_override = &ffi_type_pointer;
      else
        *argtype_override = NULL;
    }
#else
    *argtype_override = NULL;
#endif
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


static int packed_int_type (CType *ct) {
  uint16_t i;
  if (ct->incomplete)
    return 0;
  switch (ct->kind) {
    case CT_INT: case CT_BOOL: case CT_ENUM:
    case CT_POINTER: case CT_FUNC:
      return 1;
    case CT_ARRAY:
      return packed_int_type(ct->child);
    case CT_STRUCT: case CT_UNION:
      for (i = 0; i < ct->nfields; i++) {
        CField *f = &ct->fields[i];
        if (f->bitwidth == 0 && f->zero_width)
          continue;
        if (!packed_int_type(f->type))
          return 0;
      }
      return 1;
    default:
      return 0;
  }
}


static int packed_scalar_arg (CType *ct) {
  switch (ct->kind) {
    case CT_INT: case CT_BOOL: case CT_ENUM: case CT_FLOAT:
    case CT_POINTER: case CT_FUNC:
      return 1;
    default:
      return 0;
  }
}


static int packed_return_signature (CType *ft) {
  int i, ints = 0;
  if (ft->nargs > 5)
    return 0;
  for (i = 0; i < (int)ft->nargs; i++) {
    if (!packed_scalar_arg(ft->args[i]))
      return 0;
    if (ft->args[i]->kind != CT_FLOAT)
      ints++;
  }
  return ints <= 5;
}


#if defined(__x86_64__)
static void packed_emit_mov_reg (unsigned char *code, size_t *n,
                                 int dst, int src) {
  unsigned char rex = (unsigned char)(0x48 |
      (src >= 8 ? 4 : 0) | (dst >= 8 ? 1 : 0));
  code[(*n)++] = rex;
  code[(*n)++] = 0x89;
  code[(*n)++] = (unsigned char)(0xc0 | ((src & 7) << 3) | (dst & 7));
}


static void packed_emit_store_rsp (unsigned char *code, size_t *n,
                                   int src, int disp) {
  unsigned char rex = (unsigned char)(0x48 | (src >= 8 ? 4 : 0));
  code[(*n)++] = rex;
  code[(*n)++] = 0x89;
  code[(*n)++] = (unsigned char)(0x40 | ((src & 7) << 3) | 4);
  code[(*n)++] = 0x24;
  code[(*n)++] = (unsigned char)disp;
}


static int packed_mixed_call (lua_State *L, FFIState *S, CData *fn,
                              CType *ft, CType *rt, int argstart,
                              const int *pidx, int pn) {
  uint64_t chunks[6] = {0, 0, 0, 0, 0, 0};
  size_t arg_off[5], arg_chunks[5], chunk_n = 0, off = 16;
  int pc = 0;
  unsigned char code[128];
  size_t n = 0, i;
  void *mem;
  size_t pagesize = (size_t)sysconf(_SC_PAGESIZE);
  ffi_type *atypes[6];
  ffi_cif cif;
  unsigned char *retbuf = (unsigned char *)calloc(1, rt->size > 0 ? rt->size : 1);
  ArgSlot slots[5];
  ffi_type *stype[5];
  void *sval[5];
  void *values[6];
  int nscalars = 0, nints = 0, nargs = (int)ft->nargs;
  CData *out;
  ffi_status st;
  int saved_errno;
  if (retbuf == NULL)
    luaL_error(L, "ffi: out of memory for packed return");
  for (i = 0; i < (size_t)nargs; i++) {
    if (pc < pn && (int)i == pidx[pc]) {
      CType *at = ft->args[i];
      CData *cd = ffi_tocdata(L, argstart + (int)i);
      size_t ac = (at->size + 7) / 8, k;
      void *src;
      if (cd == NULL || (cd->flags & CDATA_CTYPE) != 0 || cd->ct != at)
        luaL_error(L, "ffi: argument %d: cannot convert %s to '%s'",
                   argstart + (int)i, luaL_typename(L, argstart + (int)i),
                   ffi_type_repr(S, at));
      if (chunk_n + ac > 6)
        luaL_error(L, "ffi: packed call uses too many argument chunks");
      src = ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0)
                ? cd->v.p : (void *)&cd->v;
      memcpy(&chunks[chunk_n], src, at->size);
      arg_off[pc] = off;
      arg_chunks[pc] = ac;
      for (k = 0; k < ac; k++) {
        atypes[chunk_n + k] = &ffi_type_uint64;
        values[chunk_n + k] = &chunks[chunk_n + k];
      }
      chunk_n += ac;
      off += ac * 8;
      pc++;
    }
    else {
      ffi_type *aoverride = NULL;
      memset(&slots[nscalars], 0, sizeof(slots[nscalars]));
      convert_call_arg(L, S, ft->args[i], argstart + (int)i,
                       &slots[nscalars], &sval[nscalars], &aoverride);
      stype[nscalars] = (aoverride != NULL)
                            ? aoverride
                            : build_ffi_type(L, S, ft->args[i], NULL);
      if (ft->args[i]->kind != CT_FLOAT)
        nints++;
      nscalars++;
    }
  }
  for (i = 0; i < (size_t)nscalars; i++) {
    atypes[chunk_n + i] = stype[i];
    values[chunk_n + i] = sval[i];
  }
  if (pc != pn || chunk_n + nscalars > 6)
    luaL_error(L, "ffi: unsupported mixed packed call");
  /* sub rsp,72; store packed chunks; shift scalar integer registers past
     the sret pointer; mov rdi,retbuf; add rsp,16; mov rax,target;
     call rax; sub rsp,16; add rsp,72; ret */
  code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xec; code[n++] = 0x48;
  {
    static const int regs[6] = {7, 6, 2, 1, 8, 9};
    size_t ci = 0;
    for (i = 0; i < (size_t)pn; i++) {
      size_t k;
      for (k = 0; k < arg_chunks[i]; k++)
        packed_emit_store_rsp(code, &n, regs[ci++],
                              (int)(arg_off[i] + 8 * k));
    }
  }
  {
    static const int wrapper[6] = {7, 6, 2, 1, 8, 9};
    static const int target[5] = {6, 2, 1, 8, 9};
    for (i = 0; i < (size_t)nints; i++)
      packed_emit_mov_reg(code, &n, target[i], wrapper[chunk_n + i]);
  }
  code[n++] = 0x48; code[n++] = 0xbf;
  {
    uint64_t rb = (uint64_t)(uintptr_t)retbuf;
    for (i = 0; i < 8; i++) code[n++] = (unsigned char)(rb >> (8 * i));
  }
  code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xc4; code[n++] = 0x10;
  code[n++] = 0x48; code[n++] = 0xb8;
  {
    uint64_t targetfn = (uint64_t)(uintptr_t)fn->v.p;
    for (i = 0; i < 8; i++)
      code[n++] = (unsigned char)(targetfn >> (8 * i));
  }
  code[n++] = 0xff; code[n++] = 0xd0;
  code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xec; code[n++] = 0x10;
  code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xc4; code[n++] = 0x48;
  code[n++] = 0xc3;
  st = ffi_prep_cif(&cif, FFI_DEFAULT_ABI,
                    (unsigned)(chunk_n + nscalars), &ffi_type_void, atypes);
  if (st != FFI_OK)
    luaL_error(L, "ffi: libffi cannot prepare a mixed packed call");
  mem = mmap(NULL, pagesize, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mem == MAP_FAILED)
    luaL_error(L, "ffi: cannot allocate a mixed packed thunk");
  memcpy(mem, code, n);
  if (mprotect(mem, pagesize, PROT_READ | PROT_EXEC) != 0) {
    munmap(mem, pagesize);
    luaL_error(L, "ffi: cannot make the mixed packed thunk executable");
  }
  ffi_call(&cif, FFI_FN(mem), NULL, values);
  saved_errno = errno;
  munmap(mem, pagesize);
  errno = saved_errno;
  out = ffi_push_owned(L, S, rt, rt->size);
  memcpy(out->v.p, retbuf, rt->size);
  free(retbuf);
  return 1;
}
#endif


#if defined(__x86_64__)
static int packed_return_call (lua_State *L, FFIState *S, CData *fn,
                               CType *ft, CType *rt, int argstart) {
  unsigned char code[64];
  size_t n = 0, i;
  void *mem;
  size_t pagesize = (size_t)sysconf(_SC_PAGESIZE);
  ffi_type *atypes[5];
  ffi_cif cif;
  unsigned char *retbuf = (unsigned char *)calloc(1, rt->size > 0 ? rt->size : 1);
  ArgSlot slots[5];
  void *values[5];
  int nargs = (int)ft->nargs;
  int ints = 0;
  CData *cd;
  ffi_status st;
  int saved_errno;
  for (i = 0; i < (size_t)nargs; i++) {
    ffi_type *aoverride = NULL;
    memset(&slots[i], 0, sizeof(slots[i]));
    convert_call_arg(L, S, ft->args[i], argstart + (int)i, &slots[i],
                     &values[i], &aoverride);
    atypes[i] = (aoverride != NULL) ? aoverride
                                    : build_ffi_type(L, S, ft->args[i], NULL);
    if (ft->args[i]->kind != CT_FLOAT)
      ints++;
  }
  /* sub rsp,40; shift integer args past the sret pointer; mov rdi,rsp;
     mov rax,target; call rax; mov rax,[rsp]; mov rdx,[rsp+8];
     add rsp,40; ret */
  code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xec; code[n++] = 0x28;
  for (i = (size_t)ints; i > 0; i--) {
    /* wrapper integer regs: rdi,rsi,rdx,rcx,r8; target: rsi,rdx,rcx,r8,r9 */
    static const unsigned char movs[5][3] = {
      {0x48, 0x89, 0xfe},	/* mov rsi,rdi */
      {0x48, 0x89, 0xf2},	/* mov rdx,rsi */
      {0x48, 0x89, 0xd1},	/* mov rcx,rdx */
      {0x49, 0x89, 0xc8},	/* mov r8,rcx */
      {0x4d, 0x89, 0xc1}	/* mov r9,r8 */
    };
    code[n++] = movs[i - 1][0];
    code[n++] = movs[i - 1][1];
    code[n++] = movs[i - 1][2];
  }
  if (retbuf == NULL)
    luaL_error(L, "ffi: out of memory for packed return");
  code[n++] = 0x48; code[n++] = 0xbf; /* mov rdi,retbuf */
  {
    uint64_t rb = (uint64_t)(uintptr_t)retbuf;
    for (i = 0; i < 8; i++) code[n++] = (unsigned char)(rb >> (8 * i));
  }
  code[n++] = 0x48; code[n++] = 0xb8;
  {
    uint64_t target = (uint64_t)(uintptr_t)fn->v.p;
    for (i = 0; i < 8; i++) code[n++] = (unsigned char)(target >> (8 * i));
  }
  code[n++] = 0xff; code[n++] = 0xd0;
  code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xc4; code[n++] = 0x28;
  code[n++] = 0xc3;
  st = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, (unsigned)nargs,
                    &ffi_type_void, nargs > 0 ? atypes : NULL);
  if (st != FFI_OK)
    luaL_error(L, "ffi: libffi cannot prepare a packed return");
  mem = mmap(NULL, pagesize, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mem == MAP_FAILED)
    luaL_error(L, "ffi: cannot allocate a packed-return thunk");
  memcpy(mem, code, n);
  if (mprotect(mem, pagesize, PROT_READ | PROT_EXEC) != 0) {
    munmap(mem, pagesize);
    luaL_error(L, "ffi: cannot make the packed-return thunk executable");
  }
  ffi_call(&cif, FFI_FN(mem), NULL, nargs > 0 ? values : NULL);
  saved_errno = errno;
  munmap(mem, pagesize);
  errno = saved_errno;
  cd = ffi_push_owned(L, S, rt, rt->size);
  memcpy(cd->v.p, retbuf, rt->size);
  free(retbuf);
  return 1;
}
#endif


#if defined(__x86_64__)
static int packed_multi_call (lua_State *L, FFIState *S, CData *fn,
                              CType *ft, CType *rt, int argstart) {
  uint64_t chunks[6] = {0, 0, 0, 0, 0, 0};
  size_t chunk_count = 0, arg_off[3], arg_chunks[3];
  unsigned char code[128];
  size_t n = 0, i, ci;
  void *mem;
  size_t pagesize = (size_t)sysconf(_SC_PAGESIZE);
  ffi_type *atypes[6];
  ffi_cif cif;
  unsigned char *retbuf = (unsigned char *)calloc(1, rt->size > 0 ? rt->size : 1);
  void *values[6];
  CData *out;
  ffi_status st;
  int saved_errno;
  size_t off = 16;
  if (ft->nargs < 1 || ft->nargs > 3)
    luaL_error(L, "ffi: unsupported packed call shape");
  for (i = 0; i < (size_t)ft->nargs; i++) {
    CType *at = ft->args[i];
    CData *cd = ffi_tocdata(L, argstart + (int)i);
    void *src;
    size_t ac = (at->size + 7) / 8;
    if (cd == NULL || (cd->flags & CDATA_CTYPE) != 0 || cd->ct != at)
      luaL_error(L, "ffi: argument %d: cannot convert %s to '%s'",
                 argstart + (int)i, luaL_typename(L, argstart + (int)i),
                 ffi_type_repr(S, at));
    arg_off[i] = off;
    arg_chunks[i] = ac;
    src = ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0)
              ? cd->v.p : (void *)&cd->v;
    memcpy(&chunks[chunk_count], src, at->size);
    chunk_count += ac;
    off += ac * 8;
  }
  if (chunk_count > 6)
    luaL_error(L, "ffi: packed call uses too many argument chunks");
  /* sub rsp,72; store chunks; mov rdi,rsp; add rsp,16; mov rax,target;
     call rax; sub rsp,16; mov rax,[rsp]; mov rdx,[rsp+8]; add rsp,72; ret */
  code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xec; code[n++] = 0x48;
  {
    static const unsigned char regs[6] = {7, 6, 2, 1, 8, 0};
    /* rdi,rsi,rdx,rcx,r8,r9 */
    for (ci = 0; ci < chunk_count; ci++) {
      size_t ai, disp = 0;
      /* find argument and slot for this chunk */
      size_t running = 0;
      for (ai = 0; ai < (size_t)ft->nargs; ai++) {
        if (ci < running + arg_chunks[ai]) {
          disp = arg_off[ai] + (ci - running) * 8;
          break;
        }
        running += arg_chunks[ai];
      }
      switch (ci) {
        case 0: code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0x7c;
                code[n++] = 0x24; code[n++] = (unsigned char)disp; break;
        case 1: code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0x74;
                code[n++] = 0x24; code[n++] = (unsigned char)disp; break;
        case 2: code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0x54;
                code[n++] = 0x24; code[n++] = (unsigned char)disp; break;
        case 3: code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0x4c;
                code[n++] = 0x24; code[n++] = (unsigned char)disp; break;
        case 4: code[n++] = 0x4c; code[n++] = 0x89; code[n++] = 0x44;
                code[n++] = 0x24; code[n++] = (unsigned char)disp; break;
        default: code[n++] = 0x4c; code[n++] = 0x89; code[n++] = 0x4c;
                code[n++] = 0x24; code[n++] = (unsigned char)disp; break;
      }
      (void)regs;
    }
  }
  if (retbuf == NULL)
    luaL_error(L, "ffi: out of memory for packed return");
  code[n++] = 0x48; code[n++] = 0xbf;
  {
    uint64_t rb = (uint64_t)(uintptr_t)retbuf;
    for (i = 0; i < 8; i++) code[n++] = (unsigned char)(rb >> (8 * i));
  }
  code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xc4; code[n++] = 0x10;
  code[n++] = 0x48; code[n++] = 0xb8;
  {
    uint64_t target = (uint64_t)(uintptr_t)fn->v.p;
    for (i = 0; i < 8; i++) code[n++] = (unsigned char)(target >> (8 * i));
  }
  code[n++] = 0xff; code[n++] = 0xd0;
  code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xec; code[n++] = 0x10;
  code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xc4; code[n++] = 0x48;
  code[n++] = 0xc3;
  for (i = 0; i < chunk_count; i++)
    atypes[i] = &ffi_type_uint64;
  st = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, (unsigned)chunk_count,
                    &ffi_type_void, atypes);
  if (st != FFI_OK)
    luaL_error(L, "ffi: libffi cannot prepare a packed call");
  mem = mmap(NULL, pagesize, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mem == MAP_FAILED)
    luaL_error(L, "ffi: cannot allocate a packed-call thunk");
  memcpy(mem, code, n);
  if (mprotect(mem, pagesize, PROT_READ | PROT_EXEC) != 0) {
    munmap(mem, pagesize);
    luaL_error(L, "ffi: cannot make the packed-call thunk executable");
  }
  for (i = 0; i < chunk_count; i++)
    values[i] = &chunks[i];
  ffi_call(&cif, FFI_FN(mem), NULL, values);
  saved_errno = errno;
  munmap(mem, pagesize);
  errno = saved_errno;
  out = ffi_push_owned(L, S, rt, rt->size);
  memcpy(out->v.p, retbuf, rt->size);
  free(retbuf);
  return 1;
}
#endif


#if defined(__x86_64__)
static int packed_single_call (lua_State *L, FFIState *S, CData *fn,
                               CType *at, CType *rt, int argidx) {
  CData *cd = ffi_tocdata(L, argidx);
  unsigned char chunkbuf[16];
  uint64_t chunks[2] = {0, 0};
  size_t chunks_n = (at->size + 7) / 8;
  unsigned char code[64];
  size_t n = 0, i;
  void *mem;
  size_t pagesize = (size_t)sysconf(_SC_PAGESIZE);
  ffi_type *atypes[2];
  ffi_type *rtype;
  ffi_cif cif;
  void *values[2];
  union { int64_t i; double d; void *p; unsigned char b[16]; } ret;
  FTAlloc *fta = NULL;
  ffi_status st;
  int saved_errno;
  if (cd == NULL || (cd->flags & CDATA_CTYPE) != 0 || cd->ct != at)
    luaL_error(L, "ffi: argument %d: cannot convert %s to '%s'",
               argidx, luaL_typename(L, argidx), ffi_type_repr(S, at));
  {
    void *src = ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0)
                    ? cd->v.p : (void *)&cd->v;
    memcpy(chunkbuf, src, at->size);
  }
  memcpy(chunks, chunkbuf, at->size);
  /* sub rsp,24; mov [rsp],rdi; mov [rsp+8],rsi; mov rax,target; call rax;
     add rsp,24; ret */
  code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xec; code[n++] = 0x18;
  code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0x3c; code[n++] = 0x24;
  code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0x74; code[n++] = 0x24;
  code[n++] = 0x08;
  code[n++] = 0x48; code[n++] = 0xb8;
  {
    uint64_t target = (uint64_t)(uintptr_t)fn->v.p;
    for (i = 0; i < 8; i++) code[n++] = (unsigned char)(target >> (8 * i));
  }
  code[n++] = 0xff; code[n++] = 0xd0;
  code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xc4; code[n++] = 0x18;
  code[n++] = 0xc3;
  mem = mmap(NULL, pagesize, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mem == MAP_FAILED)
    luaL_error(L, "ffi: cannot allocate a packed-call thunk");
  memcpy(mem, code, n);
  if (mprotect(mem, pagesize, PROT_READ | PROT_EXEC) != 0) {
    munmap(mem, pagesize);
    luaL_error(L, "ffi: cannot make the packed-call thunk executable");
  }
  for (i = 0; i < chunks_n; i++)
    atypes[i] = &ffi_type_uint64;
  rtype = build_ffi_type(L, S, rt, &fta);
  st = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, (unsigned)chunks_n, rtype,
                    atypes);
  if (st != FFI_OK) {
    munmap(mem, pagesize);
    ft_free_all(fta);
    luaL_error(L, "ffi: libffi cannot prepare a packed call");
  }
  for (i = 0; i < chunks_n; i++)
    values[i] = &chunks[i];
  memset(&ret, 0, sizeof(ret));
  ffi_call(&cif, FFI_FN(mem), &ret, values);
  saved_errno = errno;
  munmap(mem, pagesize);
  ft_free_all(fta);
  errno = saved_errno;
  if (rt->kind == CT_VOID)
    return 0;
  ffi_push_value(L, S, rt, &ret, 0);
  return 1;
}
#endif


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

#if defined(__x86_64__)
  if (!ft->vararg && nargs >= 1 &&
      ft->ret->kind == CT_STRUCT && ft->ret->pack == 1 &&
      ft->ret->align_override == 0 && ft->ret->size > 0 &&
      packed_int_type(ft->ret)) {
    int pidx[5], pc = 0, ms = 0, mint = 0, mchunks = 0, mok = 1, j;
    for (j = 0; j < nargs; j++) {
      CType *at = ft->args[j];
      if (at->kind == CT_STRUCT && at->pack == 1 &&
          at->align_override == 0 && at->size > 0 && at->size <= 16 &&
          packed_int_type(at)) {
        if (pc >= 5) { mok = 0; break; }
        pidx[pc++] = j;
        mchunks += (int)((at->size + 7) / 8);
      }
      else if (packed_scalar_arg(at)) {
        ms++;
        if (at->kind != CT_FLOAT)
          mint++;
      }
      else
        mok = 0;
    }
    if (mok && pc >= 1 && ms <= 5 && mchunks <= 6 &&
        mchunks + mint <= 6)
      return packed_mixed_call(L, S, fn, ft, ft->ret, argstart, pidx, pc);
  }
  if (!ft->vararg && nargs >= 1 && nargs <= 3 &&
      ft->ret->kind == CT_STRUCT && ft->ret->pack == 1 &&
      ft->ret->align_override == 0 && ft->ret->size > 0 &&
      packed_int_type(ft->ret)) {
    int ok = 1, j, total = 0;
    for (j = 0; j < nargs; j++) {
      CType *at = ft->args[j];
      if (at->kind != CT_STRUCT || at->pack != 1 ||
          at->align_override != 0 || at->size == 0 || at->size > 16 ||
          !packed_int_type(at)) { ok = 0; break; }
      total += (int)((at->size + 7) / 8);
    }
    if (ok && total <= 6)
      return packed_multi_call(L, S, fn, ft, ft->ret, argstart);
  }
  if (!ft->vararg &&
      ft->ret->kind == CT_STRUCT && ft->ret->pack == 1 &&
      ft->ret->align_override == 0 && ft->ret->size > 0 &&
      packed_int_type(ft->ret) &&
      packed_return_signature(ft))
    return packed_return_call(L, S, fn, ft, ft->ret, argstart);
  if (!ft->vararg && nargs == 1 && provided == 1) {
    CType *at = ft->args[0];
    CType *rt0 = ft->ret;
    if (at->kind == CT_STRUCT && at->pack == 1 &&
        at->align_override == 0 && at->size > 0 && at->size <= 16 &&
        packed_int_type(at) &&
        (rt0->kind == CT_INT || rt0->kind == CT_BOOL ||
         rt0->kind == CT_ENUM || rt0->kind == CT_FLOAT ||
         rt0->kind == CT_POINTER || rt0->kind == CT_FUNC ||
         rt0->kind == CT_VOID))
      return packed_single_call(L, S, fn, at, rt0, argstart);
  }
#endif

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


static int packed_callback_mixed_candidate (CType *ft) {
  CType *rt = ft->ret;
  int i, scalars = 0, packed = 0;
  if (ft->nargs < 2 || ft->nargs > 4)
    return 0;
  for (i = 0; i < (int)ft->nargs; i++) {
    CType *at = ft->args[i];
    if (at->kind == CT_STRUCT && at->pack == 1 &&
        at->align_override == 0 && at->size > 0 && at->size <= 8 &&
        packed_int_type(at))
      packed++;
    else if (((at->kind == CT_INT || at->kind == CT_BOOL ||
               at->kind == CT_ENUM) && at->size > 0 && at->size <= 8) ||
             ((at->kind == CT_FLOAT) && (at->size == 4 || at->size == 8)))
      scalars++;
    else
      return 0;
  }
  if (scalars != 1 || packed < 1 || packed > 3)
    return 0;
  if (rt->kind == CT_STRUCT && rt->pack == 1 && rt->align_override == 0 &&
      rt->size > 0 && rt->size <= 16 && packed_int_type(rt))
    return 1;
  switch (rt->kind) {
    case CT_VOID: case CT_INT: case CT_BOOL: case CT_ENUM:
    case CT_FLOAT: case CT_POINTER: case CT_FUNC:
      return 1;
    default:
      return 0;
  }
}


static int64_t packed_callback_scalar_value (CType *ct, const void *p) {
  const unsigned char *b = (const unsigned char *)p;
  uint64_t u = 0;
  size_t i;
  for (i = 0; i < ct->size && i < 8; i++)
    u |= (uint64_t)b[i] << (8 * i);
  if (ct->is_unsigned || ct->kind == CT_BOOL || ct->kind == CT_ENUM)
    return (int64_t)u;
  if (ct->size < 8 && (u & ((uint64_t)1 << (ct->size * 8 - 1))) != 0)
    u |= ~(((uint64_t)1 << (ct->size * 8)) - 1);
  return (int64_t)u;
}


static uint64_t packed_callback_scalar_ret (FCallback *cb, CType *rt) {
  lua_State *L = cb->L;
  int ok = 1;
  uint64_t out = 0;
  switch (rt->kind) {
    case CT_INT:
    case CT_BOOL:
    case CT_ENUM:
      out = (uint64_t)ffi_to_int(L, cb->S, -1, &ok);
      if (!ok) { out = 0; callback_set_error(L, "packed callback return"); }
      break;
    case CT_FLOAT: {
      double d = ffi_to_double(L, cb->S, -1, &ok);
      if (!ok) { d = 0; callback_set_error(L, "packed callback return"); }
      if (rt->size == 4) {
        float f = (float)d;
        memcpy(&out, &f, sizeof(f));
      }
      else
        memcpy(&out, &d, sizeof(d));
      break;
    }
    case CT_POINTER:
    case CT_FUNC:
      out = (uint64_t)(uintptr_t)ffi_to_ptr(L, cb->S, -1, &ok);
      if (!ok) { out = 0; callback_set_error(L, "packed callback return"); }
      break;
    default:
      break;
  }
  return out;
}


static uint64_t packed_callback_mixed_scalar (FCallback *cb, const void *buf) {
  lua_State *L = cb->L;
  CType *rt = cb->ft->ret;
  int top = lua_gettop(L);
  uint64_t out;
  if (!lua_checkstack(L, 5)) {
    callback_set_error(L, "packed callback stack overflow");
    return 0;
  }
  lua_rawgeti(L, LUA_REGISTRYINDEX, cb->fnref);
  {
    const char *p = (const char *)buf + 8;
    int ai;
    for (ai = 0; ai < (int)cb->ft->nargs; ai++) {
      CType *a = cb->ft->args[ai];
      if (a->kind == CT_STRUCT) {
        CData *cd = ffi_push_owned(L, cb->S, a, a->size);
        memcpy(cd->v.p, p, a->size);
        p += 8;
      }
      else if (a->kind == CT_FLOAT) {
        if (a->size == 4) {
          float f;
          memcpy(&f, buf, sizeof(f));
          lua_pushnumber(L, (lua_Number)f);
        }
        else {
          double d;
          memcpy(&d, buf, sizeof(d));
          lua_pushnumber(L, (lua_Number)d);
        }
      }
      else
        lua_pushinteger(L, (lua_Integer)packed_callback_scalar_value(a, buf));
    }
  }
  if (lua_pcall(L, (int)cb->ft->nargs,
                rt->kind == CT_VOID ? 0 : 1, 0) != LUA_OK) {
    callback_set_error(L, lua_tostring(L, -1));
    lua_settop(L, top);
    return 0;
  }
  out = packed_callback_scalar_ret(cb, rt);
  lua_settop(L, top);
  return out;
}


static void packed_callback_mixed_ret (FCallback *cb, const void *buf,
                                       void *ret) {
  lua_State *L = cb->L;
  CType *rt = cb->ft->ret;
  int top = lua_gettop(L);
  if (!lua_checkstack(L, 5)) {
    memset(ret, 0, rt->size);
    callback_set_error(L, "packed callback stack overflow");
    return;
  }
  lua_rawgeti(L, LUA_REGISTRYINDEX, cb->fnref);
  {
    const char *p = (const char *)buf + 8;
    int ai;
    for (ai = 0; ai < (int)cb->ft->nargs; ai++) {
      CType *a = cb->ft->args[ai];
      if (a->kind == CT_STRUCT) {
        CData *cd = ffi_push_owned(L, cb->S, a, a->size);
        memcpy(cd->v.p, p, a->size);
        p += 8;
      }
      else if (a->kind == CT_FLOAT) {
        if (a->size == 4) {
          float f;
          memcpy(&f, buf, sizeof(f));
          lua_pushnumber(L, (lua_Number)f);
        }
        else {
          double d;
          memcpy(&d, buf, sizeof(d));
          lua_pushnumber(L, (lua_Number)d);
        }
      }
      else
        lua_pushinteger(L, (lua_Integer)packed_callback_scalar_value(a, buf));
    }
  }
  if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
    memset(ret, 0, rt->size);
    callback_set_error(L, lua_tostring(L, -1));
    lua_settop(L, top);
    return;
  }
  {
    CData *cd = ffi_tocdata(L, -1);
    if (cd != NULL && (cd->flags & CDATA_CTYPE) == 0 &&
        cd->ct->kind == CT_STRUCT && cd->ct->size == rt->size) {
      memcpy(ret, cd->v.p, rt->size);
    }
    else {
      memset(ret, 0, rt->size);
      callback_set_error(L, "packed callback returned an incompatible value");
    }
  }
  lua_settop(L, top);
}


static void *packed_callback_mixed_thunk (FCallback *cb, int packed_ret) {
  unsigned char code[192];
  size_t n = 0, i;
  int packed_n = 0;
  void *mem;
  size_t pagesize = (size_t)sysconf(_SC_PAGESIZE);
  for (i = 0; i < (size_t)cb->ft->nargs; i++)
    if (cb->ft->args[i]->kind == CT_STRUCT)
      packed_n++;
  if (!packed_ret) {
    /* sub rsp,72; mov [rsp],rdi; lea r10,[rsp+80]; mov rax,[r10];
       mov [rsp+8],rax; mov rdi,cb; mov rsi,rsp; mov rax,dispatcher;
       call rax; add rsp,72; [movq xmm0,rax]; ret */
    code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xec; code[n++] = 0x48;
    {
      CType *sc = NULL;
      int ai;
      for (ai = 0; ai < (int)cb->ft->nargs; ai++)
        if (cb->ft->args[ai]->kind != CT_STRUCT) sc = cb->ft->args[ai];
      if (sc != NULL && sc->kind == CT_FLOAT) {
        if (sc->size == 4) {
          code[n++] = 0xf3; code[n++] = 0x0f; code[n++] = 0x11;
          code[n++] = 0x04; code[n++] = 0x24;
        }
        else {
          code[n++] = 0xf2; code[n++] = 0x0f; code[n++] = 0x11;
          code[n++] = 0x04; code[n++] = 0x24;
        }
      }
      else {
        code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0x3c; code[n++] = 0x24;
      }
    }
    code[n++] = 0x4c; code[n++] = 0x8d; code[n++] = 0x54; code[n++] = 0x24;
    code[n++] = 0x50;
    for (i = 0; i < (size_t)packed_n; i++) {
      int disp = (int)(8 * i);
      if (disp == 0) {
        code[n++] = 0x49; code[n++] = 0x8b; code[n++] = 0x02;
      }
      else {
        code[n++] = 0x49; code[n++] = 0x8b; code[n++] = 0x42;
        code[n++] = (unsigned char)disp;
      }
      code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0x44; code[n++] = 0x24;
      code[n++] = (unsigned char)(0x08 + disp);
    }
    code[n++] = 0x48; code[n++] = 0xbf;
    {
      uint64_t p = (uint64_t)(uintptr_t)cb;
      for (i = 0; i < 8; i++) code[n++] = (unsigned char)(p >> (8 * i));
    }
    code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0xe6;
    code[n++] = 0x48; code[n++] = 0xb8;
    {
      uint64_t p = (uint64_t)(uintptr_t)&packed_callback_mixed_scalar;
      for (i = 0; i < 8; i++) code[n++] = (unsigned char)(p >> (8 * i));
    }
    code[n++] = 0xff; code[n++] = 0xd0;
    code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xc4; code[n++] = 0x48;
    if (cb->ft->ret->kind == CT_FLOAT) {
      code[n++] = 0x66; code[n++] = 0x48; code[n++] = 0x0f;
      code[n++] = 0x6e; code[n++] = 0xc0;
    }
    code[n++] = 0xc3;
  }
  else {
    /* sub rsp,88; mov [rsp+32],rdi; mov [rsp],rsi; lea r10,[rsp+96];
       mov rax,[r10]; mov [rsp+8],rax; lea rdx,[rsp+40];
       mov rdi,cb; mov rsi,rsp; call dispatcher; mov r8,[rsp+32];
       copy ret bytes [rsp+40]; mov rax,r8; add rsp,88; ret */
    code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xec; code[n++] = 0x58;
    code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0x7c; code[n++] = 0x24;
    code[n++] = 0x20;
    {
      CType *sc = NULL;
      int ai;
      for (ai = 0; ai < (int)cb->ft->nargs; ai++)
        if (cb->ft->args[ai]->kind != CT_STRUCT) sc = cb->ft->args[ai];
      if (sc != NULL && sc->kind == CT_FLOAT) {
        if (sc->size == 4) {
          code[n++] = 0xf3; code[n++] = 0x0f; code[n++] = 0x11;
          code[n++] = 0x04; code[n++] = 0x24;
        }
        else {
          code[n++] = 0xf2; code[n++] = 0x0f; code[n++] = 0x11;
          code[n++] = 0x04; code[n++] = 0x24;
        }
      }
      else {
        code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0x34; code[n++] = 0x24;
      }
    }
    code[n++] = 0x4c; code[n++] = 0x8d; code[n++] = 0x54; code[n++] = 0x24;
    code[n++] = 0x60;
    for (i = 0; i < (size_t)packed_n; i++) {
      int disp = (int)(8 * i);
      if (disp == 0) {
        code[n++] = 0x49; code[n++] = 0x8b; code[n++] = 0x02;
      }
      else {
        code[n++] = 0x49; code[n++] = 0x8b; code[n++] = 0x42;
        code[n++] = (unsigned char)disp;
      }
      code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0x44; code[n++] = 0x24;
      code[n++] = (unsigned char)(0x08 + disp);
    }
    code[n++] = 0x48; code[n++] = 0x8d; code[n++] = 0x54; code[n++] = 0x24;
    code[n++] = 0x28;
    code[n++] = 0x48; code[n++] = 0xbf;
    {
      uint64_t p = (uint64_t)(uintptr_t)cb;
      for (i = 0; i < 8; i++) code[n++] = (unsigned char)(p >> (8 * i));
    }
    code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0xe6;
    code[n++] = 0x48; code[n++] = 0xb8;
    {
      uint64_t p = (uint64_t)(uintptr_t)&packed_callback_mixed_ret;
      for (i = 0; i < 8; i++) code[n++] = (unsigned char)(p >> (8 * i));
    }
    code[n++] = 0xff; code[n++] = 0xd0;
    code[n++] = 0x4c; code[n++] = 0x8b; code[n++] = 0x44; code[n++] = 0x24;
    code[n++] = 0x20;
    for (i = 0; i < cb->ft->ret->size; i++) {
      code[n++] = 0x8a; code[n++] = 0x44; code[n++] = 0x24;
      code[n++] = (unsigned char)(0x28 + i);
      code[n++] = 0x41; code[n++] = 0x88; code[n++] = 0x40;
      code[n++] = (unsigned char)i;
    }
    code[n++] = 0x4c; code[n++] = 0x89; code[n++] = 0xc0;
    code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xc4; code[n++] = 0x58;
    code[n++] = 0xc3;
  }
  mem = mmap(NULL, pagesize, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mem == MAP_FAILED)
    return NULL;
  memcpy(mem, code, n);
  if (mprotect(mem, pagesize, PROT_READ | PROT_EXEC) != 0) {
    munmap(mem, pagesize);
    return NULL;
  }
  return mem;
}


static int ffi_callback_new_packed_mixed (lua_State *L, FFIState *S, CType *ct,
                                          CType *ft, int funcidx) {
  FCallback *cb = (FCallback *)calloc(1, sizeof(FCallback));
  CData *cd;
  if (cb == NULL)
    return luaL_error(L, "ffi.callback: out of memory");
  cb->L = L;
  cb->S = S;
  cb->ft = ft;
  cb->fnref = LUA_NOREF;
  cb->code = packed_callback_mixed_thunk(cb, ft->ret->kind == CT_STRUCT);
  if (cb->code == NULL) {
    free(cb);
    return luaL_error(L, "ffi.callback: cannot allocate a packed thunk");
  }
  lua_pushvalue(L, funcidx);
  cb->fnref = luaL_ref(L, LUA_REGISTRYINDEX);
  cd = ffi_push_cdata_object(L, S, ct, CDATA_FUNC | CDATA_CALLBACK, 0);
  cd->v.p = cb->code;
  cd->aux = cb;
  return 1;
}


static int packed_callback_candidate (CType *ft) {
  CType *rt = ft->ret;
  int i, rt_ok;
  if (ft->nargs < 1 || ft->nargs > 3)
    return 0;
  for (i = 0; i < (int)ft->nargs; i++) {
    CType *at = ft->args[i];
    if (at->kind != CT_STRUCT || at->pack != 1 ||
        at->align_override != 0 || at->size == 0 || at->size > 16 ||
        !packed_int_type(at))
      return 0;
  }
  rt_ok = (rt->kind == CT_STRUCT && rt->pack == 1 &&
           rt->align_override == 0 && rt->size > 0 && rt->size <= 16 &&
           packed_int_type(rt));
  if (rt_ok)
    return 1;
  switch (rt->kind) {
    case CT_VOID: case CT_INT: case CT_BOOL: case CT_ENUM:
    case CT_FLOAT: case CT_POINTER: case CT_FUNC:
      return 1;
    default:
      return 0;
  }
}


static uint64_t packed_callback_scalar (FCallback *cb, const void *arg) {
  lua_State *L = cb->L;
  CType *rt = cb->ft->ret;
  int top = lua_gettop(L);
  uint64_t out = 0;
  int ok = 1;
  if (!lua_checkstack(L, (int)cb->ft->nargs + 4)) {
    callback_set_error(L, "packed callback stack overflow");
    return 0;
  }
  lua_rawgeti(L, LUA_REGISTRYINDEX, cb->fnref);
  {
    const char *p = (const char *)arg;
    int ai;
    for (ai = 0; ai < (int)cb->ft->nargs; ai++) {
      CType *a = cb->ft->args[ai];
      CData *cd = ffi_push_owned(L, cb->S, a, a->size);
      memcpy(cd->v.p, p, a->size);
      p += (a->size + 7) & ~(size_t)7;
    }
  }
  if (lua_pcall(L, (int)cb->ft->nargs,
                rt->kind == CT_VOID ? 0 : 1, 0) != LUA_OK) {
    callback_set_error(L, lua_tostring(L, -1));
    lua_settop(L, top);
    return 0;
  }
  switch (rt->kind) {
    case CT_INT:
    case CT_BOOL:
    case CT_ENUM:
      out = (uint64_t)ffi_to_int(L, cb->S, -1, &ok);
      if (!ok) { out = 0; callback_set_error(L, "packed callback return"); }
      break;
    case CT_FLOAT: {
      double d = ffi_to_double(L, cb->S, -1, &ok);
      if (!ok) { d = 0; callback_set_error(L, "packed callback return"); }
      if (rt->size == 4) {
        float f = (float)d;
        memcpy(&out, &f, sizeof(f));
      }
      else
        memcpy(&out, &d, sizeof(d));
      break;
    }
    case CT_POINTER:
    case CT_FUNC:
      out = (uint64_t)(uintptr_t)ffi_to_ptr(L, cb->S, -1, &ok);
      if (!ok) { out = 0; callback_set_error(L, "packed callback return"); }
      break;
    default:
      break;
  }
  lua_settop(L, top);
  return out;
}


static void packed_callback_ret (FCallback *cb, const void *arg, void *ret) {
  lua_State *L = cb->L;
  CType *rt = cb->ft->ret;
  int top = lua_gettop(L);
  if (!lua_checkstack(L, (int)cb->ft->nargs + 4)) {
    memset(ret, 0, rt->size);
    callback_set_error(L, "packed callback stack overflow");
    return;
  }
  lua_rawgeti(L, LUA_REGISTRYINDEX, cb->fnref);
  {
    const char *p = (const char *)arg;
    int ai;
    for (ai = 0; ai < (int)cb->ft->nargs; ai++) {
      CType *a = cb->ft->args[ai];
      CData *cd = ffi_push_owned(L, cb->S, a, a->size);
      memcpy(cd->v.p, p, a->size);
      p += (a->size + 7) & ~(size_t)7;
    }
  }
  if (lua_pcall(L, (int)cb->ft->nargs, 1, 0) != LUA_OK) {
    memset(ret, 0, rt->size);
    callback_set_error(L, lua_tostring(L, -1));
    lua_settop(L, top);
    return;
  }
  {
    CData *cd = ffi_tocdata(L, -1);
    if (cd != NULL && (cd->flags & CDATA_CTYPE) == 0 &&
        cd->ct->kind == CT_STRUCT && cd->ct->size == rt->size) {
      memcpy(ret, cd->v.p, rt->size);
    }
    else {
      memset(ret, 0, rt->size);
      callback_set_error(L, "packed callback returned an incompatible value");
    }
  }
  lua_settop(L, top);
}


static void *packed_callback_thunk (FCallback *cb, int packed_ret) {
  unsigned char code[256];
  size_t n = 0, i;
  void *mem;
  size_t pagesize = (size_t)sysconf(_SC_PAGESIZE);
  if (!packed_ret) {
    /* sub rsp,40; lea rsi,[rsp+48]; mov rdi,cb; mov rax,dispatcher;
       call rax; add rsp,40; [movq xmm0,rax]; ret */
    code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xec; code[n++] = 0x28;
    code[n++] = 0x48; code[n++] = 0x8d; code[n++] = 0x74; code[n++] = 0x24;
    code[n++] = 0x30;
    code[n++] = 0x48; code[n++] = 0xbf;
    {
      uint64_t p = (uint64_t)(uintptr_t)cb;
      for (i = 0; i < 8; i++) code[n++] = (unsigned char)(p >> (8 * i));
    }
    code[n++] = 0x48; code[n++] = 0xb8;
    {
      uint64_t p = (uint64_t)(uintptr_t)&packed_callback_scalar;
      for (i = 0; i < 8; i++) code[n++] = (unsigned char)(p >> (8 * i));
    }
    code[n++] = 0xff; code[n++] = 0xd0;
    code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xc4; code[n++] = 0x28;
    if (cb->ft->ret->kind == CT_FLOAT) {
      code[n++] = 0x66; code[n++] = 0x48; code[n++] = 0x0f;
      code[n++] = 0x6e; code[n++] = 0xc0; /* movq xmm0,rax */
    }
    code[n++] = 0xc3;
  }
  else {
    /* sub rsp,56; mov [rsp+32],rdi; lea rsi,[rsp+64]; mov rdx,rsp;
       mov rdi,cb; mov rax,dispatcher; call rax; mov r8,[rsp+32];
       copy bytes; mov rax,r8; add rsp,56; ret */
    code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xec; code[n++] = 0x38;
    code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0x7c; code[n++] = 0x24;
    code[n++] = 0x20;
    code[n++] = 0x48; code[n++] = 0x8d; code[n++] = 0x74; code[n++] = 0x24;
    code[n++] = 0x40;
    code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0xe2;
    code[n++] = 0x48; code[n++] = 0xbf;
    {
      uint64_t p = (uint64_t)(uintptr_t)cb;
      for (i = 0; i < 8; i++) code[n++] = (unsigned char)(p >> (8 * i));
    }
    code[n++] = 0x48; code[n++] = 0xb8;
    {
      uint64_t p = (uint64_t)(uintptr_t)&packed_callback_ret;
      for (i = 0; i < 8; i++) code[n++] = (unsigned char)(p >> (8 * i));
    }
    code[n++] = 0xff; code[n++] = 0xd0;
    code[n++] = 0x4c; code[n++] = 0x8b; code[n++] = 0x44; code[n++] = 0x24;
    code[n++] = 0x20;
    for (i = 0; i < cb->ft->ret->size; i++) {
      code[n++] = 0x8a; code[n++] = 0x44; code[n++] = 0x24;
      code[n++] = (unsigned char)i;
      code[n++] = 0x41; code[n++] = 0x88; code[n++] = 0x40;
      code[n++] = (unsigned char)i;
    }
    code[n++] = 0x4c; code[n++] = 0x89; code[n++] = 0xc0;
    code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xc4; code[n++] = 0x38;
    code[n++] = 0xc3;
  }
  mem = mmap(NULL, pagesize, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mem == MAP_FAILED)
    return NULL;
  memcpy(mem, code, n);
  if (mprotect(mem, pagesize, PROT_READ | PROT_EXEC) != 0) {
    munmap(mem, pagesize);
    return NULL;
  }
  return mem;
}


static int ffi_callback_new_packed (lua_State *L, FFIState *S, CType *ct,
                                    CType *ft, int funcidx) {
  FCallback *cb = (FCallback *)calloc(1, sizeof(FCallback));
  CData *cd;
  if (cb == NULL)
    return luaL_error(L, "ffi.callback: out of memory");
  cb->L = L;
  cb->S = S;
  cb->ft = ft;
  cb->fnref = LUA_NOREF;
  cb->code = packed_callback_thunk(cb, ft->ret->kind == CT_STRUCT);
  if (cb->code == NULL) {
    free(cb);
    return luaL_error(L, "ffi.callback: cannot allocate a packed thunk");
  }
  lua_pushvalue(L, funcidx);
  cb->fnref = luaL_ref(L, LUA_REGISTRYINDEX);
  cd = ffi_push_cdata_object(L, S, ct, CDATA_FUNC | CDATA_CALLBACK, 0);
  cd->v.p = cb->code;
  cd->aux = cb;
  return 1;
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
#if defined(__x86_64__)
  if (packed_callback_mixed_candidate(ft))
    return ffi_callback_new_packed_mixed(L, S, ct, ft, funcidx);
  if (packed_callback_candidate(ft))
    return ffi_callback_new_packed(L, S, ct, ft, funcidx);
#else
  /* The packed trampolines emit x86-64 machine code; other hosts use the
  ** generic libffi closure path below. */
#endif
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


int ffi_callback_free_mt (lua_State *L) {
  CData *cd = ffi_checkcdata(L, 1, "free");
  if ((cd->flags & CDATA_CALLBACK) == 0 || cd->aux == NULL)
    return luaL_error(L, "ffi: callback is already freed");
  ffi_callback_free(L, cd);
  cd->v.p = NULL;
  return 0;
}


int ffi_callback_set_mt (lua_State *L) {
  CData *cd = ffi_checkcdata(L, 1, "set");
  FCallback *cb;
  if ((cd->flags & CDATA_CALLBACK) == 0 || cd->aux == NULL)
    return luaL_error(L, "ffi: callback is already freed");
  luaL_checktype(L, 2, LUA_TFUNCTION);
  cb = (FCallback *)cd->aux;
  luaL_unref(L, LUA_REGISTRYINDEX, cb->fnref);
  lua_pushvalue(L, 2);
  cb->fnref = luaL_ref(L, LUA_REGISTRYINDEX);
  return 0;
}


void ffi_callback_free (lua_State *L, CData *cd) {
  FCallback *cb = (FCallback *)cd->aux;
  (void)L;
  if (cb == NULL)
    return;
  cd->aux = NULL;
  if (cb->closure != NULL)
    ffi_closure_free(cb->closure);
  else if (cb->code != NULL)
    munmap(cb->code, (size_t)sysconf(_SC_PAGESIZE));
  ft_free_all(cb->allocs);
  free(cb->args);
  if (cb->fnref != LUA_NOREF)
    luaL_unref(cb->L, LUA_REGISTRYINDEX, cb->fnref);
  free(cb);
}


/* J4 helper: call a zero-argument C function with a scalar return.
** Uses libffi directly; no Lua stack, allocation or metamethods. */
int ffi_trace_call0 (CData *fn, int *kind, int64_t *iv, double *dv) {
  CType *ft, *rt;
  ffi_cif cif;
  ffi_type *rtype;
  union { int64_t i; double d; } ret;
  if (fn == NULL || kind == NULL || iv == NULL || dv == NULL ||
      fn->magic != LUAR_FFI_MAGIC ||
      (fn->flags & (CDATA_CTYPE | CDATA_CALLBACK |
                    CDATA_OWNED | CDATA_REF)) != 0 ||
      fn->v.p == NULL || fn->ct == NULL)
    return 0;
  ft = (fn->ct->kind == CT_FUNC) ? fn->ct : fn->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 0 || ft->vararg)
    return 0;
  rt = ft->ret;
  if (rt == NULL)
    return 0;
  memset(&ret, 0, sizeof(ret));
  if (rt->kind == CT_INT) {
    switch (rt->size) {
      case 1: rtype = &ffi_type_sint8; break;
      case 2: rtype = &ffi_type_sint16; break;
      case 4: rtype = &ffi_type_sint32; break;
      case 8: rtype = &ffi_type_sint64; break;
      default: return 0;
    }
    *kind = 1;
  }
  else if (rt->kind == CT_FLOAT) {
    if (rt->size == 4) rtype = &ffi_type_float;
    else if (rt->size == 8) rtype = &ffi_type_double;
    else return 0;
    *kind = 2;
  }
  else
    return 0;
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 0, rtype, NULL) != FFI_OK)
    return 0;
  ffi_call(&cif, FFI_FN(fn->v.p), &ret, NULL);
  if (*kind == 1) *iv = ret.i;
  else *dv = ret.d;
  return 1;
}


/* J4 helper: call a one-integer-argument C function with an integer return.
** Uses libffi directly; no Lua stack, allocation or metamethods.  libffi
** types are chosen from the declared CType sizes and the raw result is
** extended according to the declared signedness. */
int ffi_trace_call1i (CData *fn, int64_t arg, int64_t *out) {
  CType *ft, *at, *rt;
  ffi_cif cif;
  ffi_type *atype, *rtype;
  ffi_type *atypes[1];
  void *avalue[1];
  union {
    int8_t i8; int16_t i16; int32_t i32; int64_t i64;
    uint8_t u8; uint16_t u16; uint32_t u32; uint64_t u64;
  } av, ret;
  if (fn == NULL || out == NULL || fn->magic != LUAR_FFI_MAGIC ||
      (fn->flags & (CDATA_CTYPE | CDATA_CALLBACK |
                    CDATA_OWNED | CDATA_REF)) != 0 || fn->v.p == NULL)
    return 0;
  if (fn->ct->kind == CT_FUNC)
    ft = fn->ct;
  else if (fn->ct->kind == CT_POINTER && fn->ct->child != NULL &&
           fn->ct->child->kind == CT_FUNC)
    ft = fn->ct->child;
  else
    return 0;
  if (ft->nargs != 1 || ft->vararg)
    return 0;
  at = ft->args[0];
  rt = ft->ret;
  if (at == NULL || rt == NULL || at->kind != CT_INT || rt->kind != CT_INT)
    return 0;
  memset(&av, 0, sizeof(av));
  switch (at->size) {
    case 1: av.i8 = (int8_t)arg; avalue[0] = &av.i8;
            atype = &ffi_type_sint8; break;
    case 2: av.i16 = (int16_t)arg; avalue[0] = &av.i16;
            atype = &ffi_type_sint16; break;
    case 4: av.i32 = (int32_t)arg; avalue[0] = &av.i32;
            atype = &ffi_type_sint32; break;
    case 8: av.i64 = arg; avalue[0] = &av.i64;
            atype = &ffi_type_sint64; break;
    default: return 0;
  }
  switch (rt->size) {
    case 1: rtype = &ffi_type_sint8; break;
    case 2: rtype = &ffi_type_sint16; break;
    case 4: rtype = &ffi_type_sint32; break;
    case 8: rtype = &ffi_type_sint64; break;
    default: return 0;
  }
  atypes[0] = atype;
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, rtype, atypes) != FFI_OK)
    return 0;
  memset(&ret, 0, sizeof(ret));
  ffi_call(&cif, FFI_FN(fn->v.p), &ret, avalue);
  switch (rt->size) {
    case 1:
      *out = rt->is_unsigned ? (int64_t)ret.u8 : (int64_t)ret.i8;
      break;
    case 2:
      *out = rt->is_unsigned ? (int64_t)ret.u16 : (int64_t)ret.i16;
      break;
    case 4:
      *out = rt->is_unsigned ? (int64_t)ret.u32 : (int64_t)ret.i32;
      break;
    default:
      *out = rt->is_unsigned ? (int64_t)ret.u64 : ret.i64;
      break;
  }
  return 1;
}


/* J4 helper: call a C function with one integer argument converted to a
** double parameter, returning a double. */
int ffi_trace_call1d_iarg (CData *fn, int64_t arg, double *out) {
  CType *ft, *at, *rt;
  ffi_cif cif;
  ffi_type *atype = &ffi_type_double, *rtype;
  void *avalue[1];
  double av = (double)arg, ret = 0.0;
  if (fn == NULL || out == NULL || fn->magic != LUAR_FFI_MAGIC ||
      (fn->flags & (CDATA_CTYPE | CDATA_CALLBACK)) != 0 || fn->v.p == NULL)
    return 0;
  ft = (fn->ct->kind == CT_FUNC) ? fn->ct : fn->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 1 || ft->vararg)
    return 0;
  at = ft->args[0];
  rt = ft->ret;
  if (at == NULL || rt == NULL || at->kind != CT_FLOAT ||
      at->size != 8 || rt->kind != CT_FLOAT || rt->size != 8)
    return 0;
  rtype = &ffi_type_double;
  avalue[0] = &av;
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, rtype, &atype) != FFI_OK)
    return 0;
  ffi_call(&cif, FFI_FN(fn->v.p), &ret, avalue);
  *out = ret;
  return 1;
}


/* J4 helper: call a C function with one double argument and a double
** return.  No Lua stack, allocation or metamethods. */
int ffi_trace_call1d (CData *fn, double arg, double *out) {
  CType *ft, *at, *rt;
  ffi_cif cif;
  ffi_type *atype = &ffi_type_double, *rtype = &ffi_type_double;
  void *avalue[1];
  double av = arg, ret = 0.0;
  if (fn == NULL || out == NULL || fn->magic != LUAR_FFI_MAGIC ||
      (fn->flags & (CDATA_CTYPE | CDATA_CALLBACK |
                    CDATA_OWNED | CDATA_REF)) != 0 ||
      fn->v.p == NULL || fn->ct == NULL)
    return 0;
  if (fn->ct->kind == CT_FUNC)
    ft = fn->ct;
  else if (fn->ct->kind == CT_POINTER && fn->ct->child != NULL &&
           fn->ct->child->kind == CT_FUNC)
    ft = fn->ct->child;
  else
    return 0;
  if (ft->nargs != 1 || ft->vararg)
    return 0;
  at = ft->args[0];
  rt = ft->ret;
  if (at == NULL || rt == NULL || at->kind != CT_FLOAT ||
      at->size != 8 || rt->kind != CT_FLOAT || rt->size != 8)
    return 0;
  avalue[0] = &av;
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, rtype, &atype) != FFI_OK)
    return 0;
  ffi_call(&cif, FFI_FN(fn->v.p), &ret, avalue);
  *out = ret;
  return 1;
}


/* J4 helper: call a C function with two double arguments and a double
** return.  No Lua stack, allocation or metamethods. */
int ffi_trace_call2d (CData *fn, double arg1, double arg2, double *out) {
  CType *ft, *at1, *at2, *rt;
  ffi_cif cif;
  ffi_type *atypes[2];
  void *avalue[2];
  double av1 = arg1, av2 = arg2, ret = 0.0;
  if (fn == NULL || out == NULL || fn->magic != LUAR_FFI_MAGIC ||
      (fn->flags & (CDATA_CTYPE | CDATA_CALLBACK |
                    CDATA_OWNED | CDATA_REF)) != 0 ||
      fn->v.p == NULL || fn->ct == NULL)
    return 0;
  if (fn->ct->kind == CT_FUNC)
    ft = fn->ct;
  else if (fn->ct->kind == CT_POINTER && fn->ct->child != NULL &&
           fn->ct->child->kind == CT_FUNC)
    ft = fn->ct->child;
  else
    return 0;
  if (ft->nargs != 2 || ft->vararg)
    return 0;
  at1 = ft->args[0];
  at2 = ft->args[1];
  rt = ft->ret;
  if (at1 == NULL || at2 == NULL || rt == NULL ||
      at1->kind != CT_FLOAT || at1->size != 8 ||
      at2->kind != CT_FLOAT || at2->size != 8 ||
      rt->kind != CT_FLOAT || rt->size != 8)
    return 0;
  atypes[0] = &ffi_type_double;
  atypes[1] = &ffi_type_double;
  avalue[0] = &av1;
  avalue[1] = &av2;
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ffi_type_double,
                   atypes) != FFI_OK)
    return 0;
  ffi_call(&cif, FFI_FN(fn->v.p), &ret, avalue);
  *out = ret;
  return 1;
}


/* J4 helper: call a C function with (double, int) arguments and double
** return. */
int ffi_trace_call2di (CData *fn, double a0, int64_t a1, double *out) {
  CType *ft, *at0, *at1, *rt;
  ffi_cif cif;
  ffi_type *atypes[2], *rtype = &ffi_type_double;
  void *avalue[2];
  double av0 = a0, ret = 0.0;
  int32_t av1;
  if (fn == NULL || out == NULL || fn->magic != LUAR_FFI_MAGIC ||
      (fn->flags & (CDATA_CTYPE | CDATA_CALLBACK)) != 0 || fn->v.p == NULL)
    return 0;
  ft = (fn->ct->kind == CT_FUNC) ? fn->ct : fn->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 2 || ft->vararg)
    return 0;
  at0 = ft->args[0];
  at1 = ft->args[1];
  rt = ft->ret;
  if (at0 == NULL || at1 == NULL || rt == NULL ||
      at0->kind != CT_FLOAT || at0->size != 8 ||
      at1->kind != CT_INT || at1->size != 4 ||
      rt->kind != CT_FLOAT || rt->size != 8)
    return 0;
  atypes[0] = &ffi_type_double;
  atypes[1] = &ffi_type_sint32;
  av1 = (int32_t)a1;
  avalue[0] = &av0;
  avalue[1] = &av1;
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, rtype, atypes) != FFI_OK)
    return 0;
  ffi_call(&cif, FFI_FN(fn->v.p), &ret, avalue);
  *out = ret;
  return 1;
}


int ffi_trace_call2id (CData *fn, int64_t a0, double a1, double *out) {
  CType *ft, *at0, *at1, *rt;
  ffi_cif cif;
  ffi_type *atypes[2], *rtype = &ffi_type_double;
  void *avalue[2];
  int32_t av0;
  double av1 = a1, ret = 0.0;
  if (fn == NULL || out == NULL || fn->magic != LUAR_FFI_MAGIC ||
      (fn->flags & (CDATA_CTYPE | CDATA_CALLBACK)) != 0 || fn->v.p == NULL)
    return 0;
  ft = (fn->ct->kind == CT_FUNC) ? fn->ct : fn->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 2 || ft->vararg)
    return 0;
  at0 = ft->args[0]; at1 = ft->args[1]; rt = ft->ret;
  if (at0 == NULL || at1 == NULL || rt == NULL ||
      at0->kind != CT_INT || at0->size != 4 ||
      at1->kind != CT_FLOAT || at1->size != 8 ||
      rt->kind != CT_FLOAT || rt->size != 8)
    return 0;
  atypes[0] = &ffi_type_sint32; atypes[1] = &ffi_type_double;
  av0 = (int32_t)a0; avalue[0] = &av0; avalue[1] = &av1;
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, rtype, atypes) != FFI_OK)
    return 0;
  ffi_call(&cif, FFI_FN(fn->v.p), &ret, avalue);
  *out = ret;
  return 1;
}


int ffi_trace_call2fi (CData *fn, double a0, int64_t a1, double *out) {
  CType *ft, *at0, *at1, *rt;
  ffi_cif cif;
  ffi_type *atypes[2], *rtype = &ffi_type_float;
  void *avalue[2];
  float av0 = (float)a0, ret = 0.0f;
  int32_t av1;
  if (fn == NULL || out == NULL || fn->magic != LUAR_FFI_MAGIC ||
      (fn->flags & (CDATA_CTYPE | CDATA_CALLBACK)) != 0 || fn->v.p == NULL)
    return 0;
  ft = (fn->ct->kind == CT_FUNC) ? fn->ct : fn->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 2 || ft->vararg)
    return 0;
  at0 = ft->args[0]; at1 = ft->args[1]; rt = ft->ret;
  if (at0 == NULL || at1 == NULL || rt == NULL ||
      at0->kind != CT_FLOAT || at0->size != 4 ||
      at1->kind != CT_INT || at1->size != 4 ||
      rt->kind != CT_FLOAT || rt->size != 4)
    return 0;
  atypes[0] = &ffi_type_float; atypes[1] = &ffi_type_sint32;
  av1 = (int32_t)a1;
  avalue[0] = &av0; avalue[1] = &av1;
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, rtype, atypes) != FFI_OK)
    return 0;
  ffi_call(&cif, FFI_FN(fn->v.p), &ret, avalue);
  *out = (double)ret;
  return 1;
}


int ffi_trace_call2if (CData *fn, int64_t a0, double a1, double *out) {
  CType *ft, *at0, *at1, *rt;
  ffi_cif cif;
  ffi_type *atypes[2], *rtype = &ffi_type_float;
  void *avalue[2];
  int32_t av0;
  float av1, ret = 0.0f;
  if (fn == NULL || out == NULL || fn->magic != LUAR_FFI_MAGIC ||
      (fn->flags & (CDATA_CTYPE | CDATA_CALLBACK)) != 0 || fn->v.p == NULL)
    return 0;
  ft = (fn->ct->kind == CT_FUNC) ? fn->ct : fn->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 2 || ft->vararg)
    return 0;
  at0 = ft->args[0]; at1 = ft->args[1]; rt = ft->ret;
  if (at0 == NULL || at1 == NULL || rt == NULL ||
      at0->kind != CT_INT || at0->size != 4 ||
      at1->kind != CT_FLOAT || at1->size != 4 ||
      rt->kind != CT_FLOAT || rt->size != 4)
    return 0;
  atypes[0] = &ffi_type_sint32; atypes[1] = &ffi_type_float;
  av0 = (int32_t)a0; av1 = (float)a1;
  avalue[0] = &av0; avalue[1] = &av1;
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, rtype, atypes) != FFI_OK)
    return 0;
  ffi_call(&cif, FFI_FN(fn->v.p), &ret, avalue);
  *out = (double)ret;
  return 1;
}


int ffi_trace_call2fd (CData *fn, double a0, double a1, double *out) {
  CType *ft, *at0, *at1, *rt;
  ffi_cif cif;
  ffi_type *atypes[2], *rtype = &ffi_type_double;
  void *avalue[2];
  float av0; double av1 = a1, ret = 0.0;
  if (fn == NULL || out == NULL || fn->magic != LUAR_FFI_MAGIC ||
      (fn->flags & (CDATA_CTYPE | CDATA_CALLBACK)) != 0 || fn->v.p == NULL)
    return 0;
  ft = (fn->ct->kind == CT_FUNC) ? fn->ct : fn->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 2 || ft->vararg)
    return 0;
  at0 = ft->args[0]; at1 = ft->args[1]; rt = ft->ret;
  if (at0 == NULL || at1 == NULL || rt == NULL ||
      at0->kind != CT_FLOAT || at0->size != 4 ||
      at1->kind != CT_FLOAT || at1->size != 8 ||
      rt->kind != CT_FLOAT || rt->size != 8)
    return 0;
  atypes[0] = &ffi_type_float; atypes[1] = &ffi_type_double;
  av0 = (float)a0; avalue[0] = &av0; avalue[1] = &av1;
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, rtype, atypes) != FFI_OK)
    return 0;
  ffi_call(&cif, FFI_FN(fn->v.p), &ret, avalue);
  *out = ret;
  return 1;
}


int ffi_trace_call2df (CData *fn, double a0, double a1, double *out) {
  CType *ft, *at0, *at1, *rt;
  ffi_cif cif;
  ffi_type *atypes[2], *rtype = &ffi_type_double;
  void *avalue[2];
  double av0 = a0, ret = 0.0;
  float av1;
  if (fn == NULL || out == NULL || fn->magic != LUAR_FFI_MAGIC ||
      (fn->flags & (CDATA_CTYPE | CDATA_CALLBACK)) != 0 || fn->v.p == NULL)
    return 0;
  ft = (fn->ct->kind == CT_FUNC) ? fn->ct : fn->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 2 || ft->vararg)
    return 0;
  at0 = ft->args[0]; at1 = ft->args[1]; rt = ft->ret;
  if (at0 == NULL || at1 == NULL || rt == NULL ||
      at0->kind != CT_FLOAT || at0->size != 8 ||
      at1->kind != CT_FLOAT || at1->size != 4 ||
      rt->kind != CT_FLOAT || rt->size != 8)
    return 0;
  atypes[0] = &ffi_type_double; atypes[1] = &ffi_type_float;
  av1 = (float)a1; avalue[0] = &av0; avalue[1] = &av1;
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, rtype, atypes) != FFI_OK)
    return 0;
  ffi_call(&cif, FFI_FN(fn->v.p), &ret, avalue);
  *out = ret;
  return 1;
}


int ffi_trace_call3di (CData *fn, double a0, double a1, int64_t a2,
                       double *out) {
  CType *ft, *at0, *at1, *at2, *rt; ffi_cif cif; ffi_type *atypes[3];
  void *avalue[3]; double av0 = a0, av1 = a1, ret = 0.0; int32_t av2;
  if (fn == NULL || out == NULL || fn->magic != LUAR_FFI_MAGIC ||
      (fn->flags & (CDATA_CTYPE | CDATA_CALLBACK)) != 0 || fn->v.p == NULL)
    return 0;
  ft = (fn->ct->kind == CT_FUNC) ? fn->ct : fn->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 3 || ft->vararg)
    return 0;
  at0 = ft->args[0]; at1 = ft->args[1]; at2 = ft->args[2]; rt = ft->ret;
  if (at0 == NULL || at1 == NULL || at2 == NULL || rt == NULL ||
      at0->kind != CT_FLOAT || at0->size != 8 ||
      at1->kind != CT_FLOAT || at1->size != 8 ||
      at2->kind != CT_INT || at2->size != 4 ||
      rt->kind != CT_FLOAT || rt->size != 8) return 0;
  atypes[0] = &ffi_type_double; atypes[1] = &ffi_type_double;
  atypes[2] = &ffi_type_sint32; av2 = (int32_t)a2;
  avalue[0] = &av0; avalue[1] = &av1; avalue[2] = &av2;
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 3, &ffi_type_double, atypes) != FFI_OK)
    return 0;
  ffi_call(&cif, FFI_FN(fn->v.p), &ret, avalue);
  *out = ret; return 1;
}


/* J4 helper: call a C function with two integer arguments and int return. */
int ffi_trace_call2ii (CData *fn, int64_t a0, int64_t a1, int64_t *out) {
  CType *ft, *at0, *at1, *rt;
  ffi_cif cif;
  ffi_type *atypes[2], *rtype = &ffi_type_sint32;
  void *avalue[2];
  int32_t av0, av1;
  int32_t ret = 0;
  if (fn == NULL || out == NULL || fn->magic != LUAR_FFI_MAGIC ||
      (fn->flags & (CDATA_CTYPE | CDATA_CALLBACK)) != 0 || fn->v.p == NULL)
    return 0;
  ft = (fn->ct->kind == CT_FUNC) ? fn->ct : fn->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 2 || ft->vararg)
    return 0;
  at0 = ft->args[0];
  at1 = ft->args[1];
  rt = ft->ret;
  if (at0 == NULL || at1 == NULL || rt == NULL ||
      at0->kind != CT_INT || at0->size != 4 ||
      at1->kind != CT_INT || at1->size != 4 ||
      rt->kind != CT_INT || rt->size != 4)
    return 0;
  atypes[0] = &ffi_type_sint32;
  atypes[1] = &ffi_type_sint32;
  av0 = (int32_t)a0;
  av1 = (int32_t)a1;
  avalue[0] = &av0;
  avalue[1] = &av1;
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, rtype, atypes) != FFI_OK)
    return 0;
  ffi_call(&cif, FFI_FN(fn->v.p), &ret, avalue);
  *out = (int64_t)ret;
  return 1;
}


/* J4 helper: call a C function with three double arguments and a double
** return.  No Lua stack, allocation or metamethods. */
int ffi_trace_call3d (CData *fn, double arg1, double arg2, double arg3,
                      double *out) {
  CType *ft, *at1, *at2, *at3, *rt;
  ffi_cif cif;
  ffi_type *atypes[3];
  void *avalue[3];
  double av1 = arg1, av2 = arg2, av3 = arg3, ret = 0.0;
  if (fn == NULL || out == NULL || fn->magic != LUAR_FFI_MAGIC ||
      (fn->flags & (CDATA_CTYPE | CDATA_CALLBACK |
                    CDATA_OWNED | CDATA_REF)) != 0 ||
      fn->v.p == NULL || fn->ct == NULL)
    return 0;
  if (fn->ct->kind == CT_FUNC)
    ft = fn->ct;
  else if (fn->ct->kind == CT_POINTER && fn->ct->child != NULL &&
           fn->ct->child->kind == CT_FUNC)
    ft = fn->ct->child;
  else
    return 0;
  if (ft->nargs != 3 || ft->vararg)
    return 0;
  at1 = ft->args[0];
  at2 = ft->args[1];
  at3 = ft->args[2];
  rt = ft->ret;
  if (at1 == NULL || at2 == NULL || at3 == NULL || rt == NULL ||
      at1->kind != CT_FLOAT || at1->size != 8 ||
      at2->kind != CT_FLOAT || at2->size != 8 ||
      at3->kind != CT_FLOAT || at3->size != 8 ||
      rt->kind != CT_FLOAT || rt->size != 8)
    return 0;
  atypes[0] = &ffi_type_double;
  atypes[1] = &ffi_type_double;
  atypes[2] = &ffi_type_double;
  avalue[0] = &av1;
  avalue[1] = &av2;
  avalue[2] = &av3;
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 3, &ffi_type_double,
                   atypes) != FFI_OK)
    return 0;
  ffi_call(&cif, FFI_FN(fn->v.p), &ret, avalue);
  *out = ret;
  return 1;
}


/* J4 helper: call a C function with one float argument and a float return.
** The Lua value is a double; it is narrowed for the ffi_type_float ABI. */
int ffi_trace_call1f (CData *fn, double arg, double *out) {
  CType *ft, *at, *rt;
  ffi_cif cif;
  ffi_type *atypes[1];
  void *avalue[1];
  float av = (float)arg, ret = 0.0f;
  if (fn == NULL || out == NULL || fn->magic != LUAR_FFI_MAGIC ||
      (fn->flags & (CDATA_CTYPE | CDATA_CALLBACK |
                    CDATA_OWNED | CDATA_REF)) != 0 ||
      fn->v.p == NULL || fn->ct == NULL)
    return 0;
  if (fn->ct->kind == CT_FUNC)
    ft = fn->ct;
  else if (fn->ct->kind == CT_POINTER && fn->ct->child != NULL &&
           fn->ct->child->kind == CT_FUNC)
    ft = fn->ct->child;
  else
    return 0;
  if (ft->nargs != 1 || ft->vararg)
    return 0;
  at = ft->args[0];
  rt = ft->ret;
  if (at == NULL || rt == NULL || at->kind != CT_FLOAT ||
      at->size != 4 || rt->kind != CT_FLOAT || rt->size != 4)
    return 0;
  atypes[0] = &ffi_type_float;
  avalue[0] = &av;
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &ffi_type_float,
                   atypes) != FFI_OK)
    return 0;
  ffi_call(&cif, FFI_FN(fn->v.p), &ret, avalue);
  *out = (double)ret;
  return 1;
}


/* J4 helper: two float32 arguments, float32 return. */
int ffi_trace_call2f (CData *fn, double a0, double a1, double *out) {
  CType *ft, *at0, *at1, *rt;
  ffi_cif cif;
  ffi_type *atypes[2], *rtype = &ffi_type_float;
  void *avalue[2];
  float av0 = (float)a0, av1 = (float)a1, ret = 0.0f;
  if (fn == NULL || out == NULL || fn->magic != LUAR_FFI_MAGIC ||
      (fn->flags & (CDATA_CTYPE | CDATA_CALLBACK |
                    CDATA_OWNED | CDATA_REF)) != 0 ||
      fn->v.p == NULL || fn->ct == NULL)
    return 0;
  ft = (fn->ct->kind == CT_FUNC) ? fn->ct : fn->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 2 || ft->vararg)
    return 0;
  at0 = ft->args[0];
  at1 = ft->args[1];
  rt = ft->ret;
  if (at0 == NULL || at1 == NULL || rt == NULL ||
      at0->kind != CT_FLOAT || at0->size != 4 ||
      at1->kind != CT_FLOAT || at1->size != 4 ||
      rt->kind != CT_FLOAT || rt->size != 4)
    return 0;
  atypes[0] = &ffi_type_float;
  atypes[1] = &ffi_type_float;
  avalue[0] = &av0;
  avalue[1] = &av1;
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, rtype, atypes) != FFI_OK)
    return 0;
  ffi_call(&cif, FFI_FN(fn->v.p), &ret, avalue);
  *out = (double)ret;
  return 1;
}
