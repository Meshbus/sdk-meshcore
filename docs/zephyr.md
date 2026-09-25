# Zephyr Integration

## External Adapter Contract

[zephyr/module.yml](../zephyr/module.yml) declares `cmake-ext` and
`kconfig-ext`. This repository supplies protocol sources and headers; the host
supplies external Zephyr build integration and every platform hook. Module
discovery alone does not compile or configure a usable runtime.

The library's own [Zephyr tests](../tests/zephyr/TESTING.md) compile this
checkout directly from its source manifest and use test-local platform hooks.
They do not exercise a host's external CMake/Kconfig adapter. The production
adapter contract below remains the host's responsibility.

Use the module name `meshcore` (and checkout directory `meshcore`) so the
external integration variables below match discovery. The host owns the
adapter files and its Kconfig symbols.

## Independent Host Layout

An independent application can provide its own `MODULE_EXT_ROOT`:

```text
host-integration/
  modules/
    modules.cmake
    meshcore/
      CMakeLists.txt
      Kconfig
```

Its `modules/modules.cmake` identifies the external files:

```cmake
set(ZEPHYR_MESHCORE_CMAKE_DIR ${CMAKE_CURRENT_LIST_DIR}/meshcore)
set(ZEPHYR_MESHCORE_KCONFIG ${CMAKE_CURRENT_LIST_DIR}/meshcore/Kconfig)
```

A minimal host-owned `modules/meshcore/Kconfig` can select the library:

```kconfig
config MESHCORE
    bool "MeshCore protocol runtime"
    depends on ZEPHYR_MESHCORE_MODULE
```

The matching `CMakeLists.txt` consumes the canonical manifest:

```cmake
if(CONFIG_MESHCORE)
  include(${ZEPHYR_MESHCORE_MODULE_DIR}/cmake/meshcore_sources.cmake)
  zephyr_include_directories(${MESHCORE_PUBLIC_INCLUDE_DIRS})
  zephyr_library()
  zephyr_library_sources(${MESHCORE_RUNTIME_LIBRARY_SOURCES})
  zephyr_library_include_directories(${MESHCORE_INTERNAL_INCLUDE_DIRS})
endif()
```

Enable `CONFIG_MESHCORE=y` in the host application and compile the application's
`meshcore_platform_*` implementations separately. Register the checkout through
your west manifest or `ZEPHYR_EXTRA_MODULES`; pass the absolute integration root
using `-DMODULE_EXT_ROOT=/absolute/path/to/host-integration` when configuring
Zephyr, or expose it through a host module's `build.settings.module_ext_root`.
Use a fresh build directory when changing module discovery or adapter paths.

Do not also compile this library through standalone `add_subdirectory` in the
same Zephyr image: that would create a second source provider. Keep compiler
settings in the Zephyr adapter and follow the
[configuration guide](configuration.md) when propagating macros.

## Host Runtime Work

Implement timers using queued expiry on one runtime owner, copy radio RX/TX
buffers with the documented lifetimes, and bridge host crypto, storage and
application events as described in the [porting guide](porting.md). The generic
library does not register a Zephyr thread, radio driver, Settings handler,
Bluetooth service or product initialization sequence.

## Validation Scope

The standalone CI runs native CMake/CTest, installed-package checks and the
library-owned Zephyr test suite on `native_sim`. That suite compiles the library
without a host adapter. The layout above documents the external-module
mechanism and source manifest contract; it does not validate a host application
or radio. Select the host repository's supported board/test metadata and record
the Zephyr revision, configuration, build command and any device evidence there.
