# MiniPack

轻量级的文件打包工具和库（C++20）。

Lightweight file packaging tool and libraries (C++20).

---

## MiniPack 提供：

## MiniPack provides:

- 将多个文件打包为单个 `.pack` 文件的可执行程序（`MiniPack`）。

- An executable (`MiniPack`) that packs multiple files into a single `.pack` file.

- 读写 pack 的核心库（`minipack_writer`、`minipack_reader`、`minipack_utf`）。

- Core libraries for reading/writing packs (`minipack_writer`, `minipack_reader`, `minipack_utf`).

- 支持从文件列表或直接递归打包目录（在可执行文件中通过 `dir_scan.cpp` 实现）。

- Supports packaging from a file list or by recursively packing a directory (directory scanning implemented in `dir_scan.cpp` for the executable).

---

## 主要特点

## Main features

- 除STL外不依赖任何第三方库

- No third-party dependencies except for the STL.

- 纯 C++20 实现，可移植到 Windows / POSIX 平台。

- Pure C++20 implementation, portable to Windows / POSIX platforms.

- 支持将文件名以相对路径的形式存储到包内。

- File names are stored in the pack as relative paths.

- 可选择只写索引（info block），不写数据（`--index-only`）。

- Optionally write only the index (info block) without file data (`--index-only`).

- 可选使用静态 C 运行时（CMake 选项 `USE_STATIC_CRT`）。

- Optional static C runtime via CMake option `USE_STATIC_CRT`.

---

## 快速开始（使用 CMake）

## Quick start (using CMake)

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
# 可执行文件位于 build/bin/MiniPack
```

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
# The executable is located at build/bin/MiniPack
```

---

- 说明：CMake 选项 `USE_STATIC_CRT` 默认为 `ON`，用于选择是否链接静态运行时（MSVC 使用 `/MT`，其它工具链尝试 `-static-libgcc -static-libstdc++`）。

- Note: The CMake option `USE_STATIC_CRT` defaults to `ON` and controls whether to link the static runtime (MSVC uses `/MT`, other toolchains attempt `-static-libgcc -static-libstdc++`).

---

## 用法示例

## Usage examples

- 通过文件列表打包：
  `MiniPack list.txt output.pack`
  其中 `list.txt` 每行一个要打包的文件路径。

- Pack using a file list:
  `MiniPack list.txt output.pack`
  where `list.txt` contains one file path per line.

---

- 直接打包目录：
  `MiniPack path/to/directory output.pack`
  将递归枚举目录下的所有常规文件，存储名为相对于目录的路径。

- Pack a directory directly:
  `MiniPack path/to/directory output.pack`
  This recursively enumerates all regular files under the directory and stores names as paths relative to the directory.

---

- 只写索引（不写数据）：
  `MiniPack path/to/directory output.pack --index-only`

- Write only the index (no data):
  `MiniPack path/to/directory output.pack --index-only`

---

## 文件列表格式

## File list format

- 每行一个文件路径。

- One file path per line.

- 空行或以 `#` 开头的行将被忽略（可用于注释）。

- Empty lines or lines starting with `#` are ignored (can be used for comments).

---

## MiniPack 文件结构（概要）

## MiniPack file layout (overview)

下面用表格形式给出 `.pack` 文件开头各部分的概要说明：

The following table gives an overview of the leading parts of a `.pack` file:

| 字段 | 偏移 / 大小 | 描述 |
|---|---:|---|
| Magic | 0 (8 bytes) | ASCII `"MINIPACK"`，用于识别文件类型。 |
| InfoSize | 8 (4 bytes, little-endian uint32) | 表示紧随其后的 info 块长度（字节数）。 |
| Info Block | 12 (InfoSize bytes) | 包含包级元数据与每个文件的条目（详见下方“Info Block 布局”表）。 |
| Data Area | 12 + InfoSize (variable) | 可选，紧跟 info 块后的各文件原始字节流，按条目顺序拼接。若使用 `--index-only` 则不存在数据区。 |

| Field | Offset / Size | Description |
|---|---:|---|
| Magic | 0 (8 bytes) | ASCII "MINIPACK" used to identify the file type. |
| InfoSize | 8 (4 bytes, little-endian uint32) | Length in bytes of the following info block. |
| Info Block | 12 (InfoSize bytes) | Contains package-level metadata and per-file entries (see the Info Block layout table below). |
| Data Area | 12 + InfoSize (variable) | Optional; concatenated raw file data that follows the info block. If `--index-only` is used, no data area is present. |

---

说明：包内文件名始终使用 UTF-8 存储，不再包含“名称编码”字节。名称区布局调整为：先写入所有文件名长度（每个 1 字节，不含 NUL），再连续写入所有 UTF-8 名称，每个名称追加一个 `\0` 结尾。

Note: Filenames in the pack are always stored as UTF-8; no per-pack name-encoding byte. The names area layout is: first all name lengths (1 byte each, excluding NUL), then all UTF-8 names back-to-back, each terminated by a single `\0`.

---

## Info Block 布局（相对于 Info Block 起始偏移）

## Info Block layout (offsets relative to start of the info block)

下面的表格详细列出 info 块头部与每个文件条目的字段（所有多字节整数采用小端 little-endian）：

The table below details the info block header and per-file entry fields (multi-byte integers are little-endian):

| Offset (in info) | Size | Field | 描述 |
|---:|:---:|---|---|
| 0 | 4 | `version` | uint32，包格式版本。 |
| 4 | 4 | `file_count` | uint32，包内文件总数。 |
| 8 | N | `name_lengths[]` | `file_count` 个 1 字节长度（不含 NUL）。 |
| 8+N | M | `names` | 按顺序拼接的 UTF-8 名称，每个以 `\0` 结尾。 |
| 8+N+M | 8*file_count | `entries` | 每个文件的元数据：`data_length`(u32) + `data_offset`(u32)。 |

| Offset (in info) | Size | Field | Description |
|---:|:---:|---|---|
| 0 | 4 | `version` | uint32 package format version. |
| 4 | 4 | `file_count` | uint32 total number of files in the package. |
| 8 | N | `name_lengths[]` | `file_count` one-byte lengths (excluding NUL). |
| 8+N | M | `names` | UTF-8 names concatenated, each terminated by `\0`. |
| 8+N+M | 8*file_count | `entries` | Per-file metadata: `data_length`(u32) + `data_offset`(u32). |

---

注意事项：
Notes:

- `data_offset` 指向紧随 info 块之后的数据区内的位置（即数据区的起始为文件中全局偏移 `12 + InfoSize`）。
- `data_offset` values point into the data area that follows immediately after the info block.

- 文件名以长度表 + NUL 结尾的 UTF-8 名称区形式存储（非逐条长度前缀）。
- Filenames are stored as a name-length table + a NUL-terminated UTF-8 names area (not per-entry length prefixes).

- 单个文件和所有文件大小的总合都不能超过32位极限
- The total size of a single file and all files cannot exceed the 32-bit limit

---

## 库和源码用途说明

## Library and source purpose

- `minipack_writer`（库）

- `minipack_writer` (library)

  - 提供 `MiniPackBuilder` 和写入器接口（`MiniPackWriter`），负责在内存中构建条目并将最终的 header/info 与数据写入目标（例如文件或内存）。

  - Provides `MiniPackBuilder` and writer interfaces (`MiniPackWriter`) to build entries in memory and write the final header/info and data to a target (e.g., file or memory).

  - 包含 `mini_pack_builder_file.cpp` 中的便利函数，用于从磁盘加载文件数据并把它们作为条目添加到 `MiniPackBuilder`。

  - Includes helper functions in `mini_pack_builder_file.cpp` to load file data from disk and add entries to `MiniPackBuilder`.

---

- `minipack_reader`（库）

- `minipack_reader` (library)

  - 提供加载和解析 `.pack` 的功能（例如读入 info 块并构建索引），供消费端查询包内文件列表与元数据。

  - Provides functionality to load and parse `.pack` files (e.g., read the info block and build an index) so consumers can query file lists and metadata.

  - 包内文件名为 UTF-8 存储，读取依据“长度表 + NUL 结尾名称区”的新布局解析。

  - Filenames in packs are stored as UTF-8 and parsed according to the new "length table + NUL-terminated names area" layout.

---

- `minipack_utf`（库）

- `minipack_utf` (library)

  - 提供 UTF 编码/转换的实现（例如 UTF-8/16/32 的互转），以及与平台相关的编码抽象（Windows/Posix 实现）。

  - Provides UTF encoding/conversion utilities (e.g., UTF-8/16/32 conversions) and platform-specific encoding abstractions (Windows/Posix implementations).

  - 主要用于处理外部文本输入（如文件列表、平台代码页等），与包内名称读取解耦。

  - Primarily used for external text inputs (e.g., file lists, platform code pages), decoupled from reading names inside packs.

---

- 可执行文件相关源

- Executable-related sources

  - `file_list_reader.cpp`：实现从 `list.txt` 中读取文件路径的逻辑（用于命令行模式）。

  - `file_list_reader.cpp`: Implements reading file paths from `list.txt` (used by the CLI).

  - `dir_scan.cpp`：实现递归扫描目录并产生 `(disk_path, stored_name)` 对，仅由可执行程序 `MiniPack` 使用，不被静态库依赖。

  - `dir_scan.cpp`: Implements recursive directory scanning producing `(disk_path, stored_name)` pairs; used only by the `MiniPack` executable and not by the static libraries.

  - `mini_pack_writer_file.cpp` / `mini_pack_writer_vector.cpp`：提供将最终包写入文件或内存缓冲区的具体 `MiniPackWriter` 实现。

  - `mini_pack_writer_file.cpp` / `mini_pack_writer_vector.cpp`: Provide concrete `MiniPackWriter` implementations to write the final pack to a file or an in-memory buffer.

---

## 开发与贡献

## Development & Contribution

欢迎提交 issue 或 pull request。请在提交前确保代码在主流平台上能通过构建。

Issues and pull requests are welcome. Please ensure the code builds on mainstream platforms before submitting.

---

## 许可证

## License

本项目采用 BSD 3-Clause 许可证发布，详见仓库根目录的 `LICENSE` 文件。

This project is released under the BSD 3-Clause License; see the `LICENSE` file in the repository root for the full text.

这个库过于简单，所以我们不关心您用它做什么，也不对任何使用该库的结果负责。您使用它产生的任何问题都与我们无关，请自行承担风险。

This library is very simple, so we do not care what you do with it and are not responsible for any results of using it. Any issues arising from your use of it are your own responsibility; use at your own risk.

---

## 联系方式

## Contact

如需帮助，请在仓库中打开 issue。

If you need help, please open an issue in the repository.