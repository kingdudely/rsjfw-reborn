# RSJFW **RE***BOR***N**
**Roblox Studio Just Fucking Works**. REBORN!

![Badge](https://img.shields.io/badge/C%2B%2B-20-blue) ![Badge](https://img.shields.io/badge/Vulkan-Enabled-red) ![Badge](https://img.shields.io/badge/Zero-Latency-brightgreen)

The high-performance, native C++20 Linux environment for Roblox Studio. Built to obliterate Python/Shell/*Go* wrappers with raw speed and engineering precision.

> **Note**: While RSJFW is an independent, clean-room engineered solution designed for maximum performance, we acknowledge the prior work of the Vinegar project which demonstrated the viability of Wine-based Roblox environments on Linux. 
> **Also**: RSJFW Reborn is in V1 again. Expect bugs, but not gonna lie it should be stabler now.

## Why RSJFW? (Vinegar K)
Legacy wrappers are slow, bloated, and prone to breakage. RSJFW is an engine, not a simple command runner.

| Feature | Legacy Wrappers | RSJFW |
|---------|-----------------|-------|
| **Core Architecture** | Python / Bash Glue | **Native C++20 Engine** |
| **Startup Latency** | 2-5 Seconds | **< 200ms** |
| **Protocol Handling** | Fragile, often breaks | **Native Socket/Pipe Interop** |
| **Diagnostics** | "Check the logs" | **Self-Healing Orchestrator** |
| **Asset Control** | Vague "latest" | **Precise Binary Selection** |
| **GPU Management** | Manual env vars | **Auto-Discovery & Injection** |

## Features

*   **Zero-Latency UI**: Built with Dear ImGui and GLFW for an instant, responsive dashboard.
*   **Modular Runners**: First-class support for **Steam Proton**, **GE-Proton**, and **System Wine**.
*   **Auto-Discovery**: Automatically finds your Steam Proton installations and GPU hardware.
*   **Smart Assets**: Intelligently filters GitHub releases to download only valid binary archives, preventing "source code" download errors.
*   **Preset System**: Save, load, import, and export configuration presets. Exports are sanitized to protect your privacy.
*   **Live Diagnostics**: A real-time health grid that detects and fixes issues with dependencies, desktop entries, and protocol handlers.

## Usage

### 1. Registration (First Run)
After compiling, register RSJFW with your desktop environment to handle `roblox-studio://` links.
```bash
./rsjfw register
```

### 2. Configuration
Open the high-fidelity dashboard to configure runners, DXVK, and FFlags.
```bash
./rsjfw config
```

### 3. Launching
RSJFW automatically handles browser protocols. You can also launch it manually:
```bash
./rsjfw launch
```

### 4. Headless Install
For CI/CD or automated setups, install the latest Roblox Studio without launching the UI:
```bash
./rsjfw install
```

## Configuration Guide

### GPU Selector
In the **General** tab, select your primary GPU. RSJFW automatically injects `MESA_VK_DEVICE_SELECT` and `DRI_PRIME` to ensure Studio runs on your discrete hardware.

### Presets
*   **Save**: Snapshots your current configuration (Runner, FFlags, Env Vars).
*   **Export**: Creates a portable `.rsjfwpreset` file. Local paths are stripped to ensure the preset works on other machines.

### FFlags
Manage Fast Flags with a clean grid interface. Use the "Presets" dropdown to instantly apply **Performance**, **Quality**, or **Vanilla** profiles.

## Build from Source

### Dependencies
*   CMake 3.14+
*   C++20 Compiler (GCC 10+ / Clang 10+)
*   Development headers: `libcurl`, `libarchive`, `glfw3`, `gtk3` (for file dialogs), `opengl`

### Compilation
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## License

MIT License. See [LICENSE](LICENSE) for details.
