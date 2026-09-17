#!/usr/bin/env python3
"""生成 compile_commands.json（clangd 用），不依赖 bear。

用法：python3 tools/gen_compdb.py <输出文件> <编译器> <编译开关> <源文件>...
内容不变时不重写，避免 clangd 每次构建都重建索引。
"""

import json
import os
import pathlib
import shlex
import sys

out, cc, cflags, *sources = sys.argv[1:]

root = os.getcwd()
flags = shlex.split(cflags)
entries = [
    {
        "directory": root,
        "file": os.path.join(root, src),
        "arguments": [cc, *flags, "-c", os.path.join(root, src)],
    }
    for src in sources
]
text = json.dumps(entries, indent=2) + "\n"

path = pathlib.Path(out)
if not path.exists() or path.read_text() != text:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    print(f"  CDB     {out}")
