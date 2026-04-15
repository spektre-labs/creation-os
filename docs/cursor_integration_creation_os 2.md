# Integration guide — Creation OS

## C

```bash
make standalone
./creation_os
```

Split objects: `make core` → `.build/*.o`. Link your host binary with those objects and `core/creation_os.h`.

## C++ / CMake (llama.cpp)

`integration/ghost-boot-CMakeLists.txt` builds a static stub from `gda_llama_integration.cpp`. Replace the stub body with your real FFI and include paths.

## MLX (optional)

MLX bridges live outside this minimal C repository. If you combine them, keep buffer ownership explicit and avoid redundant host copies; wire optional Python/MLX paths only where you control the full runtime.
