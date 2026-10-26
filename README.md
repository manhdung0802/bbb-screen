# A small application combining:
## 1. Hardware:
  - BeagleBone Black
  - Nunchuk (I2C)
  - 7-inch monitor
  - Updates...
## 2. Software:
  - Linux kernel 6.xxx (built with Yocto)
  - Kernel driver
  - User space (Wayland)

---

# Note dự án
## Yocto
- Yocto 6.0.4 wrynose
- Check recipe nào đang dùng: `bitbake -e virtual/kernel | grep PREFERRED_PROVIDER_virtual/kernel`
- Check phiên bản của 1 recipe đang dùng: `bitbake -e virtual/kernel | grep ^PV=`
- Chuyển hệ thống sang dùng systemd trong local.conf: `INIT_MANAGER = "systemd"`
- Giải nén vào nfsroot: `sudo tar xpf *.rootfs.tar.xz -C nfsroot/`
- kiểm tra xem recipe của mình đã được ưu tiên hơn mặc định yocto chưa: `bitbake -e virtual/kernel | grep ^FILESEXTRAPATHS=`
- Lệnh copy rootfs mới: `sudo tar --overwrite -xpf /home/as/Desktop/linuxEmbeddedBBB/bootlin/Yocto-Project/yocto-bbb-labs/build/deploy-ti/images/bbb-screen-machine/core-image-minimal-bbb-screen-machine.rootfs.tar.xz -C /home/as/Desktop/linuxEmbeddedBBB/bbb-screen/nfsroot`
- Trong bbappend, nếu URI là .patch thì yocto sẽ chỉnh sửa vào nội dung file gốc, nếu là tên file trùng với file gốc thì sẽ dùng file mới đó

## Device tree
- muốn sửa đổi 1 node trong các device tree được include thì cần dấu `&`, ví dụ: &i2c1, &i2c2. Nếu không có `&` thì sẽ hiểu là tạo node mới, sẽ có thể bị báo duplicate với 1 

## Kernel linux
- Kernel version: 6.6.44
- IIO subsystem:
  + Các bước cần làm trong probe
    + thiết lập, mô tả device đó trong linux (cấp phát iio_dev)
    + cấu hình thanh ghi và các mục cần thiết của cảm biến
    + quản lý trigger (iio_trigger)
    + quản lý buffer
  + Các bước cấu hình chung 1 cảm biến:
    + đọc chip ID, reset cấu hình rác
    + quản lý nguồn và xung nhịp
    + cấu hình dải đo, tốc độ
    + cấu hình bộ lọc nhiễu (thông thấp, thông cao)
    + thiết lập bộ đệm (FIFO, ...), ngắt

## 