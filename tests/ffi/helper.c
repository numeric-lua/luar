/*
** tests/ffi/helper.c -- small C library used by the FFI tests.
**
** It provides layout probes (sizeof/offsetof), struct-by-value entry
** points and a qsort wrapper with a C comparator.  Only the C standard
** library is used.  Built at test time by tests/ffi/build_helper.sh.
*/

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

struct Pair { int a; int b; };
struct Big { char c; double d; long n; };
struct Nested { struct Pair p; int z; };
struct Bits { unsigned char a; unsigned short b; unsigned int c; };
struct PtrPair { struct Pair *p; int tag; };
union Val { int i; double d; };
struct WithArray { int a[3]; };
struct WithUnion { int t; union { int i; double d; } u; };
struct Multi {
  int x;
  int y;
  char name[8];
  struct { short s; unsigned char b; } sub;
};
struct Outer {
  int tag;
  union { int i; double d; };
  struct { int a; int b; };
};


int size_Pair (void) { return (int)sizeof(struct Pair); }
int off_Pair_b (void) { return (int)offsetof(struct Pair, b); }
int size_Big (void) { return (int)sizeof(struct Big); }
int off_Big_c (void) { return (int)offsetof(struct Big, c); }
int off_Big_d (void) { return (int)offsetof(struct Big, d); }
int off_Big_n (void) { return (int)offsetof(struct Big, n); }
int size_Nested (void) { return (int)sizeof(struct Nested); }
int off_Nested_p (void) { return (int)offsetof(struct Nested, p); }
int off_Nested_z (void) { return (int)offsetof(struct Nested, z); }
int size_Bits (void) { return (int)sizeof(struct Bits); }
int off_Bits_b (void) { return (int)offsetof(struct Bits, b); }
int off_Bits_c (void) { return (int)offsetof(struct Bits, c); }
int size_PtrPair (void) { return (int)sizeof(struct PtrPair); }
int off_PtrPair_p (void) { return (int)offsetof(struct PtrPair, p); }
int off_PtrPair_tag (void) { return (int)offsetof(struct PtrPair, tag); }
int size_Val (void) { return (int)sizeof(union Val); }
int off_Val_i (void) { return (int)offsetof(union Val, i); }
int off_Val_d (void) { return (int)offsetof(union Val, d); }
int size_WithArray (void) { return (int)sizeof(struct WithArray); }
int size_WithUnion (void) { return (int)sizeof(struct WithUnion); }
int off_WithUnion_u (void) { return (int)offsetof(struct WithUnion, u); }
int size_Multi (void) { return (int)sizeof(struct Multi); }
int off_Multi_x (void) { return (int)offsetof(struct Multi, x); }
int off_Multi_y (void) { return (int)offsetof(struct Multi, y); }
int off_Multi_name (void) { return (int)offsetof(struct Multi, name); }
int off_Multi_sub (void) { return (int)offsetof(struct Multi, sub); }
int size_Outer (void) { return (int)sizeof(struct Outer); }
int off_Outer_tag (void) { return (int)offsetof(struct Outer, tag); }
int off_Outer_i (void) { return (int)offsetof(struct Outer, i); }
int off_Outer_d (void) { return (int)offsetof(struct Outer, d); }
int off_Outer_a (void) { return (int)offsetof(struct Outer, a); }
int off_Outer_b (void) { return (int)offsetof(struct Outer, b); }


struct Pair pair_make (int a, int b) {
  struct Pair p;
  p.a = a;
  p.b = b;
  return p;
}

int pair_sum (struct Pair p) { return p.a + p.b; }

struct Pair pair_add (struct Pair p, struct Pair q) {
  struct Pair r;
  r.a = p.a + q.a;
  r.b = p.b + q.b;
  return r;
}

int pair_sum_ptr (struct Pair *p) { return p->a + p->b; }

void pair_bump (struct Pair *p, int d) {
  p->a += d;
  p->b += d;
}

int pair_array_sum (struct Pair *p, int n) {
  int sum = 0, i;
  for (i = 0; i < n; i++)
    sum += p[i].a + p[i].b;
  return sum;
}

struct PtrPair ptrpair_make (struct Pair *p, int tag) {
  struct PtrPair pp;
  pp.p = p;
  pp.tag = tag;
  return pp;
}

int ptrpair_sum (struct PtrPair pp) { return pp.p->a + pp.p->b + pp.tag; }

double big_sum (struct Big b) {
  return (double)b.c + b.d + (double)b.n;
}

struct Big big_make (char c, double d, long n) {
  struct Big b;
  b.c = c;
  b.d = d;
  b.n = n;
  return b;
}

struct Nested nested_make (int a, int b, int z) {
  struct Nested n;
  n.p.a = a;
  n.p.b = b;
  n.z = z;
  return n;
}

int nested_sum (struct Nested n) {
  return n.p.a + n.p.b + n.z;
}

int bits_sum (struct Bits b) {
  return (int)b.a + (int)b.b + (int)b.c;
}

int with_array_sum (struct WithArray w) {
  return w.a[0] + w.a[1] + w.a[2];
}

int with_union_sum (struct WithUnion w) {
  return w.t + w.u.i;
}

int val_int (union Val v) { return v.i; }

union Val val_make (int i) {
  union Val v;
  v.i = i;
  return v;
}

long long ll_add (long long a, long long b) { return a + b; }
unsigned long long ull_add (unsigned long long a, unsigned long long b) {
  return a + b;
}
double d_add (double a, double b) { return a + b; }
float f_add (float a, float b) { return a + b; }
int fn_apply (int (*fn)(int), int x) { return fn(x); }

int cmp_int (const void *a, const void *b) {
  int x = *(const int *)a;
  int y = *(const int *)b;
  return (x > y) - (x < y);
}

void sort_ints (int *arr, int n) {
  qsort(arr, (size_t)n, sizeof(int), cmp_int);
}

enum HelperColor { HC_RED = 1, HC_GREEN = 2, HC_BLUE = 3 };
enum HelperColor helper_color_next (enum HelperColor c) {
  return c == HC_BLUE ? HC_RED : (enum HelperColor)(c + 1);
}
int helper_color_value (enum HelperColor c) { return (int)c; }
_Bool helper_bool_not (_Bool b) { return !b; }

/* Variadic probes: check the vararg conversions without depending on a
** particular printf implementation. */
int vararg_first_is_null (int tag, ...) {
  va_list ap;
  void *p;
  (void)tag;
  va_start(ap, tag);
  p = va_arg(ap, void *);
  va_end(ap);
  return p == NULL;
}

long long vararg_fixed_sum (struct Pair p, enum HelperColor c, _Bool b,
                            long long n, ...) {
  va_list ap;
  long long x;
  va_start(ap, n);
  x = va_arg(ap, long long);
  va_end(ap);
  return (long long)p.a + p.b + (long long)c + (b ? 1 : 0) + n + x;
}
/* Callback with a struct-by-value argument. */
int call_pair_cb (int (*cb)(struct Pair), struct Pair p) {
  return cb(p);
}
/* Callback returning a struct by value. */
struct Pair call_pair_ret_cb (struct Pair (*cb)(void)) {
  return cb();
}
/* Thread-local storage through dlsym. */
__thread int ffi_tls_counter = 11;
int ffi_tls_get (void) { return ffi_tls_counter; }
void ffi_tls_set (int v) { ffi_tls_counter = v; }
/* Bitfield layout and access probes. */
struct BitPack { unsigned int a : 3; int b : 5; unsigned int c : 8; int d; };
int size_BitPack (void) { return (int)sizeof(struct BitPack); }
int off_BitPack_d (void) { return (int)offsetof(struct BitPack, d); }
int bitpack_a_ptr (const struct BitPack *p) { return (int)p->a; }
int bitpack_b_ptr (const struct BitPack *p) { return (int)p->b; }
int bitpack_c_ptr (const struct BitPack *p) { return (int)p->c; }
int bitpack_d_ptr (const struct BitPack *p) { return p->d; }
int bitpack_by_value (struct BitPack p) { return (int)p.a; }
struct BitMix { unsigned char a : 4; unsigned int b : 12;
                unsigned char c : 4; int d; };
int size_BitMix (void) { return (int)sizeof(struct BitMix); }
int off_BitMix_d (void) { return (int)offsetof(struct BitMix, d); }
int bitmix_a_ptr (const struct BitMix *p) { return (int)p->a; }
int bitmix_b_ptr (const struct BitMix *p) { return (int)p->b; }
int bitmix_c_ptr (const struct BitMix *p) { return (int)p->c; }
/* Structs with array members, passed and returned by value. */
struct Named { char name[8]; int n; };
int named_sum (struct Named x) {
  int i = 0;
  while (i < 8 && x.name[i] != '\0') i++;
  return i + x.n;
}
struct Named named_make (const char *s, int n) {
  struct Named x;
  int i;
  for (i = 0; i < 7 && s[i] != '\0'; i++) x.name[i] = s[i];
  x.name[i] = '\0';
  x.n = n;
  return x;
}
struct PairBox { struct Named items[2]; };
int box_sum (struct PairBox b) { return b.items[0].n + b.items[1].n; }
/* Scalar unions by value. */
union DoubleU { double d; };
double du_get (union DoubleU u) { return u.d; }
union DoubleU du_make (double d) { union DoubleU u; u.d = d; return u; }
union FloatU { float f; };
float fu_get (union FloatU u) { return u.f; }
union FloatU fu_make (float f) { union FloatU u; u.f = f; return u; }
union MixedU { float f; int i; };
int mu_get (union MixedU u) { return u.i; }
union MixedU mu_make (int i) { union MixedU u; u.i = i; return u; }
/* Callback returning a small union by value. */
int call_val_cb (union Val (*cb)(void)) { union Val v = cb(); return v.i; }
struct BitAfter { char x; int a : 3; };
int size_BitAfter (void) { return (int)sizeof(struct BitAfter); }
int bafter_a_ptr (const struct BitAfter *p) { return p->a; }
struct BitLong { char a : 4; long b : 40; char c; };
int size_BitLong (void) { return (int)sizeof(struct BitLong); }
int off_BitLong_c (void) { return (int)offsetof(struct BitLong, c); }
long blong_b_ptr (const struct BitLong *p) { return p->b; }
struct ZeroBit { char a; int : 0; char b; };
int size_ZeroBit (void) { return (int)sizeof(struct ZeroBit); }
int off_ZeroBit_b (void) { return (int)offsetof(struct ZeroBit, b); }
struct PadBit { unsigned a : 3; unsigned : 2; unsigned b : 3; };
int size_PadBit (void) { return (int)sizeof(struct PadBit); }
unsigned padbit_a_ptr (const struct PadBit *p) { return p->a; }
unsigned padbit_b_ptr (const struct PadBit *p) { return p->b; }
union PairU { double d[2]; };
double pairu_sum (union PairU u) { return u.d[0] + u.d[1]; }
union PairU pairu_make (double a, double b) { union PairU u; u.d[0] = a; u.d[1] = b; return u; }
union LongPairU { long v[2]; };
long lpair_sum (union LongPairU u) { return u.v[0] + u.v[1]; }
union LongPairU lpair_make (long a, long b) { union LongPairU u; u.v[0] = a; u.v[1] = b; return u; }
struct PairUBox { int tag; union PairU u; };
double pairu_box_sum (struct PairUBox b) { return b.tag + b.u.d[0] + b.u.d[1]; }
union Mixed16 { struct { long l; double d; } s; double d[2]; };
double mixed16_get (union Mixed16 u) { return (double)u.s.l + u.s.d; }
union Mixed16 mixed16_make (long l, double d) {
  union Mixed16 u; u.s.l = l; u.s.d = d; return u;
}
struct Mixed16Box { int tag; union Mixed16 u; };
double mixed16_box_get (struct Mixed16Box b) { return b.tag + b.u.s.l + b.u.s.d; }
