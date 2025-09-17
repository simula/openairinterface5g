# Overview

IWYU - [include what you use](https://github.com/include-what-you-use/include-what-you-use)
is a tool that can detect and correct C/C++ include errors

# Usage

```
TARGET=<your target> docker compose up
```

Automatically apply suggestions:

```
TARGET=<your target> docker compose run iwyu | tee suggestions
cat suggestions | python3 ~/include-what-you-use/fix_includes.py
```

Apply only removal suggestions

```
cat suggestions | grep "should add these line\|The full include-list\|should remove these lines\|^-" | python3 ~/include-what-you-use/fix_includes.py
```
