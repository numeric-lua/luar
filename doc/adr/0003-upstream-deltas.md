# ADR 0003 — Upstream deltas as a tracked baseline, not a patch queue

Date: 2026-11-17
Status: accepted

## Context

LuaR must adopt new Lua releases quickly. The usual fork options are:

1. keep a directory of quilt-style patches applied to upstream sources;
2. edit sources in place and rely on git history;
3. vendor a pristine snapshot and track the fork's deltas explicitly.

Patch queues are exact but painful to rebase across releases and awkward for
normal development. Pure in-place editing makes it hard to answer "what did
we change and is it still current?".

## Decision

Keep `vendor/lua-X.Y.Z/` as a pristine snapshot and `src/` as the working
tree. Record the known deltas in `tools/upstream-baseline.txt` (file paths,
LuaR hashes, upstream hashes), enforce it with `tools/upstream-check.sh`, and
provide `tools/upstream-sync.sh` for adopting a new release with a three-way
diff. Core edits are small and tagged `/* LUAR */`; new functionality lives
in new files.

## Consequences

Positive:

* the delta is reviewable at any time and CI fails on unrecorded drift;
* adopting a release is a merge operation with a clear checklist;
* contributors can work on `src/` normally, without a patch-application step.

Negative:

* the baseline must be refreshed intentionally after each core edit;
* duplicate source trees in the repository (small: ~1.5 MB).

## Alternatives rejected

* **Patch queue only** — adds tooling friction to every core touch and breaks
  the normal edit/build/test loop.
* **Git-subtree only** — does not express semantic deltas; rebases still
  require manual conflict resolution plus a test pass.
