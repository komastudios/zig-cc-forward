# zig-cc-forward

## Compile

**Windows**:
```
zig build --prefix .install/win64 -Dtarget=x86_64-windows-gnu -Doptimize=ReleaseFast
```

## Debug

**Windows**:

```
zig cc -l shlwapi -o zig-cc.exe .\zig-cc-forward.c
```
