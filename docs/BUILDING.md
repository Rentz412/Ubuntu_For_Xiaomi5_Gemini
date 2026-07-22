# 小米 5（gemini）Ubuntu 24.04 ARM64 构建参考

环境: `Ubuntu 24.04 x86_64 主机`或 `Windows 环境下的 WSL2 Ubuntu 24.04 `

本仓库使用主线为6.3.1的msm8996仓库：[Gitlab链接](https://gitlab.com/msm8996-mainline/linux)，并在原仓库内容上针对小米5修复了一部分问题

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
cat > env.sh << EOF
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
export CC=aarch64-linux-gnu-gcc
EOF
```

## 3. 构建内核、DTB 和 Debian 包

加载环境变量并准备构建目录：

```bash
cd ~/Ubuntu_For_Xiaomi5_Gemini/
source env.sh
mkdir -p ~/chroot
```

复制仓库内的 Gemini 配置，并补全当前内核源码新增的配置项：

```bash
cp ./kernel-config/6.3.1-gemini-Ubuntu.config ./linux/.config
cd linux
make menuconfig
```

编译内核和两个 DTB：

```bash
make \
  ARCH="$ARCH" CROSS_COMPILE="$CROSS_COMPILE" CC="$CC" \
  -j"$(nproc)" \
  Image.gz \
  qcom/msm8996-xiaomi-gemini.dtb \
  qcom/msm8996-xiaomi-gemini-lgd-td4322.dtb
```

生成 Debian 软件包：

```bash
make ARCH="$ARCH" CROSS_COMPILE="$CROSS_COMPILE" CC="$CC" -j"$(nproc)" bindeb-pkg
```

```bash
cp ./arch/arm64/boot/Image.gz ~/Ubuntu_For_Xiaomi5_Gemini/boot/Image.gz
cp ./arch/arm64/boot/dts/qcom/msm8996-xiaomi-gemini.dtb ~/Ubuntu_For_Xiaomi5_Gemini/boot/jdi.dtb
cp ./arch/arm64/boot/dts/qcom/msm8996-xiaomi-gemini-lgd-td4322.dtb ~/Ubuntu_For_Xiaomi5_Gemini/boot/lgd.dtb
rm ~/Ubuntu_For_Xiaomi5_Gemini/linux-libc*
rm ~/Ubuntu_For_Xiaomi5_Gemini/linux-upstream*
```

---

## 4. 创建 Ubuntu 24.04 ARM64 rootfs

以下操作会重新格式化 `~/rootfs-noble.img`，如文件已经存在，请先备份。

创建 5 GiB ext4 镜像：

> 可依据实际情况自行调整镜像大小 

```bash
dd if=/dev/zero of=rootfs-noble.img bs=1G count=5
mkfs.ext4 rootfs-noble.img
```

记录 rootfs UUID：

```bash
ROOT_UUID="$(sudo blkid -s UUID -o value "~/rootfs-noble.img")"
printf '%s\n' "$ROOT_UUID" | tee ~/Ubuntu_For_Xiaomi5_Gemini/boot/rootfs-uuid.txt
```

在主机上执行 debootstrap ：

```bash
sudo debootstrap --arch arm64 noble ~/chroot https://mirrors.aliyun.com/ubuntu-ports
```

---

## 5. 挂载 chroot

```bash
sudo mount ~/rootfs-noble.img ~/chroot
```

```bash
sudo mount --bind /proc ~/chroot/proc
sudo mount --bind /dev ~/chroot/dev
sudo mount --bind /dev/pts ~/chroot/dev/pts
sudo mount --bind /sys ~/chroot/sys
```

---

## 6. 配置 Ubuntu 24.04 rootfs

以下命令在 chroot 内执行，现在进入 chroot：

```bash
sudo chroot ~/chroot
```

配置清华源：

```bash
cat > /etc/apt/sources.list.d/ubuntu.sources << EOF
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

---

## 7. 安装自编译内核

```bash
sudo cp ~/Ubuntu_For_Xiaomi5_Gemini/linux-image-*.deb ~/chroot/tmp/
sudo cp ~/Ubuntu_For_Xiaomi5_Gemini/linux-headers-*.deb ~/chroot/tmp/
```

进入 chroot：

```bash
sudo chroot "~/chroot"
```

安装内核：

```bash
cd /tmp
dpkg -l | grep -E "linux-headers|linux-image" |awk '{print $2}'|xargs dpkg -P
rm -rf /lib/modules/*
dpkg -i linux*.deb
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

---

## 8. 拷贝firmware文件


复制到 rootfs：

```bash
sudo mkdir -p "~/chroot/usr/lib/firmware"
sudo cp -a "~/Ubuntu_For_Xiaomi5_Gemini/firmware/*" "~/chroot/usr/lib/firmware/"
```

在chroot环境中执行：

```bash
sudo chroot "~/chroot"
```

```bash
cd /usr/lib/firmware/
ldconfig
```

---

## 9. 自动扩展文件系统

进入chroot：

```bash
sudo chroot ~/chroot
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
```

> 每次安装内核后该服务有可能会被关闭，注意检查

---

## 10. 启用 ttyGS0 串口登录

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
```

仅当内核使用 `g_serial` 且没有其他 USB Gadget 配置时加入：

```bash
echo g_serial | sudo tee -a "~/chroot/etc/modules"
```

```bash
exit
```

---

## 11. 打包 boot.img

```bash
cd ~/Ubuntu_For_Xiaomi5_Gemini/boot
```

```bash
cp ~/chroot/boot/initrd* ~/Ubuntu_For_Xiaomi5_Gemini/boot/initrd.img
```

```bash
cat Image.gz jdi.dtb > kernel-dtb-jdi
cat Image.gz lgd.dtb > kernel-dtb-lgd
```

生成 boot ：

```bash
ROOT_UUID="$(cat "~/Ubuntu_For_Xiaomi5_Gemini/boot/rootfs-uuid.txt")"

python3 ./mkbootimg/mkbootimg.py \
  --header_version 0 \
  --base 0x80000000 \
  --kernel_offset 0x00008000 \
  --ramdisk_offset 0x01000000 \
  --tags_offset 0x00000100 \
  --pagesize 4096 \
  --second_offset 0x00f00000 \
  --ramdisk initrd.img \
  --cmdline "console=tty0 root=UUID=$ROOT_UUID rw loglevel=3 splash" \
  --kernel kernel-dtb-jdi \
  --output boot-jdi.img

python3 ./mkbootimg/mkbootimg.py \
  --header_version 0 \
  --base 0x80000000 \
  --kernel_offset 0x00008000 \
  --ramdisk_offset 0x01000000 \
  --tags_offset 0x00000100 \
  --pagesize 4096 \
  --second_offset 0x00f00000 \
  --ramdisk initrd.img \
  --cmdline "console=tty0 root=UUID=$ROOT_UUID rw loglevel=3 splash" \
  --kernel kernel-dtb-lgd \
  --output boot-lgd.img
```

---

## 12. 卸载 rootfs 并生成 Android sparse 镜像

清理 rootfs：

```bash
sudo chroot ~/chroot 
```

```bash
apt clean
rm -f /tmp/*
history -c
exit
```

卸载绑定目录和 rootfs：

```bash
sudo umount ~/chroot/proc
sudo umount ~/chroot/sys
sudo umount ~/chroot/dev/pts
sudo umount ~/chroot/dev
sudo umount ~/chroot
```

转换为 Android sparse 镜像：

```bash
img2simg ~/rootfs-noble.img "~/Ubuntu_For_Xiaomi5_Gemini/boot/rootfs.simg"
```

---

## 最后生成物为：

```text
~/Ubuntu_For_Xiaomi5_Gemini/boot/rootfs.img    ## Ubuntu 本体镜像
~/Ubuntu_For_Xiaomi5_Gemini/boot/boot-jdi.img    ## 适用jdi屏幕的boot镜像
~/Ubuntu_For_Xiaomi5_Gemini/boot/boot-lgd.img    ## 适用lgd屏幕的boot镜像
```