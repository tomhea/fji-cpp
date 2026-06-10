# Known warnings baseline

Captured from `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build`
on the initial working source (commit acd45d6).

```
(no warnings)
```

All new PRs must produce zero new lines beyond this baseline under `-Wall -Wpedantic`.
