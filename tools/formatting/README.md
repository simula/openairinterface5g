# Overview

Dockerized tool to list clang-format errors introduced in a branch.

# Usage

Checkout the branch you want to compare. This is necessary because if there are files added by the branch
they need to be present for clang-format to analyze them.

Then:

```
./tools/formatting/detect_clang_format_errors.sh
```

Or

```
docker compose up
```

Or (to avoid docker related lines in stdout)

```
docker compose run clang-formatter | tee output
```
