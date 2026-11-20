# Following upstream Lua

See `doc/lua56-readiness.md` for the concrete 5.6 adoption checklist.

Status: normative for the 0.1 line. See `doc/adr/0003-upstream-deltas.md`
for why Luar chose a tracked delta baseline over a patch queue.

Luar is not a fork of the language. The language, bytecode format, C API and
module ABI are the official PUC-Rio Lua ones; Luar adds runtime features on
top. Every upstream release is therefore an adoption exercise, not a porting
project.

The short version:

```sh
make upstream-check                       # offline: src/ vs vendor/, always in CI
tools/upstream-sync.sh 5.6.0              # dry run: fetch, verify, three-way diff
tools/upstream-sync.sh 5.6.0 --apply      # install vendor/lua-5.6.0/, merge by hand
tools/upstream-check.sh --update          # record reviewed src/ edits in the baseline
tests/upstream/run.sh --jobs 4            # official Lua suite under src/luar
```

## 1. Trees and ownership

| Path | Owner | Rule |
| --- | --- | --- |
| `vendor/lua-<version>/` | upstream | Pristine tarball contents. Never edit; never add local files. |
| `src/` | Luar | Working tree derived from the vendor snapshot. All changes happen here. |
| `src/luar_*`, `src/luar.h`, `src/luar_version.h` | Luar | New functionality lives in new files. |
| `tools/upstream-baseline.txt` | Luar | Checked-in record of every modified upstream file. |
| `tests/tmp/` | disposable | Downloaded suites, caches and reports. Git-ignored. |

`vendor/lua-<version>/` is deliberately pristine, so it still contains
upstream's compile-time `LUA_COMPAT_*` options.  They are not built: `src/`
has every legacy compatibility path removed and targets Lua 5.5 only.  The
vendor tree is a reference for sync and comparison, not a configuration.

`src/Makefile` is project-owned build machinery but has an upstream
counterpart, so the baseline tracks it as a modification. That way a Lua
release that changes the official Makefile shows up during sync instead of
being missed.

## 2. Patch discipline

1. **Never edit `vendor/`.** If a vendor file is wrong, the fix belongs in
   `src/` and needs a decision note.
2. **Keep core edits small and tagged.** Every change in an upstream file
   gets a `/* LUAR */` comment (or `/* LUAR begin */` / `/* LUAR end */` for
   a block). A reader diffing against upstream must see *why* the line is
   there.
3. **Prefer new files.** `jit`, FFI, `table.new`/`table.clear` and the
   extended `openlibs` live in `src/luar_*.c`. Do not scatter `#ifdef`s
   through the core.
4. **No unrelated reformatting.** Do not touch whitespace, order or comments
   in untouched upstream lines; it turns every future rebase into noise.
5. **No legacy-version compatibility code.** If upstream removed it, Luar does not add
   it back.
6. **Record before committing.** A changed core file is not done until
   `tools/upstream-check.sh --update` has recorded its two hashes, the diff
   has been reviewed, and the suites pass.

The baseline is deliberately annoying: editing a tracked file without
refreshing the baseline makes CI fail. That is the point. `--update` is an
acknowledgement that the delta was reviewed, not a way to silence the check.

## 3. The baseline

`tools/upstream-baseline.txt` starts with the vendored upstream version and
then one row per modified file:

```
upstream lua-5.5.1
<sha256 of src/file>  <sha256 of vendor/lua-5.5.1/src/file>  <file>
```

`tools/upstream-check.sh` classifies `src/` against the newest
`vendor/lua-*` snapshot:

* **Luar-added** — `luar_*.c`, `luar_*.h`, `luar.h`. Expected, never
  compared with upstream.
* **Modified** — content differs from the vendored file. Must be in the
  baseline, with both hashes matching the current tree.
* **Missing** — an upstream file absent from `src/`. Always a failure.
* **Extra** — a file in `src/` that is neither an upstream file, a known
  Luar addition, nor a build product. Always a failure; this catches stray
  copies and forgotten renames.
* **Ignored** — build products (`*.o`, `libluar.a`, `luar`, `luarc`, ...)
  are counted but not treated as source drift.

The check fails when:

* a modified file is not listed in the baseline;
* a listed file's **Luar hash** changed (review and `--update`);
* a listed file's **upstream hash** no longer matches the vendored tree
  (the vendor snapshot changed: do the upstream review/merge first);
* a baseline row is stale (the file is no longer modified or was deleted);
* the baseline records a different upstream version from the selected
  vendor tree;
* an upstream file is missing or an unexpected extra source file appears.

`--update` writes a fresh baseline but still refuses when files are missing
or extra. It does not touch `src/` or `vendor/`.

Exit codes: `0` clean, `1` drift, `2` usage/configuration error (for example
no SHA-256 tool). The check is entirely offline, which is why
`make upstream-check` is safe in CI and on a plane.

## 4. Syncing a new release

`tools/upstream-sync.sh <version>` downloads

```
https://www.lua.org/ftp/lua-<version>.tar.gz
```

into `tests/tmp/upstream-sync/`, verifies that it is a readable gzip tar
with the expected `lua-<version>/src/lua.h`, prints the tarball SHA-256, and
unpacks it. It then compares three trees:

* the existing vendor snapshot (or `--from DIR`);
* the new release;
* `src/`.

The report lists upstream-modified/added/removed files, Luar-modified and
Luar-added files, conflicts (changed on both sides), newly added upstream
files missing from `src/`, and upstream removals still present in `src/`.
That is the rebase worklist.

The default is a dry run: **no repository file is modified**. Useful modes:

```sh
tools/upstream-sync.sh 5.6.0 --check-url   # HEAD/GET check only, no download
tools/upstream-sync.sh 5.6.0 --offline     # cached tarball only, no network
tools/upstream-sync.sh 5.6.0 --sha256 HASH # pin the expected tarball hash
tools/upstream-sync.sh 5.6.0 --apply       # install vendor/lua-5.6.0/
```

`--apply` only installs the pristine vendor tree; it never edits `src/`.
An existing `vendor/lua-<version>/` is never overwritten silently:

* identical to the download: no-op;
* different: refuse, because it may contain local edits or a different
  upstream build. `--force` moves the old tree to
  `tests/tmp/upstream-sync/backup/` before replacing it and prints the backup
  path.

For a version that is already vendored, `--apply` with no differences is a
no-op; refreshing the *same* version can therefore be used to verify that the
checked-in vendor tree is still the official tarball.

A new version is added beside the old one so the old tree remains available
for comparison during the merge. Remove the old `vendor/lua-*` directory only
after the new baseline is green and the release is adopted; the baseline
header must name the vendor tree that remains.

## 5. Adoption checklist (example: Lua 5.6.0)

Work on a branch, one logical commit per phase. Do not commit downloaded
tarballs, extracted suites or reports.

### Phase 0 — announce and prepare

- [ ] Read the upstream release notes/manual diffs and the new test suite
      page (`https://www.lua.org/tests/`). Record the suite tarball URL and
      SHA-256 in the release issue.
- [ ] Run the current `make test`, `tools/upstream-check.sh` and
      `tests/upstream/run.sh` on the old release; the baseline must be green
      before changing it.
- [ ] Check that `LUAR_LUA_VERSION`, `LUAR_LUA_VERSION_NUM` and `LUAVER`
      are the only version knobs that need touching (see §6).

### Phase 1 — vendor the release

- [ ] `tools/upstream-sync.sh 5.6.0` (dry run). Save the three-way report in
      the release issue.
- [ ] `tools/upstream-sync.sh 5.6.0 --sha256 <official-hash> --apply`.
- [ ] Commit `vendor/lua-5.6.0/` alone (`vendor: add Lua 5.6.0 snapshot`).
      No `src/` changes in this commit.
- [ ] Add the new test-suite hash/URL to the release issue.

### Phase 2 — rebase Luar's deltas

- [ ] For every conflict and every new/removed upstream file in the sync
      report, inspect the upstream change and re-apply the Luar intent to
      `src/`, keeping edits small and `/* LUAR */`-tagged.
- [ ] Update the version-specific code paths: `src/luar_version.h`
      (`LUAR_LUA_VERSION`, `LUAR_LUA_VERSION_NUM`), top-level `Makefile`
      (`LUAVER`), and any tests that pin the Lua version or bytecode version.
- [ ] Copy new upstream files into `src/`; do not copy the upstream
      `Makefile` over Luar's.
- [ ] Run `tools/upstream-check.sh`. Expect failures for every deliberate
      change; review the full diff (`diff -u vendor/lua-5.6.0/src/FILE
      src/FILE`) before running `tools/upstream-check.sh --update`.
- [ ] Commit `src/` rebase and the refreshed baseline together
      (`core: rebase on Lua 5.6.0`). Never combine unrelated feature work.

### Phase 3 — verify

- [ ] `make clean && make` (also `make WITH_FFI=1` when the FFI milestone
      is complete). Warnings are review material, not background noise.
- [ ] `make test` — smoke, CLI/ABI and semantics/compat suites.
- [ ] `tests/cli.sh` with a stock Lua module built for the new minor, where
      a system Lua is available; this is the ABI proof.
- [ ] `tests/upstream/run.sh --jobs 4` — the official basic suite. Inspect
      `tests/tmp/upstream-report.txt`; the canonical `all.lua` must print
      `final OK !!!`. Investigate every per-file failure; a file that merely
      returns early because `_U` skipped non-portable tests is a known
      limitation, not a pass of that subsystem.
- [ ] Run the semantics suite under stock Lua 5.6 as well when available
      (`tests/diff/run.sh` style differential run) so Luar is not the only
      oracle.
- [ ] `make bench` and record the numbers; compare against the previous
      release. No hard gate, but unexplained regressions block the release.
- [ ] Optional but recommended: run the upstream **complete** suite manually
      (see §7) and record the known system-dependent/branding differences.

### Phase 4 — bookkeeping and release

- [ ] Update `README.md` status/version text, `man/luar.1`, `etc/luar.pc`
      if the Luar version changes, and the compatibility/architecture docs
      where they name the Lua base release.
- [ ] Add a changelog entry: upstream release, Luar deltas rebased, tests
      and benchmarks, known deviations.
- [ ] Remove the old `vendor/lua-*` tree once the new baseline is green
      (separate commit), unless it is needed for a maintenance branch.
- [ ] Tag the release only after `make test`, `tools/upstream-check.sh` and
      the official suite pass from a clean checkout.
- [ ] Never release with a stale baseline or with uncommitted `src/` edits.

## 6. Version-specific knobs

Keep these in sync when moving from 5.5 to 5.6:

| Location | What |
| --- | --- |
| `src/lua.h` | `LUA_VERSION_*` (comes from the upstream snapshot; do not hand-edit unless upstream changed it) |
| `src/luar_version.h` | `LUAR_LUA_VERSION`, `LUAR_LUA_VERSION_NUM` |
| `Makefile` | `LUAVER` (module directory component, e.g. `5.5`) |
| `src/luaconf.h` | module paths are built from `LUA_VDIR`; only touch `LUAR_*` additions |
| `README.md`, `man/luar.1`, `doc/*` | human-readable release names |
| `tests/upstream/run.sh` | derives the suite version from `LUAR_LUA_VERSION`; no edit normally |

The extension directories (`share/luar/5.5`, `lib/luar/5.5`) follow the Lua
minor, not the Luar minor: a Lua 5.5 ABI module keeps loading on Luar 0.33.0.

## 7. Official test suite

`tests/upstream/run.sh` (also wired as `make test-upstream`) downloads

```
https://www.lua.org/tests/lua-<version>-tests.tar.gz
```

into `tests/tmp/upstream/` (ignored), unpacks it there, and runs it under
`src/luar`. It never commits the suite.

What it does:

1. fetches the suite only if the cached tarball is absent/corrupt
   (`--offline` forbids the network);
2. creates a `lua` symlink on `PATH` for tests that invoke the binary by
   name;
3. runs every top-level test file except `all.lua`, `tracegc.lua` and
   `heavy.lua` in its own copy of the suite, with `_soft`, `_port` and
   `_nomsg` set as the official `_U` mode does — safe for `--jobs N`;
4. runs the suite's authoritative `all.lua` driver with `-e"_U=true"` and
   checks for `final OK !!!`; because Luar is strict 5.5, the runner disables
   the one `goto.lua` assertion that expects `global` to be a legal variable
   name under `LUA_COMPAT_GLOBAL` (upstream's own `ltests` build makes the
   same choice);
5. writes `tests/tmp/upstream-report.txt` with one PASS/FAIL line per file,
   the canonical result and the tail of each failure log.

At the time of writing (Lua 5.5.1, Luar 0.33.0.1) the observed result is:

```
per-file: 31 run, 31 passed, 0 failed
canonical: PASS (final OK !!!)
suite sha256: da07b543872dc0bb2ff12aabd0c248578d78df3eb6b67efdc537a46d455c7f31
```

### Known limitations

* The runner uses the official **basic** mode (`_U=true`). That is the mode
  lua.org documents as portable; it deliberately skips non-portable tests.
  `main.lua` returns immediately in this mode, and `big.lua` short-circuits
  under `_soft`. Their PASS means the file's harness completed, not that
  every subsystem was exercised.
* The **complete** suite is not automated. It needs the C helpers in
  `libs/` built against Luar's headers, and it contains environment-specific
  checks. On Linux/glibc with Luar 0.33.0.1:
  * `main.lua` asserts that `-v` prints `PUC-Rio` in the copyright line;
    Luar intentionally prints its own banner (`Luar ... Copyright (C) 2026
    Luar contributors`) while still naming `Lua 5.5.1`. This is a deliberate
    branding deviation, not a semantic one, but it stops the unmodified
    complete run at `main.lua`.
  * `files.lua` expects seeking on stdin to fail; that depends on what stdin
    is. Feeding the suite from a pipe (`: | ./src/luar all.lua`) makes that
    check pass, but the same file later asserts `io.popen`/`os.execute`
    signal-vs-exit statuses that vary by shell and OS (`files.lua:814` on the
    Linux/dash environment used here). With a branding-tolerant scratch
    copy and pipe stdin, the run reaches the last top-level test file and
    stops at that assertion.
  The complete suite is explicitly system-dependent upstream ("expect to get
  your hands dirty"); it is useful for manual deep dives and is not a release
  gate. The basic mode above is the repeatable one.
* Internal `ltests` tests require recompiling Lua with `ltests.c`/`ltests.h`
  and `-DLUA_USER_H`, exactly as upstream documents. Luar does not ship that
  build; those tests are outside this runner.

Downloaded tarballs and reports under `tests/tmp/` are disposable. Delete the
directory at any time; the next run re-fetches.

## 8. CI contract

`.github/workflows/ci.yml` runs, on Linux and optionally macOS:

```sh
make
make test
tools/upstream-check.sh
```

plus a best-effort `make WITH_FFI=1` once `src/luar_ffi.c` exists. The
upstream check is offline and uses only POSIX shell plus a SHA-256 tool
(`sha256sum`, `shasum` or `openssl`). Official-suite runs are not part of CI:
they need network access, and the suite is not a product. Run them before a
release and attach the report to the release issue.

## 9. Why the ABI makes this cheap

Luar does not define a second value model. `TValue`, the stack layout, the
C API in `lua.h`/`lauxlib.h` and the module ABI remain the official ones;
extensions allocate their own registries and userdata and call the public
API. Consequences:

* a binary C module built for the matching upstream Lua minor loads in Luar
  without recompilation (`tests/cli.sh` checks a stock `lua5.5` module);
* upstream bug fixes and semantic changes arrive by merging core files, not
  by translating between two runtimes;
* the delta stays small enough to audit: eleven tracked core/build files
  (the CLI, `luaconf.h`, `lualib.h`, `lauxlib.h`, the lexer/parser,
  `lmathlib.c`, the gated FFI hook in `lvm.c`, and `Makefile`) plus a small
  set of additive `luar_*` source/header files.  The lexer/parser/mathlib
  deltas remove the old-version compatibility code; `lvm.c` carries only the
  gated `/* LUAR */` FFI equality hook;
* when Lua 5.6 changes internals, the rebase is visible in one three-way
  report, and anything the JIT hooks is forced through the same review.

Keeping the baseline honest is what preserves that property: an unrecorded
core edit is a hidden fork, and a stale upstream hash means the vendor
reference no longer means what the baseline says it means.
