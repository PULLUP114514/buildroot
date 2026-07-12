# epass 共享字体 - 产品字表

`epass-fonts` 包在 host 侧子集化时读取这里的字表,决定思源黑/宋保留哪些字、
FontAwesome 保留哪些图标码点。产物装到设备 `/usr/share/fonts/epass/`。

```
fonts/
├─ charset/          # 思源黑/宋要保留的字 (逐字符, 目录下所有 *.txt 取并集)
│  ├─ common.txt     #   常用字 + 标点
│  ├─ operators.txt  #   方舟干员名用字
│  └─ literals.txt   #   drm_app_neo 源码里出现的中日文/全角字面量
└─ icons.txt         # FontAwesome/LV_SYMBOL 图标码点 (十六进制, 每行一个)
```

假名 / 各类标点 / 全角等 CJK 排版基础区间由子集脚本固定内建,不用列在这里。

## 更新字表

字表由 `drm_app_neo/tools/font_generate/export_board_charset.py` 从各程序源码
(+ 可选 `character_table.json` 干员表) 重新导出到本目录。换字体、加新 UI 文案 /
新图标后重跑一次并提交,再重建 `epass-fonts` 包即可。
