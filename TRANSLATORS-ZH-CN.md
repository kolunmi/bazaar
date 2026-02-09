# 翻译者指南

感谢你对翻译 Bazaar 的兴趣！🏷️🗺️💜

一些基本规则：
* 你必须精通你所贡献的语言。
* **严禁**使用 LLM 生成译文。如果你这样做，你将被项目作者禁止参与此项目。
* flathub.org 本身就有良好的中文支持，因此 Bazaar 里 Flathub 页面的翻译应当和 flathub.org 高度一致。
即使 flathub.org 中个别翻译可能并不恰当，也请直接采用 flathub.org 的原文。
例如 https://flathub.org/zh-Hans/apps/category/game/subcategories/ArcadeGame 里将 Arcade 翻译为“游乐中心”，虽然实际上是“街机”更合适，但我们依旧选择直接采用“游乐中心”作为翻译
* 统一 Uninstall 翻译为“卸载”而非“删除”
根据：
```bash
❯ flatpak uninstall --help
用法：
  flatpak uninstall [选项…] [引用…] - 卸载应用程序或运行时
...
```

## 基本流程

Fork 本项目（以便稍后开启 PR）并克隆仓库。然后确保你的当前目录位于 Bazaar 项目的根目录：

```sh
# 将 '...' 替换为你拥有写入权限的 Bazaar Fork 的 URL
git clone ...
cd bazaar
```

# 自动设置

完成上述操作后，你可以运行 `./translators.sh` 并按照屏幕上的提示进行操作。该脚本会显示当前 `po/LINGUAS` 的内容。如果一切正确，输入 `Y` 并按回车。随后脚本会要求你输入语言代码，请输入并回车。脚本现在将生成一个新的 `po` 文件或更新现有文件，以便所有新的待翻译字符串都可用。

现在，你可以使用文本编辑器或翻译编辑器（POEdit、GTranslator、Lokalize 等）打开 `po` 文件并开始翻译。完成后，提交你的更改并在 GitHub 上提交 Pull Request（PR）。

# 手动设置

完成克隆后，使用 Meson 设置项目，并将 `im_a_translator` 标志设为 `true`：

```sh
meson setup build -Dim_a_translator=true
```

接下来，进入构建目录：

```sh
cd build
```

运行以下命令生成主 `pot`（**P**ortable **O**bject **T**emplate）文件：

```sh
meson compile bazaar-pot
```

你可能会看到一堆警告 `blp` 扩展名未知的输出，可以安全忽略它。

最后，仍然在构建目录内，运行以下命令来更新和/或创建 `po`（**P**ortable **O**bject）文件：

```sh
meson compile bazaar-update-po
```

现在，你可以使用文本编辑器或翻译编辑器（[POEdit](https://flathub.org/apps/net.poedit.Poedit)、[GTranslator](https://flathub.org/apps/org.gnome.Gtranslator)、[Lokalize](https://flathub.org/apps/org.kde.lokalize) 等）打开 `po` 文件并开始翻译。
完成后，提交你的更改并在 GitHub 上提交 PR。请确保只提交与你的翻译相关的文件。

## 更新现有翻译

根据上述命令再次生成全新的 `.pot` 文件（如有必要）。

```sh
msgmerge --update --verbose po/zh_CN.po po/bazaar.pot
```

请在 PR 中将上述更新步骤作为一个单独的提交（Commit），以便于代码审查。谢谢！

## 测试你的翻译

确保使用简体中文对应的[语言代码](https://en.wikipedia.org/wiki/List_of_ISO_639_language_cozh_CNs)"zh_CN"！

```sh
msgfmt po/zh_CN.po -o bazaar.mo
```

复制 `.mo` 文件，使 Bazaar 能够识别它：

```sh
sudo cp bazaar.mo /var/lib/flatpak/runtime/io.github.kolunmi.Bazaar.Locale/x86_64/stable/active/files/zh/share/zh_CN/LC_MESSAGES/
```

确保先结束 Bazaar 的后台进程，以便应用所需的更改/语言。

```sh
killall bazaar
```

覆盖使用的语言并启动 Bazaar：

```sh
LANGUAGE=zh_CN flatpak run io.github.kolunmi.Bazaar
```

# 翻译者注意事项

自动和手动流程都可能生成标记为 `fuzzy`（模糊）的条目。这意味着对于这些条目，`gettext` 尝试从现有的翻译中推导结果。某些翻译软件（如 Lokalize）会利用此标记将字符串设为“未审核”，并在条目被标记为完成时移除标记。如果你使用文本编辑器处理 pot 文件，请务必手动删除你认为已完成条目的 `fuzzy` 标记，否则你的翻译将不会出现在 Bazaar 中。
