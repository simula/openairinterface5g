#!/bin/bash
set -eo pipefail

BASE_COMMIT=$(git merge-base HEAD origin/develop)
git diff -U0 --no-color $BASE_COMMIT...HEAD | clang-format-diff -p1
