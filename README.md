# Ubuntu_For_Xiaomi5_Gemini

[酷安/Coolapk 个人主页](https://www.coolapk.com/u/8875247)

## Release 中的懒人包

在仓库的`Release`中提供构建好的懒人包，下载解压后点击脚本刷入即可

理论上无底包固件限制。

[GUIDE.md](docs/how_to_use.md) 该文档是对懒人包的一部分说明，以及如何使用putty和webssh连接到Ubuntu上。

[CHANGELOG.md](docs/CHANGELOG.md) 该文档是历史更新日志

> **懒人包第三方网盘链接（长期有效）**
>
> 百度网盘：[链接](https://pan.baidu.com/s/13Tp68LbJ1g0AP9CqwEHAAQ?pwd=m9rg)，提取码：m9rg
>
>123盘： [链接](https://www.123865.com/s/FUIyVv-zybP3?pwd=FTgO#)，提取码：FTgO
>
>天翼盘： [链接](https://cloud.189.cn/web/share?code=mmiEZvbuMRJf)，访问码：j6hh


## 特性

已支持：

```text
 wifi
 OTG
 热点   # 硬件限制热点与wifi无法同时打开
 蓝牙   # 含鼠标键盘等
 触摸   # 三大金刚键不支持
 音频   # 仅otg小尾巴有声音，扬声器与3.5mm不被支持
 rndis驱动
 tty串口登录
 tty串口自动登录
 zram   # 默认开启与ram对等大小的zram 
 docker # 未预装docker软件包
```

另已支持：`rmi4`与`atmel`两种触控 ic，以及 `jdi`与`lgd`两种屏幕。

## Ubuntu 24.04 构建

 构建文档：[BUILDING.md](docs/BUILDING.md)

> 你可以根据文档自行编译linux内核，自行选择心仪的操作系统。

## 引用

- linux 主线仓库：[Gitlab仓库](https://gitlab.com/msm8996-mainline/linux)

- firmware固件来自 [Gitee仓库](https://gitee.com/meiziyang2023/firmware-postmarketos-xiaomi-gemini) 与 umeiko大佬

- [BUGFIX.md](docs/gemini.note.md) 出自 umeiko佬的仓库：[KlipperPhonesLinux](https://github.com/umeiko/KlipperPhonesLinux)

- 构建参考文档：[博客文档](https://www.cnblogs.com/holdmyhand/articles/18048158)
