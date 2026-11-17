#!/bin/sh
# Build the demo and run it. Exits nonzero if the build fails or if the
# interpreted/native comparison in ./demo finds any mismatch.
set -eu
cd "$(dirname "$0")"
make
exec ./demo
