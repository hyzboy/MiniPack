# MiniPack

轻量级的文件打包工具和库（C++20）。

MiniPack 提供：

- 将多个文件打包为单个 `.pack` 文件的可执行程序（`MiniPack`）。
- 读写 pack 的核心库（`minipack_writer`、`minipack_reader`、`minipack_utf`）。
- 支持从文件列表或直接递归打包目录（在可执行文件中通过 `dir_scan.cpp` 实现）。

主要特点

- 纯 C++20 实现，可移植到 Windows / POSIX 平台。
- 支持将文件名以相对路径的形式存储到包内。
- 可选择只写索引（info block），不写数据（`--index-only`）。
- 可选使用静态 C 运行时（CMake 选项 `USE_STATIC_CRT`）。

快速开始（使用 CMake）

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
# 可执行文件位于 build/bin/MiniPack
```

说明：CMake 选项 `USE_STATIC_CRT` 默认为 `ON`，用于选择是否链接静态运行时（MSVC 使用 `/MT`，其它工具链尝试 `-static-libgcc -static-libstdc++`）。

用法示例

- 通过文件列表打包：
  `MiniPack list.txt output.pack`
  其中 `list.txt` 每行一个要打包的文件路径。

- 直接打包目录：
  `MiniPack path/to/directory output.pack`
  将递归枚举目录下的所有常规文件，存储名为相对于目录的路径。

- 只写索引（不写数据）：
  `MiniPack path/to/directory output.pack --index-only`

文件列表格式

- 每行一个文件路径。
- 空行或以 `#` 开头的行将被忽略（可用于注释）。

MiniPack 文件结构（概要）

下面用表格形式给出 `.pack` 文件开头各部分的概要说明：

| 字段 | 偏移 / 大小 | 描述 |
|---|---:|---|
| Magic | 0 (8 bytes) | ASCII `"MINIPACK"`，用于识别文件类型。 |
| InfoSize | 8 (4 bytes, little-endian uint32) | 表示紧随其后的 info 块长度（字节数）。 |
| Info Block | 12 (InfoSize bytes) | 包含包级元数据与每个文件的条目（详见下方“Info Block 布局”表）。 |
| Data Area | 12 + InfoSize (variable) | 可选，紧跟 info 块后的各文件原始字节流，按条目顺序拼接。若使用 `--index-only` 则不存在数据区。 |

该结构使得读取程序仅通过读取开头的 info 块即可获取目录信息，而不必逐个读取文件数据。

Info Block 布局（相对于 Info Block 起始偏移）

下面的表格详细列出 info 块头部与每个文件条目的字段（所有多字节整数采用小端 little-endian）：

| Offset (in info) | Size | Field | 描述 |
|---:|:---:|---|---|
| 0 | 4 | `version` | uint32，包格式版本。 |
| 4 | 4 | `file_count` | uint32，包内文件总数。 |
| 8 | 1 | `name_encoding` | uint8，文件名编码枚举（例如 0 = UTF-8）。 |
| 9 | var | `reserved` | 可选的保留/对齐字节，直到 entries 开始。 |

每个文件条目（按 `file_count` 重复，条目间紧密排列）：

| Entry Offset (relative to entry start) | Size | Field | 描述 |
|---:|:---:|---|---|
| 0 | 4 | `name_length` | uint32，名字字节长度（不包含 NUL）。 |
| 4 | name_length | `name` | 文件名的字节流，使用 `name_encoding` 指定的编码。 |
| 4 (after name) | 4 | `data_length` | uint32，该文件的数据长度（字节）。 |
| 4 | 4 | `data_offset` | uint32，指向数据区内该文件数据的偏移（相对于数据区起始）。 |

Notes:

- `data_offset` 指向紧随 info 块之后的数据区内的位置（即数据区的起始为文件中全局偏移 `12 + InfoSize`）。
- 文件名以长度前缀存储（不以 NUL 结束）。
- 许多整数字段为 32 位，因此单个文件大小或 info 大小有 32 位限制。

二进制布局示意（字节偏移表）

下面给出一个典型 `.pack` 文件的字节偏移示意（概览）：

| Offset | Size | Description |
|------:|:----:|---|
| 0 | 8 | Magic: ASCII "MINIPACK" |
| 8 | 4 | InfoSize = uint32 (length of the info block in bytes) |
| 12 | InfoSize | Info block (参见 Info Block 布局表) |
| 12 + InfoSize | (var) | Data area (optional): concatenated file data in entry order |

Notes:

- `data_offset` values point into the data area that follows immediately after the info block.
- Name bytes are stored using the declared name encoding; filenames are not NUL-terminated (length-prefixed).
- Many integer fields are 32-bit and therefore limits apply (e.g. individual file size must fit in uint32).

库和源码用途说明

- `minipack_writer`（库）
  - 提供 `MiniPackBuilder` 和写入器接口（`MiniPackWriter`），负责在内存中构建条目并将最终的 header/info 与数据写入目标（例如文件或内存）。
  - 包含 `mini_pack_builder_file.cpp` 中的便利函数，用于从磁盘加载文件数据并把它们作为条目添加到 `MiniPackBuilder`。

- `minipack_reader`（库）
  - 提供加载和解析 `.pack` 的功能（例如读入 info 块并构建索引），供消费端查询包内文件列表与元数据。
  - 该库依赖于 `minipack_utf` 以便正确处理不同编码的文件名。

- `minipack_utf`（库）
  - 提供 UTF 编码/转换的实现（例如 UTF-8/16/32 的互转），以及与平台相关的编码抽象（Windows/Posix 实现）。
  - 仅当需要处理跨编码的文件名输入/输出时才会被其它库使用。

- 可执行文件相关源
  - `file_list_reader.cpp`：实现从 `list.txt` 中读取文件路径的逻辑（用于命令行模式）。
  - `dir_scan.cpp`：实现递归扫描目录并产生 `(disk_path, stored_name)` 对，仅由可执行程序 `MiniPack` 使用，不被静态库依赖。
  - `mini_pack_writer_file.cpp` / `mini_pack_writer_vector.cpp`：提供将最终包写入文件或内存缓冲区的具体 `MiniPackWriter` 实现。

开发与贡献

欢迎提交 issue 或 pull request。请在提交前确保代码在主流平台上能通过构建。

许可证

项目中未包含特定许可证信息。请在仓库根目录添加 `LICENSE` 文件以明确许可条款。

联系方式

如需帮助，请在仓库中打开 issue。