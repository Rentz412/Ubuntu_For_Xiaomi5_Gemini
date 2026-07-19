# 小米 5（gemini）Ubuntu 24.04 ARM64 构建参考

本文档用于在 Ubuntu 24.04 x86_64 主机或 WSL2 中构建小米 5（代号 `gemini`）所需的 ARM64 内核、Debian 软件包、Ubuntu rootfs 与 Android `boot.img`。

所有工作目录统一放在当前用户的 home 目录下：

```text
~/Ubuntu_For_Xiaomi5_Gemini   本仓库
~/chroot                      ARM64 rootfs 挂载目录
~/rootfs-noble.img            Ubuntu 24.04 rootfs 原始镜像
```

内核构建缓存、最终构建成果和 boot 打包目录位于仓库内部：

```text
~/Ubuntu_For_Xiaomi5_Gemini/.build   内核构建缓存和失败日志
~/Ubuntu_For_Xiaomi5_Gemini/out      Image.gz、DTB 和 DEB
~/Ubuntu_For_Xiaomi5_Gemini/boot     boot.img、rootfs.img 和打包中间文件
```

---

## 1. 安装 Ubuntu 24.04 构建依赖

启用软件源并安装内核、ARM64 rootfs、Debian 打包和 Android 镜像工具：

```bash
sudo apt update
sudo apt install -y \
  build-essential bc bison flex pkg-config \
  libncurses-dev libssl-dev libelf-dev \
  gcc-aarch64-linux-gnu \
  fakeroot dpkg-dev debhelper \
  pahole device-tree-compiler \
  qemu-user-static binfmt-support debootstrap \
  python3 ca-certificates curl git \
  unzip xz-utils zstd lz4 cpio rsync kmod file \
  e2fsprogs util-linux \
  android-sdk-libsparse-utils fastboot
```

---

## 2. 克隆仓库：

```bash
cd ~
git clone https://github.com/Rentz412/Ubuntu_For_Xiaomi5_Gemini.git
cd ~/Ubuntu_For_Xiaomi5_Gemini
```

创建环境变量文件：

```bash
cat > "$HOME/env.sh" <<'EOF'
export REPO="$HOME/Ubuntu_For_Xiaomi5_Gemini"
export KERNEL_SRC="$REPO/linux"
export KERNEL_BUILD="$REPO/.build/kernel"
export KERNEL_OUT="$REPO/out"

export ROOTFS_IMG="$HOME/rootfs-noble.img"
export ROOTFS_MNT="$HOME/chroot"

export BOOT_DIR="$REPO/boot"
export BOOT_WORK="$BOOT_DIR/work"
export MKBOOTIMG_DIR="$BOOT_DIR/mkbootimg"

export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
export CC=aarch64-linux-gnu-gcc
EOF
```

应用环境变量：

```bash
source "$HOME/env.sh"
mkdir -p "$ROOTFS_MNT" "$BOOT_DIR"
```

后续每次打开新终端，都应先执行：

```bash
source "$HOME/env.sh"
```

---

## 3. 准备 mkbootimg 工具

```bash
source "$HOME/env.sh"

mkdir -p "$MKBOOTIMG_DIR"

curl -L --fail --silent --show-error \
  'https://android.googlesource.com/platform/system/tools/mkbootimg/+archive/d2bb0af5ba6d3198a3e99529c97eda1be0b5a093.tar.gz' \
  | tar -xz -C "$MKBOOTIMG_DIR"
```

确认工具存在：

```bash
test -f "$MKBOOTIMG_DIR/mkbootimg.py"
test -f "$MKBOOTIMG_DIR/unpack_bootimg.py"
```

---

## 4. 构建内核、DTB 和 Debian 包

加载环境变量并准备构建目录：

```bash
source "$HOME/env.sh"

mkdir -p "$KERNEL_BUILD" "$KERNEL_OUT" "$BOOT_DIR"
```

复制仓库内的 Gemini 配置，并补全当前内核源码新增的配置项：

```bash
cp "$REPO/kernel/6.3.1-gemini-Ubuntu.config" "$KERNEL_BUILD/.config"

make -C "$KERNEL_SRC" O="$KERNEL_BUILD" \
  ARCH="$ARCH" CROSS_COMPILE="$CROSS_COMPILE" CC="$CC" \
  olddefconfig
```

读取实际将要编译的内核版本：

```bash
KERNEL_RELEASE="$(make -s -C "$KERNEL_SRC" O="$KERNEL_BUILD" \
  ARCH="$ARCH" CROSS_COMPILE="$CROSS_COMPILE" CC="$CC" \
  kernelrelease)"

printf '%s\n' "$KERNEL_RELEASE" | tee "$BOOT_DIR/kernel-release.txt"
```

编译 `Image.gz` 和两个 Gemini DTB：

```bash
make -C "$KERNEL_SRC" O="$KERNEL_BUILD" \
  ARCH="$ARCH" CROSS_COMPILE="$CROSS_COMPILE" CC="$CC" \
  -j"$(nproc)" \
  Image.gz \
  qcom/msm8996-xiaomi-gemini.dtb \
  qcom/msm8996-xiaomi-gemini-lgd-td4322.dtb
```

生成 Debian 软件包。先删除构建目录中以前遗留的 DEB，避免与本次结果混淆：

```bash
rm -f "$REPO/.build/"*.deb

make -C "$KERNEL_SRC" O="$KERNEL_BUILD" \
  ARCH="$ARCH" CROSS_COMPILE="$CROSS_COMPILE" CC="$CC" \
  -j"$(nproc)" bindeb-pkg
```

整理本次构建成果到 `out/`：

```bash
install -m 0644 \
  "$KERNEL_BUILD/arch/arm64/boot/Image.gz" \
  "$KERNEL_OUT/Image.gz"

install -m 0644 \
  "$KERNEL_BUILD/arch/arm64/boot/dts/qcom/msm8996-xiaomi-gemini.dtb" \
  "$KERNEL_OUT/msm8996-xiaomi-gemini.dtb"

install -m 0644 \
  "$KERNEL_BUILD/arch/arm64/boot/dts/qcom/msm8996-xiaomi-gemini-lgd-td4322.dtb" \
  "$KERNEL_OUT/msm8996-xiaomi-gemini-lgd-td4322.dtb"

mv "$REPO/.build/"*.deb "$KERNEL_OUT/"
```

---

## 5. 创建 Ubuntu 24.04 ARM64 rootfs

以下操作会重新格式化 `$ROOTFS_IMG`，如文件已经存在，请先备份。

创建 5 GiB ext4 镜像：

> 可依据实际情况自行调整镜像大小 

```bash
source "$HOME/env.sh"

truncate -s 5G "$ROOTFS_IMG"
sudo mkfs.ext4 -F -L ubuntu-rootfs "$ROOTFS_IMG"

mkdir -p "$ROOTFS_MNT"
sudo mount -o loop "$ROOTFS_IMG" "$ROOTFS_MNT"
```

记录 rootfs UUID：

```bash
ROOT_UUID="$(sudo blkid -s UUID -o value "$ROOTFS_IMG")"
printf '%s\n' "$ROOT_UUID" | tee "$BOOT_DIR/rootfs-uuid.txt"
```

在主机上执行 debootstrap ：

```bash
sudo debootstrap --arch arm64 focal "$ROOTFS_MNT" https://mirrors.aliyun.com/ubuntu-ports
```

---

## 6. 挂载并进入 chroot

准备 chroot DNS：

```bash
sudo rm -f "$ROOTFS_MNT/etc/resolv.conf"
sudo cp /etc/resolv.conf "$ROOTFS_MNT/etc/resolv.conf"
```

挂载必要文件系统：

```bash
sudo mount --bind /proc "$ROOTFS_MNT/proc"
sudo mount --bind /dev "$ROOTFS_MNT/dev"
sudo mount --bind /dev/pts "$ROOTFS_MNT/pts"
sudo mount --bind /sys "$ROOTFS_MNT/sys"
```

进入 ARM64 chroot：

```bash
sudo chroot "$ROOTFS_MNT"
```

---

## 7. 配置 Ubuntu 24.04 rootfs

以下命令在 chroot 内执行。

配置清华源：

```bash
cat > /etc/apt/sources.list.d/ubuntu.sources <<'EOF'
Types: deb
URIs: https://mirrors.tuna.tsinghua.edu.cn/ubuntu-ports
Suites: noble noble-updates noble-backports
Components: main restricted universe multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF

rm -f /etc/apt/sources.list
apt update && apt upgrade
```

安装基础软件：

```bash
apt install man man-db bash-completion vim tmux network-manager chrony openssh-server initramfs-tools rfkill nano --no-install-recommends -y
```

配置语言、时区和 hostname：

```bash
locale-gen en_US.UTF-8
locale-gen zh_CN.UTF-8

rm /etc/localtime
ln -s /usr/share/zoneinfo/Asia/Shanghai /etc/localtime

echo 'xiaomi-5' > /etc/hostname
```

创建用户：

```bash
useradd -m -s /bin/bash gemini
usermod -aG sudo gemini
```

设置密码：

```bash
passwd gemini
```

卸载netplan：

```bash
apt purge netplan.io
```

退出 chroot：

```bash
exit
```

写入 `fstab`：

```bash
ROOT_UUID="$(cat "$BOOT_DIR/rootfs-uuid.txt")"

printf 'UUID=%s / ext4 defaults,noatime 0 1\n' "$ROOT_UUID" \
  | sudo tee "$ROOTFS_MNT/etc/fstab"
```

---

## 8. 安装自编译内核

只复制 `linux-image` 和可选的 `linux-headers`，不要复制 `linux-libc-dev`：

```bash
source "$HOME/env.sh"

sudo cp "$KERNEL_OUT"/linux-image-*.deb "$ROOTFS_MNT/tmp/"
sudo cp "$KERNEL_OUT"/linux-headers-*.deb "$ROOTFS_MNT/tmp/" 2>/dev/null || true
```

进入 chroot：

```bash
sudo chroot "$ROOTFS_MNT"
```

安装内核：

```bash
cd /tmp
dpkg -l | grep -E "linux-headers|linux-image" |awk '{print $2}'|xargs dpkg -P
rm -rf /lib/modules/*
dpkg -i linux*.deb

sed -i 's/^MODULES=.*/MODULES=most/' \
  /etc/initramfs-tools/initramfs.conf

update-initramfs -u -k all
```

检查安装结果：

```bash
dpkg --get-selections | grep linux
ls /lib/modules
```

退出 chroot：

```bash
exit
```

确认目标 initramfs 存在：

```bash
KERNEL_RELEASE="$(cat "$BOOT_DIR/kernel-release.txt")"
test -f "$ROOTFS_MNT/boot/initrd.img-$KERNEL_RELEASE"
```

---

## 9. 拷贝firmware文件

仓库固件目录：

```text
~/Ubuntu_For_Xiaomi5_Gemini/firmware
```

复制到 rootfs：

```bash
sudo mkdir -p "$ROOTFS_MNT/usr/lib/firmware"
sudo cp -a "$REPO/firmware/*" "$ROOTFS_MNT/usr/lib/firmware/"
```

在chroot环境中执行：

```bash
sudo chroot "$ROOTFS_MNT"
```

```bash
cd /usr/lib/firmware/
ldconfig
```

---

## 10. 自动扩展文件系统

进入chroot：

```bash
sudo chroot "$ROOTFS_MNT"
```

创建服务：

```bash
cat > /etc/systemd/system/resizefs.service <<'EOF'
[Unit]
Description=Expand root filesystem
After=systemd-remount-fs.service

[Service]
Type=oneshot
ExecStart=/bin/bash -c '/sbin/resize2fs "$(findmnt -n -o SOURCE /)"'
ExecStartPost=/bin/systemctl disable resizefs.service
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
EOF
```

开启服务：
```bash
systemctl enable resizefs.service
exit
```

---

## 11. 启用 ttyGS0 串口登录

进入 chroot：

```bash
sudo chroot "$ROOTFS_MNT"
```

创建服务：

```bash
cat > /etc/systemd/system/serial-getty@ttyGS0.service <<'EOF'
[Unit]
Description=Serial Console Service on ttyGS0
After=systemd-user-sessions.service

[Service]
ExecStart=-/sbin/agetty -L 115200 ttyGS0 xterm-256color
Type=idle
Restart=always
RestartSec=1

[Install]
WantedBy=multi-user.target
EOF

systemctl enable serial-getty@ttyGS0.service
exit
```

仅当内核使用 `g_serial` 且没有其他 USB Gadget 配置时加入：

```bash
echo g_serial | sudo tee -a "$ROOTFS_MNT/etc/modules"
```

---

## 12. 在仓库根目录打包 boot.img

所有 boot 打包文件都放在：

```text
~/Ubuntu_For_Xiaomi5_Gemini/boot
```

准备工作目录：

```bash
source "$HOME/env.sh"

rm -rf "$BOOT_WORK"
mkdir -p "$BOOT_WORK"
```

选择要使用的 DTB。

标准 JDI：

```bash
DTB_JDI=msm8996-xiaomi-gemini.dtb
```

LGD TD4322：

```bash
DTB_LGD=msm8996-xiaomi-gemini-lgd-td4322.dtb
```

复制内核、DTB 和 initramfs：

```bash
KERNEL_RELEASE="$(cat "$BOOT_DIR/kernel-release.txt")"

install -m 0644 "$KERNEL_OUT/Image.gz" "$BOOT_WORK/Image.gz"
install -m 0644 "$KERNEL_OUT/$DTB_JDI" "$BOOT_WORK/dtb-jdi"
install -m 0644 "$KERNEL_OUT/$DTB_LGD" "$BOOT_WORK/dtb-lgd"

sudo install -m 0644 \
  "$ROOTFS_MNT/boot/initrd.img-$KERNEL_RELEASE" \
  "$BOOT_WORK/initrd.img"
```

拼接内核和 DTB：

```bash
cat "$BOOT_WORK/Image.gz" "$BOOT_WORK/dtb-jdi" \
  > "$BOOT_WORK/kernel-dtb-jdi"
```

```bash
cat "$BOOT_WORK/Image.gz" "$BOOT_WORK/dtb-lgd" \
  > "$BOOT_WORK/kernel-dtb-lgd"
```

生成 boot ：

```bash
ROOT_UUID="$(cat "$BOOT_DIR/rootfs-uuid.txt")"

python3 "$MKBOOTIMG_DIR/mkbootimg.py" \
  --header_version 0 \
  --base 0x80000000 \
  --kernel_offset 0x00008000 \
  --ramdisk_offset 0x01000000 \
  --tags_offset 0x00000100 \
  --pagesize 4096 \
  --second_offset 0x00f00000 \
  --ramdisk "$BOOT_WORK/initrd.img" \
  --cmdline "console=tty0 root=UUID=$ROOT_UUID rw loglevel=3 splash" \
  --kernel "$BOOT_WORK/kernel-dtb-jdi" \
  --output "$BOOT_DIR/boot-jdi.img"

python3 "$MKBOOTIMG_DIR/mkbootimg.py" \
  --header_version 0 \
  --base 0x80000000 \
  --kernel_offset 0x00008000 \
  --ramdisk_offset 0x01000000 \
  --tags_offset 0x00000100 \
  --pagesize 4096 \
  --second_offset 0x00f00000 \
  --ramdisk "$BOOT_WORK/initrd.img" \
  --cmdline "console=tty0 root=UUID=$ROOT_UUID rw loglevel=3 splash" \
  --kernel "$BOOT_WORK/kernel-dtb-lgd" \
  --output "$BOOT_DIR/boot-jdi.lgd"
```

---

## 13. 卸载 rootfs 并生成 Android sparse 镜像

清理 rootfs：

```bash
sudo chroot "$ROOTFS_MNT" /bin/bash -c 'apt clean; rm -rf /tmp/*'
```

卸载绑定目录和 rootfs：

```bash
sudo umount "$ROOTFS_MNT/proc"
sudo umount "$ROOTFS_MNT/sys"
sudo umount "$ROOTFS_MNT/dev/pts"
sudo umount "$ROOTFS_MNT/dev"
sudo umount "$ROOTFS_MNT"
```

转换为 Android sparse 镜像：

```bash
img2simg "$ROOTFS_IMG" "$BOOT_DIR/rootfs.img"
```

检查文件：

```bash
file "$BOOT_DIR/boot.img"
file "$BOOT_DIR/rootfs.img"
```
