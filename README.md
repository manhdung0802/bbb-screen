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

# Tổng hợp quy trình làm việc

## 1. Các bước viết Device Tree (DTS)

> Tổng hợp từ: `bootlin/linux-kernel/README.md`, `bootlin/embedded-linux/README.md`

### Khái niệm cơ bản
- **Device Tree** mô tả phần cứng (hardware) chứ **không** mô tả cách cấu hình hay cách driver hoạt động.
- File nguồn `.dts` (device tree source) được biên dịch bởi `dtc` ra file nhị phân `.dtb` (device tree blob).
  ```bash
  dtc -I dts -O dtb -o foo.dtb foo.dts   # compile
  dtc -I dtb -O dts foo.dtb              # decompile để đọc
  ```
- `.dtsi`: file dùng để include, chứa định nghĩa chung ở mức SoC.
- `.dts`: file cuối cùng dành cho board cụ thể.
- File DTS nằm ở `arch/<ARCH>/boot/dts/<vendor>/`.

### Các bước thực hiện

**Bước 1: Hiểu cấu trúc DTS**
- Node gốc `/` → các node con biểu diễn peripheral.
- Mỗi node có dạng `label: name@address { properties; child-nodes; }`.
- Thuộc tính quan trọng:
  - `compatible = "vendor,model"`: để kernel tìm driver phù hợp.
  - `reg = <address size>`: địa chỉ vật lý và kích thước vùng nhớ / địa chỉ I2C slave.
  - `status = "okay"` / `"disabled"`: bật/tắt thiết bị.
  - `interrupts`, `clocks`, `dmas`, `gpios`, ...: mô tả tài nguyên.
  - `#address-cells` và `#size-cells`: định nghĩa kích thước ô địa chỉ và size cho node con.
  - `pinctrl-<x>` và `pinctrl-names`: liên kết cấu hình pin mux.
  - với interrupt cần 2 thuộc tính
    ```
    interrupt-parent = <&gpio2>; // nhóm gpio chứa chân ngắt
    interrupt = <2 1>; // 2: thứ tự chân trong nhóm gpio, 1: chế độ ngắt IRQ_TYPE_EDGE_RISING
    ```

**Bước 2: Cấu hình Pin Mux**
- Khai báo trạng thái pin mux trong node `&am33xx_pinmux` (trên AM335x):
  ```dts
  &am33xx_pinmux {
      i2c1_pins: i2c1_pins {
          pinctrl-single,pins = <
              AM33XX_PADCONF(AM335X_PIN_SPI0_D1, PIN_INPUT_PULLUP, MUX_MODE2)
              AM33XX_PADCONF(AM335X_PIN_SPI0_CS0, PIN_INPUT_PULLUP, MUX_MODE2)
          >;
      };
  };
  ```
- Tra cứu file `am33xx.h` để biết enum tương ứng với pin và mode.

**Bước 3: Khai báo thiết bị I2C trên bus**
```dts
&i2c1 {
    status = "okay";
    clock-frequency = <100000>;
    pinctrl-0 = <&i2c1_pins>;
    pinctrl-names = "default";

    nunchuk: nunchuk@52 {
        compatible = "nintendo,nunchuk";
        reg = <0x52>;
    };
};
```

**Bước 4: Thêm DTS vào Makefile kernel**
- Trong `arch/arm/boot/dts/ti/omap/Makefile`, thêm:
  ```makefile
  dtb-$(CONFIG_SOC_AM33XX) += am335x-boneblack-custom.dtb
  ```

**Bước 5: Build và validate**
```bash
make dtbs                          # build tất cả device tree
make DT_SCHEMA_FILES=Documentation/devicetree/bindings/trivial-devices.yaml dt_binding_check
make DT_SCHEMA_FILES=Documentation/devicetree/bindings/trivial-devices.yaml dtbs_check
```

**Bước 6: Kiểm tra trên board**
```bash
dtc -I fs /sys/firmware/devicetree/base/ > /tmp/dts
grep -C10 nunchuk /tmp/dts
i2cdetect -y 1    # UU = đã có driver; số = chưa có driver
```

### Binding syntax (YAML)
- Mỗi driver cần 1 file YAML trong `Documentation/devicetree/bindings/`.
- Các thành phần: `compatible`, `reg`, `required`, `additionalProperties: false`.
- Validate binding: `make dt_binding_check`.

---

## 2. Các bước viết Kernel Module

> Tổng hợp từ: `bootlin/linux-kernel/README.md`

### Phân loại module
| Loại | Mô tả |
|------|-------|
| **Out-of-tree** | Code nằm ngoài kernel source, build riêng, chỉ có thể là `.ko` |
| **In-tree** | Tích hợp vào kernel source, có thể build static hoặc module |

### Các bước thực hiện

**Bước 1: Viết boilerplate module**
```c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

static int __init mydrv_init(void)
{
    pr_info("Driver loaded\n");
    return 0;
}

static void __exit mydrv_exit(void)
{
    pr_info("Driver unloaded\n");
}

module_init(mydrv_init);
module_exit(mydrv_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("My driver");
MODULE_AUTHOR("Author");
```
- `__init`: vùng nhớ được giải phóng sau khi kernel init xong.
- `__exit`: bị loại bỏ nếu build static.
- Chỉ dùng kernel headers (linux/), không dùng thư viện C thông thường.

**Bước 2: Viết Makefile out-of-tree**
```makefile
ifneq ($(KERNELRELEASE),)
obj-m := mydriver.o
else
KDIR := /path/to/kernel/sources
all:
	$(MAKE) -C $(KDIR) M=$$PWD
clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean
endif
```
- Cross-compile: `make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-`

**Bước 3: Khai báo bảng compatible (với DT)**
```c
static const struct of_device_id mydrv_of_ids[] = {
    { .compatible = "vendor,mydevice" },
    { }
};
MODULE_DEVICE_TABLE(of, mydrv_of_ids);
```
- `MODULE_DEVICE_TABLE`: cho phép `udev` tự động load module khi device xuất hiện.

**Bước 4: Đăng ký driver với bus subsystem**
- đưa device vào danh mục quản lý của linux

*I2C driver:*
```c
static struct i2c_driver mydrv_driver = {
    .driver = {
        .name = "mydriver",
        .of_match_table = mydrv_of_ids,
    },
    .probe  = mydrv_probe,
    .remove = mydrv_remove,
};
module_i2c_driver(mydrv_driver);
```

*Platform driver:*
```c
static struct platform_driver mydrv_driver = {
    .driver = {
        .name = "mydriver",
        .of_match_table = mydrv_of_ids,
    },
    .probe  = mydrv_probe,
    .remove = mydrv_remove,
};
module_platform_driver(mydrv_driver);
```

**Bước 5: Implement hàm `probe()`**
```c
static int mydrv_probe(struct i2c_client *client)
{
    struct mydrv_data *data;

    /* 1. Cấp phát bộ nhớ */
    data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    /* 2. Lưu context */
    i2c_set_clientdata(client, data);
    data->client = client;

    /* 3. Khởi tạo phần cứng (ví dụ: init nunchuk) */
    /* gửi lệnh init qua i2c_master_send() */

    /* 4. Đăng ký với kernel framework */
    /* ví dụ: input_register_device(), misc_register(), ... */

    return 0;
}
```
- Lấy tài nguyên: `devm_gpiod_get()`, `devm_request_threaded_irq()`, `devm_platform_ioremap_resource()`.
- Dùng prefix `devm_` để tự động giải phóng khi device bị tháo.
- Các bước chung trong hàm probe();
  + Bước 1: Cấp phát bộ nhớ quản lý thiết bị (Private Data)Driver cần một biến cấu trúc riêng (thường đặt tên là struct my_device_data) để lưu toàn bộ trạng thái của thiết bị (địa chỉ I/O, số ngắt, con trỏ subsystem, mutex...).Cấp phát bộ nhớ cho cấu trúc này bằng devm_kzalloc().Lưu con trỏ này vào cấu trúc struct device bằng hàm dev_set_drvdata() để các hàm khác (như remove(), hàm ngắt ISR) có thể lấy lại dữ liệu khi cần.
  + Bước 2: Rút trích tài nguyên phần cứng (Resource Acquisition)Driver đọc thông tin phần cứng mà Device Tree hoặc bus cung cấp:   Địa chỉ vùng nhớ / Thanh ghi (I/O Memory): Lấy tài nguyên bằng platform_get_resource() và ánh xạ sang địa chỉ ảo bằng devm_ioremap_resource() hoặc ioremap() để CPU đọc/ghi thanh ghi.   Chân ngắt (IRQ): Lấy số hiệu ngắt từ Device Tree bằng platform_get_irq().   Chân điều khiển (GPIOs): Lấy các chân GPIO như Reset, Chip Select, Power Enable bằng devm_gpiod_get_optional().Các thuộc tính mở rộng: Đọc các tham số cấu hình riêng từ Device Tree bằng các hàm of_property_read_*.   
  + Bước 3: Đánh thức và kiểm tra phần cứng (Hardware Init)Trước khi công bố thiết bị cho hệ điều hành, driver cần đảm bảo chip phần cứng thực sự phản hồi:Cấp nguồn hoặc bật Clock cho thiết bị nếu có.Thực hiện chuỗi kích hoạt cơ bản (như kéo chân Reset mức thấp rồi kéo lên mức cao để Hard Reset IC).Đọc một thanh ghi nhận diện phần cứng (ví dụ WHO_AM_I hoặc CHIP_ID) để xác minh chip đã gắn đúng và giao tiếp tốt.Gửi chuỗi thiết lập ban đầu (Init Sequence) để đưa linh kiện về trạng thái sẵn sàng.
  + Bước 4: Thiết lập cơ chế nhận dữ liệu & Cấu hình SubsystemChuẩn bị sẵn "khung" của loại thiết bị mà bạn muốn đăng ký:Cấp phát cấu trúc của Subsystem: Cấp phát struct input_dev, struct iio_dev, hay struct net_device tùy vào mục đích nghiệp vụ.Khai báo năng lực (Capabilities): Báo cho kernel biết thiết bị có thể làm những gì (ví dụ: các nút bấm có thể nhấn, dải đo gia tốc, độ phân giải màn hình).Đăng ký xử lý ngắt: Gọi devm_request_threaded_irq() gắn số IRQ đã lấy ở Bước 2 với hàm xử lý ngắt (ISR) để khi chip phát tín hiệu, driver sẽ đón được dữ liệu.
  + Bước 5: Đăng ký thiết bị với Subsystem (Registration)Đây là thao tác cuối cùng để kích hoạt thiết bị:Gọi hàm đăng ký chính thức của subsystem tương ứng (ví dụ: input_register_device(), iio_device_register(), hoặc register_netdev()).   Sau khi hàm này hoàn tất, kernel sẽ tạo các file node trong /dev (hoặc /sys/class/) để ứng dụng User-space có thể mở và sử dụng.   Kết thúc hàm probe() và trả về 0 (thành công).


**Bước 6: Implement hàm `remove()`**
```c
static void mydrv_remove(struct i2c_client *client)
{
    /* driver tự động giải phóng nếu dùng devm_* */
    /* power down hardware nếu cần */
}
```

**Bước 7 (tùy chọn): Implement framework tương ứng**

*Input subsystem:*
```c
struct input_dev *idev = devm_input_allocate_device(&client->dev);
set_bit(EV_KEY, idev->evbit);
set_bit(BTN_0,  idev->keybit);
input_register_device(idev);
/* Khi có event: */
input_report_key(idev, BTN_0, 1);
input_sync(idev);
```

*Character device:*
```c
static struct file_operations mydrv_fops = {
    .open    = mydrv_open,
    .release = mydrv_release,
    .read    = mydrv_read,
    .write   = mydrv_write,
};
/* copy data kernel <-> user space */
copy_to_user(buf, kbuf, len);
copy_from_user(kbuf, buf, len);
```

**Bước 8: Test trên board**
```bash
insmod mydriver.ko            # nạp module
lsmod                         # kiểm tra module đã nạp
dmesg | tail -20              # xem log
ls /sys/bus/i2c/drivers/      # kiểm tra driver đã đăng ký
ls /dev/                      # kiểm tra device file
rmmod mydriver                # gỡ module
```

**Bước 9: Kiểm tra coding style**
```bash
~/linux-kernel/scripts/checkpatch.pl --file --no-tree mydriver.c
```

### Giao tiếp I2C
```c
/* Gửi data */
int i2c_master_send(const struct i2c_client *client, const char *buf, int count);
/* Nhận data */
int i2c_master_recv(const struct i2c_client *client, char *buf, int count);
/* Transfer nhiều message */
int i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);
```

### Lập trình với Ngắt (Interrupt)

#### Khái niệm: Process context vs Atomic context

| Context | Đặc điểm |
|---------|----------|
| **Process context** | Thread/process bình thường, được lập lịch bởi scheduler, **cho phép sleep** |
| **Atomic context** | CPU đang xử lý ngắt hoặc giữ spinlock, **không được sleep**, tất cả ngắt khác bị disable |

> **Quy tắc vàng:** Trong interrupt handler (top half), **không được sleep**, không dùng `copy_to_user/copy_from_user`, không dùng `mutex_lock`. Phải cấp phát bộ nhớ với `GFP_ATOMIC`.

#### Đăng ký interrupt handler (trong `probe()`)

```c
/* API đơn giản – không có bottom half */
int devm_request_irq(struct device *dev,
                     unsigned int irq,
                     irq_handler_t handler,
                     unsigned long irq_flags,
                     const char *devname,
                     void *dev_id);

/* API threaded – có top half + bottom half */
int devm_request_threaded_irq(struct device *dev,
                               unsigned int irq,
                               irq_handler_t handler,      /* top half */
                               irq_handler_t thread_fn,    /* bottom half */
                               unsigned long flags,
                               const char *name,
                               void *dev_id);
```

Lấy số ngắt từ Device Tree (platform device):
```c
int irq = platform_get_irq(pdev, 0);   /* lấy IRQ số 0 trong DT */
```

Lấy số ngắt từ I2C client:
```c
int irq = client->irq;
```

**`irq_flags` thường dùng:**

| Flag | Ý nghĩa |
|------|---------|
| `IRQF_SHARED` | Chia sẻ đường ngắt giữa nhiều device |
| `IRQF_ONESHOT` | Dùng với threaded IRQ – giữ đường ngắt disable đến khi `thread_fn` chạy xong |
| `IRQF_TRIGGER_RISING` | Kích hoạt theo cạnh lên |
| `IRQF_TRIGGER_FALLING` | Kích hoạt theo cạnh xuống |

#### Prototype của interrupt handler

```c
irqreturn_t my_interrupt_handler(int irq, void *dev_id)
{
    struct mydrv_data *data = dev_id;

    /* 1. Kiểm tra đúng device mình quản lý */
    /* 2. Xác nhận ngắt với phần cứng (ack) – bắt buộc, nếu không sẽ bị interrupt flood */
    /*    - Đọc thanh ghi status của device, hoặc */
    /*    - Ghi vào thanh ghi clear-interrupt */

    /* 3. Đọc data từ device */

    /* 4. Đánh thức process đang chờ (nếu có) */
    wake_up_interruptible(&data->wait_queue);

    return IRQ_HANDLED;   /* ngắt đã xử lý */
    /* return IRQ_NONE;   ngắt không phải của device này */
    /* return IRQ_WAKE_THREAD;  kích hoạt bottom half (threaded handler) */
}
```

#### Top half + Bottom half (Threaded IRQ)

```c
/* Top half – chạy trong atomic context, phải nhanh */
irqreturn_t my_hard_irq(int irq, void *dev_id)
{
    /* Xác nhận ngắt */
    /* Đọc data tối thiểu cần thiết */
    return IRQ_WAKE_THREAD;  /* kích hoạt bottom half */
}

/* Bottom half – chạy trong process context, được phép sleep */
irqreturn_t my_threaded_handler(int irq, void *dev_id)
{
    struct mydrv_data *data = dev_id;

    /* Xử lý data tốn thời gian */
    /* Báo cáo sự kiện lên kernel framework */
    input_report_key(data->idev, BTN_0, 1);
    input_sync(data->idev);

    return IRQ_HANDLED;
}

/* Trong probe(): */
ret = devm_request_threaded_irq(&client->dev, client->irq,
                                 my_hard_irq,         /* top half */
                                 my_threaded_handler, /* bottom half */
                                 IRQF_ONESHOT,
                                 "mydriver",
                                 data);
```

#### Workqueue (cơ chế trì hoãn linh hoạt hơn)

```c
#include <linux/workqueue.h>

struct mydrv_data {
    struct work_struct work;
    /* ... */
};

/* Định nghĩa work handler */
void my_work_handler(struct work_struct *work)
{
    struct mydrv_data *data = container_of(work, struct mydrv_data, work);
    /* Xử lý tốn thời gian, được phép sleep */
}

/* Trong probe(): khởi tạo work */
INIT_WORK(&data->work, my_work_handler);

/* Trong interrupt handler: lên lịch work */
schedule_work(&data->work);
```

#### Wait Queue – đồng bộ process với interrupt

```c
#include <linux/wait.h>

struct mydrv_data {
    wait_queue_head_t wait;
    bool data_ready;
    /* ... */
};

/* Trong probe(): khởi tạo wait queue */
init_waitqueue_head(&data->wait);

/* Trong interrupt handler: đánh thức process đang chờ */
data->data_ready = true;
wake_up_interruptible(&data->wait);

/* Trong hàm read() của driver: chờ data sẵn sàng */
ret = wait_event_interruptible(data->wait, data->data_ready);
if (ret)
    return -ERESTARTSYS;  /* bị signal ngắt */
data->data_ready = false;
```

#### Bảng tóm tắt các cơ chế xử lý ngắt

| Cơ chế | Context | Sleep? | Dùng khi nào |
|--------|---------|--------|-------------|
| **Hard IRQ (top half)** | Atomic | ❌ | Xác nhận ngắt, lấy data tối thiểu |
| **Softirq** | Softirq | ❌ | Dùng bởi kernel subsystem (network...), không dùng trong driver |
| **Threaded IRQ (bottom half)** | Process | ✅ | Xử lý data sau top half |
| **Workqueue** | Process | ✅ | Trì hoãn công việc bất kỳ, không chỉ từ ngắt |

#### Lưu ý thực hành
- Mỗi lần ngắt xảy ra, **phải xác nhận (ACK) ngắt** với hardware, nếu không sẽ bị **interrupt flood** (ngắt lặp liên tục).
- Kiểm tra `/proc/interrupts` để xem các đường ngắt đang hoạt động.
- Dùng `IRQF_ONESHOT` khi đăng ký threaded IRQ để tránh ngắt tiếp theo kích hoạt trước khi `thread_fn` hoàn thành.

---

## 3. Các bước làm việc với Yocto

> Tổng hợp từ: `bootlin/Yocto-Project/README.md`

### Sơ đồ tổng quan
```
poky/             ← Yocto reference system (không sửa trực tiếp)
meta-ti/          ← BSP layer cho TI/BeagleBone
meta-arm/         ← Layer hỗ trợ ARM
meta-bbb-screen/  ← Custom layer của dự án
  conf/
    layer.conf
    machine/bbb-screen-machine.conf
  recipes-kernel/linux/linux-bb.org_%.bbappend
build/
  conf/
    bblayers.conf ← danh sách các layer
    local.conf    ← cấu hình build (MACHINE, INIT_MANAGER, ...)
```

### Bước 1: Thiết lập môi trường
```bash
source poky/oe-init-build-env <build-dir>
```
- Lệnh này set biến môi trường (`BUILDDIR`, `PATH`) và chuyển vào thư mục build.
- Sau mỗi lần mở terminal mới đều phải chạy lại.

### Bước 2: Cấu hình build
**`build/conf/bblayers.conf`** – thêm các layer cần dùng:
```bitbake
BBLAYERS ?= " \
  /path/to/poky/meta \
  /path/to/poky/meta-poky \
  /path/to/meta-ti/meta-ti-bsp \
  /path/to/meta-arm/meta-arm \
  /path/to/meta-bbb-screen \
"
```

**`build/conf/local.conf`** – cấu hình build:
```bitbake
MACHINE = "bbb-screen"
INIT_MANAGER = "systemd"
BB_NUMBER_THREADS = "8"
PARALLEL_MAKE = "-j8"
```

### Bước 3: Tạo custom layer
```bash
bitbake-layers create-layer -p <PRIORITY> meta-<layername>
bitbake-layers add-layer meta-<layername>
```
- Cấu trúc layer:
  ```
  meta-<name>/
    conf/
      layer.conf           ← bắt buộc (entry point)
    recipes-<cat>/
      <recipe>/
        <recipe>_<ver>.bb
        <recipe>_%.bbappend
  ```
- Không copy/sửa recipe gốc → dùng `.bbappend`.

### Bước 4: Cấu hình machine file
**`meta-bbb-screen/conf/machine/bbb-screen-machine.conf`:**
```bitbake
include conf/machine/include/arm/armv7a/tune-cortexa8.inc
PREFERRED_PROVIDER_virtual/kernel = "linux-bb.org"
PREFERRED_VERSION_linux-bb.org    = "6.6%"
PREFERRED_PROVIDER_virtual/bootloader = "u-boot"
KERNEL_IMAGETYPE    = "zImage"
KERNEL_DEVICETREE   = "ti/omap/am335x-boneblack-bbb-screen.dtb"
SERIAL_CONSOLES     = "115200;ttyO0"
MACHINE_FEATURES    = "usbgadget usbhost vfat alsa"
IMAGE_FSTYPES       = "tar.xz wic"
```

### Bước 5: Tạo recipe hoặc bbappend
**Ví dụ bbappend cho kernel – thêm DTS và patch:**
```bitbake
# linux-bb.org_%.bbappend
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += " \
    file://am335x-boneblack-bbb-screen.dts \
    file://nunchuk-driver.patch \
"

# Kernel features (nếu dùng linux-yocto Advanced Metadata)
KERNEL_FEATURES += "features/nunchuk.scc"
```

**Ví dụ recipe cho kernel module:**
```bitbake
# recipes-kernel/nunchuk/nunchuk.bb
SUMMARY = "Nunchuk I2C kernel module"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://COPYING;md5=..."

inherit module

SRC_URI = "file://nunchuk.c file://Makefile"
S = "${WORKDIR}"

RPROVIDES:${PN} += "kernel-module-nunchuk"
```

**Thêm module vào image trong `local.conf`:**
```bitbake
IMAGE_INSTALL:append = " nunchuk"
```

### Bước 6: Build
```bash
# Build toàn bộ image
bitbake core-image-minimal

# Build chỉ kernel và dtb
bitbake virtual/kernel

# Build 1 recipe cụ thể
bitbake nunchuk

# Debug: xem biến của recipe
bitbake-getvar -r linux-bb.org SRC_URI
bitbake -e virtual/kernel | grep ^PV=
bitbake -e virtual/kernel | grep PREFERRED_PROVIDER_virtual/kernel
```

### Bước 7: Các lệnh hữu ích
```bash
# Kiểm tra layer đang có
bitbake-layers show-layers
bitbake-layers show-recipes 'linux-*'
bitbake-layers show-appends

# Liệt kê task của recipe
bitbake -c listtasks virtual/kernel

# Chạy task cụ thể
bitbake -c menuconfig virtual/kernel
bitbake -c compile virtual/kernel

# Buộc build lại
bitbake -f virtual/kernel

# Tải tất cả source
bitbake --runall=fetch core-image-minimal
```

### Bước 8: Deploy
```bash
# Flash vào SD card
xz -dc core-image-minimal-beaglebone.rootfs.wic.xz | \
    sudo dd of=/dev/sdX conv=fdatasync bs=4M status=progress

# Giải nén rootfs vào NFS
sudo tar xpf core-image-minimal-beaglebone.rootfs.tar.xz -C /path/to/nfsroot/
```

### Cấu trúc thư mục build sau khi build
| Thư mục | Nội dung |
|---------|----------|
| `conf/` | File cấu hình |
| `downloads/` | Source tải về |
| `sstate-cache/` | Cache để tái sử dụng |
| `tmp/deploy/images/` | **Output cuối** (kernel, dtb, rootfs, bootloader) |
| `tmp/work/` | Source đã giải nén, log build của từng recipe |
| `tmp/buildstats/` | Thống kê CPU/time |

### Debug build lỗi
```bash
# Xem log của task bị lỗi
cat build/tmp/work/<machine>-poky-linux-gnueabi/<recipe>/<ver>/temp/log.do_compile

# Kiểm tra biến
bitbake -e <recipe> | grep <VARIABLE>
bitbake-getvar <VARIABLE>

# Chạy devshell để debug thủ công
bitbake -c devshell <recipe>
```

### Virtual provides
| Virtual | Các recipe cung cấp |
|---------|-------------------|
| `virtual/kernel` | `linux-yocto`, `linux-bb.org`, `linux-ti-staging` |
| `virtual/bootloader` | `u-boot`, `u-boot-ti-staging` |
| `virtual/libc` | `glibc`, `musl` |

```bitbake
PREFERRED_PROVIDER_virtual/kernel = "linux-bb.org"
PREFERRED_VERSION_linux-bb.org    = "6.6%"
```

---

# Note dự án

## Yocto
- Yocto 6.0.4 wrynose
- Check recipe nào đang dùng: `bitbake -e virtual/kernel | grep PREFERRED_PROVIDER_virtual/kernel`
- Check phiên bản của 1 recipe đang dùng: `bitbake -e virtual/kernel | grep ^PV=`
- Chuyển hệ thống sang dùng systemd trong local.conf:
  ```bitbake
  INIT_MANAGER = "systemd"
  ```
- Giải nén vào nfsroot: `sudo tar xpf *.rootfs.tar.xz -C nfsroot/`

## Kernel linux
- Kernel version: 6.6.44
- **Các bước viết một Kernel Driver I2C:**
  1. **Device Tree (DTS) & Pinmux:** Cấu hình pinmux (`&am33xx_pinmux`), khai báo node thiết bị trên bus ngoại vi (`&i2c1`, `&spi0`...) với các thuộc tính: `compatible`, `reg`, `pinctrl-*`, `interrupts`...
  2. **Boilerplate Kernel Module:** Khai báo header `<linux/init.h>`, `<linux/module.h>`, bus header (`<linux/i2c.h>`, `<linux/platform_device.h>`), và thông tin bản quyền `MODULE_LICENSE("GPL")`.
  3. **Match Table & Driver Registration:** Định nghĩa bảng `of_device_id` với chuỗi `compatible` khớp với DTS, gán vào struct driver (`struct i2c_driver`, `struct platform_driver`) và đăng ký qua macro (`module_i2c_driver()`, `module_platform_driver()`).
  4. **Hàm `probe()` (Khởi tạo tài nguyên & phần cứng):**
     - Cấp phát bộ nhớ quản lý thiết bị bằng `devm_kzalloc()`, lưu con trỏ với `i2c_set_clientdata()` / `platform_set_drvdata()`.
     - Lấy tài nguyên: GPIO (`devm_gpiod_get`), ngắt (`devm_request_threaded_irq`), I/O memory (`devm_platform_ioremap_resource`).
     - Đọc/ghi thanh ghi kiểm tra phần cứng (thanh ghi ID, WHO_AM_I qua I2C/SPI API).
     - Đăng ký thiết bị với Kernel Framework tương ứng (Input subsystem, IIO, Misc Device / Cdev).
  5. **Hàm `remove()` (Dọn dẹp & giải phóng):** Đưa thiết bị về chế độ tiết kiệm điện (power down/sleep), hủy đăng ký framework (nếu không dùng cơ chế tự động giải phóng `devm_*`).
  6. **Biên dịch & Test:** Viết `Makefile` ngoài cây nguồn (out-of-tree) trỏ tới `KDIR`, biên dịch ra file `.ko`, nạp vào bo mạch (`insmod`), kiểm tra log qua `dmesg`, kiểm tra file trong `/sys` và `/dev`, gỡ module bằng `rmmod`.