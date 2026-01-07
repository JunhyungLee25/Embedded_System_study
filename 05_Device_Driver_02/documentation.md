## 1. 문자 디바이스 드라이버의 이해

리눅스 커널이 장치를 식별하는 방식인 주 번호(Major Number)를 직접 지정하여 장치를 등록해 보기.

- 장치 등록: `/proc/devices`에서 비어 있는 번호를 확인하고, 이를 커널 모듈에 정의
	  
- 디바이스 노드 생성: `mknod` 명령어를 사용하여 `/dev/` 디렉터리에 실제 어플리케이션이 접근할 수 있는 파일 형태의 장치를 생성
	  
- 동작 확인: 작성한 어플리케이션(`major_num_example.c`)으로 커널 모듈의 `open`, `close` 함수가 호출되는지 확인.

## 2. GPIO 제어 실습 (LED & Button)

실제 하드웨어인 LED와 버튼을 제어하는 드라이버를 작성해보기.

- **GPIO 드라이버 구현**: 커널의 GPIO 라이브러리를 사용하여 입력(Button)과 출력(LED)을 처리하는 `read`, `write` 함수를 드라이버 내에 구현.
	  
- **크로스 컴파일 및 테스트**: 가상환경(Ubuntu)에서 빌드한 `.ko` 파일을 타겟 보드로 전송하여 실제 동작을 검증.

## 실습

#### Makefile
```d
# 1. 커널 모듈로 빌드할 오브젝트 파일명을 지정합니다.
# 이 파일명에 따라 최종적으로 dev_nr.ko 파일이 생성됩니다.
obj-m += dev_nr.o 

# 2. 빌드할 사용자 어플리케이션의 결과물 이름과 소스 파일명을 정의합니다.
RESULT = major_num_example 
SRC = $(RESULT).c 

# 3. 'all' 타겟: 커널 모듈 빌드와 어플리케이션 컴파일을 동시에 진행합니다.
all:
	# (중요) 커널 소스 경로(/home/linux123/working/kernel)는 본인의 환경에 맞게 수정. 
	# M=$(PWD)는 현재 디렉토리의 소스를 커널 빌드 시스템에 전달한다는 의미
	make -C /home/linux123/working/kernel M=$(PWD) modules 
	
	# 사용자 어플리케이션을 타겟 보드(arm64)용으로 크로스 컴파일합니다.
	aarch64-linux-gnu-gcc -o $(RESULT) $(SRC) 

# 4. 'clean' 타겟: 빌드 과정에서 생성된 임시 파일들과 결과물을 삭제.
clean:
	make -C $(HOME)/working/kernel M=$(PWD) clean 
	rm -f $(RESULT) 
```

####  Target board에서, /proc/decices에 비어있는 숫자 확인
```
# cat /proc/devices
Character devices:
  1 mem
  4 /dev/vc/0
  4 tty
  4 ttyS
  5 /dev/tty
  5 /dev/console
  5 /dev/ptmx
  5 ttyprintk
  7 vcs
 10 misc
 13 input
 14 sound
 29 fb
128 ptm
136 pts
180 usb
189 usb_device
204 ttyAMA
239 binder
240 vchiq
241 hidraw
242 rpmb
243 nvme-generic
244 nvme
245 bcm2835-gpiomem
246 vc-mem
247 bsg
248 watchdog
249 ptp
250 pps
251 lirc
252 rtc
253 dma_heap
254 gpiochip

Block devices:
  1 ramdisk
  7 loop
  8 sd
 65 sd
 66 sd
 67 sd
 68 sd
 69 sd
 70 sd
 71 sd
128 sd
129 sd
130 sd
131 sd
132 sd
133 sd
134 sd
135 sd
179 mmc
259 blkext
```

#### major_num_example.c
```d
#include <stdio.h>  
#include <stdlib.h>
#include <unistd.h>  
#include <fcntl.h>

int main() {
    int dev = open("/dev/mydevice", O_RDONLY);
    if(dev == -1) {
        printf("Opening was not possible!\n")
        return -1;
	}
    printf("Opening was successfull!\n");
    close(dev);

    return 0;
}
시스템 호출(System Call): open()과 close()는 사용자 어플리케이션이 커널에게 명령을 내리는 통로이다. 어플리케이션에서 open()을 호출하면, 커널 내부 드라이버의 driver_open 함수가 실행됨. 

장치 파일 경로: /dev/mydevice라는 이름은 임의로 정한 것이지만, 실제 하드웨어 노드가 이 이름으로 생성되어 있지 않으면 open()은 무조건 실패한다. 나중에 mknod 명령어를 통해 이 파일을 직접 만들어 줄 것이다. 

파일 디스크립터(File Descriptor): 변수 dev는 정수값을 가지며, 프로세스가 이 파일을 관리하기 위한 ID 번호이다.
```

#### dev_nr.c

```d
#include <linux/module.h> // 모든 커널 모듈에 필요한 헤더
#include <linux/init.h>   // module_init, module_exit 매크로를 위한 헤더
#include <linux/fs.h>     // 문자 디바이스 등록 및 파일 오퍼레이션을 위한 헤더

/* Meta Information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Johannes & GNU/Linux");
MODULE_DESCRIPTION("Registers a device nr. and implement some callback functions");

// 주 번호(Major Number)를 64번으로 정의합니다. 
#define MYMAJOR 64

/**
 * @brief 디바이스 파일이 열릴 때 호출되는 함수
 */
static int driver_open(struct inode *device_file, struct file *instance) {
    printk("dev_nr - open was called!\n"); // 커널 로그 출력 
    return 0;
}

/**
 * @brief 디바이스 파일이 닫힐 때 호출되는 함수
 */
static int driver_close(struct inode *device_file, struct file *instance) {
    printk("dev_nr - close was called!\n"); // 커널 로그 출력 
    return 0;
}

/* 파일 오퍼레이션 구조체: 어플리케이션의 시스템 호출과 드라이버 함수를 연결합니다.  */
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = driver_open,       // open() 연결
    .release = driver_close    // close() 연결
};

/**
 * @brief 모듈이 커널에 로드될 때 호출되는 초기화 함수 
 */
static int __init ModuleInit(void) {
    int retval;
    printk("Hello, Kernel!\n"); [cite: 381]

    /* 문자 디바이스 등록: 주 번호 64, 이름 "my_dev_nr", 그리고 fops 전달  */
    retval = register_chrdev(MYMAJOR, "my_dev_nr", &fops);

    if (retval == 0) {
        // 등록 성공 시 (정적 할당의 경우 0 리턴)
        printk("dev_nr - registered Device number Major: %d, Minor: %d\n", MYMAJOR, 0);
    } else if (retval > 0) {
        // 커널이 번호를 임의로 할당해준 경우 (동적 할당) 
        printk("dev_nr - registered Device number Major: %d, Minor: %d\n", retval >> 20, retval & 0xfffff);
    } else {
        // 등록 실패 시 
        printk("Could not register device number\n");
        return -1;
    }
    return 0;
}

/**
 * @brief 모듈이 커널에서 제거될 때 호출되는 종료 함수 
 */
static void __exit ModuleExit(void) {
    // 등록했던 문자 디바이스 해제 
    unregister_chrdev(MYMAJOR, "my_dev_nr");
    printk("Goodbye, Kernel\n"); [cite: 399]
}

module_init(ModuleInit); // 진입점 등록 
module_exit(ModuleExit); // 종료점 등록

- printk: 사용자 공간의 `printf`와 달리 커널 공간에서는 `printk`를 사용하여 메시지를 출력합니다. 이 메시지는 터미널에 바로 보이지 않을 수 있으며, 타겟 보드에서 `dmesg` 명령어를 입력해야 확인할 수 있습니다.
    
- register_chrdev: 이 함수는 커널에게 "내가 주 번호 64번을 쓸 테니, 이 번호로 들어오는 요청은 이 `fops` 구조체에 정의된 함수들로 보내줘"라고 선언하는 것입니다.
    
- __init & __exit: 이 매크로는 메모리 최적화를 위해 사용됩니다. 초기화 함수는 실행 후 메모리에서 해제될 수 있도록 표시하는 역할을 합니다.
```
- make를 이용해 cross compile

### 보드에 파일 전송 후 디바이스 드라비어 커널 등록

#### 1. `insmod dev_nr.ko`
- 컴파일된 드라이버 파일을 커널 메모리에 올린다.
- 커널 드라이버 소스 코드 내의 `ModuleInit()` 함수가 실행되면서, 커널에게 "주 번호 64번은 내가 담당할게"라고 보고(등록)한다.
  
#### 2. `mknod /dev/mydevice c 64 0` - 디바이스 노드 생성
- `/dev` 디렉터리에 `mycycle`라는 이름의 특수 파일을 만든다.
- 이 파일은 일반적인 데이터 저장용 파일이 아닌, 커널 드라이버로 가는 출입구 역할을 한다. 여기서 `c`는 문자 디바이스, 64는 방금 등록한 드라이버의 주 번호를 의미.
#### 3. `./major_num_example` (테스트 실행)
- 사용자 어플리케이션을 실행하여 `/dev/mydevice` 파일을 열어본다.
- 앱이 파일을 여는 순간, 커널이 번호 64를 확인하고 우리가 만든 드라이버의 `driver_open` 함수를 호출한다. 
#### 4. `rmmpd`, `rm`
- 등록된 커널 모듈을 내리고, 생성한 디바이스 파일 삭제

#### 5. 명령어 모음
- **`insmod`**: 커널에 드라이버를 등록하고 초기화 함수 실행.
    
- **`lsmod`**: 현재 로드된 모듈 목록 확인.
    
- **`mknod`**: 앱과 드라이버를 이어주는 인터페이스 파일(노드) 생성.
    
- **`dmesg`**: 드라이버가 내뱉는 커널 로그(`printk`) 확인.
    
- **`rmmod`**: 커널에서 드라이버 제거.

### led_button_example

#### led_button_example.c
```d
#include <stdio.h>      // 표준 입출력
#include <stdlib.h>     // 표준 라이브러리
#include <string.h>     // 문자열 처리
#include <unistd.h>     // POSIX 운영체제 API (read, write, close)
#include <fcntl.h>      // 파일 제어 (O_RDWR)

int main(int argc, char **argv) {
    char buff;          // 버튼 상태를 저장할 버퍼
    char tmp;           // LED에 쓸 값을 저장할 임시 변수
    char prev = ' ';    // 이전 상태를 저장하여 변화가 있을 때만 출력하기 위함
    int dev;            // 장치 파일 디스크립터

    // 1. GPIO 장치 파일을 읽기/쓰기 모드(O_RDWR)로 엽니다.
    // 드라이버에서 설정할 이름인 "/dev/my_gpio"와 일치해야 합니다.
    dev = open("/dev/my_gpio", O_RDWR);

    if (dev == -1) {
        printf("Opening was not possible!\n");
        return -1;
    }

    printf("Opening was successful!\n");

    // 2. 무한 루프를 돌며 버튼 상태를 감시하고 LED를 제어합니다.
    while(1) {
        // 드라이버로부터 1바이트를 읽어옵니다 (버튼 상태)
        read(dev, &buff, 1);
        
        tmp = buff;

        // 읽어온 버튼 상태 값을 그대로 드라이버에 다시 씁니다 (LED 제어)
        write(dev, &tmp, 1);

        // 이전 상태와 다를 때만 콘솔에 상태를 출력합니다.
        if (prev != tmp) {
            printf("led is %c\n", tmp);
            prev = tmp;
        }

        // CPU 점유율을 낮추기 위해 짧은 지연 시간을 둡니다. (생략 가능)
        usleep(100000); 
    }

    close(dev);
    return 0;
}
# 프로그램 로직
1. 장치 오픈: `/dev/my_gpio` 노드를 통해 커널 드라이버와 연결.
2. 상태 동기화: 버튼 입력 상태를 읽어(Read) 변수에 저장한 뒤, 곧바로 LED 출력(Write)으로 전달.
3. 상태 변화 감지: `prev` 변수를 활용해 버튼 상태가 바뀔 때만 터미널에 메시지 출력.
   
# 핵심포인트
- O_RDWR (Read/Write): 이전 실습에서는 `O_RDONLY`로 열었지만, 이번에는 버튼 값을 읽고 LED 값을 써야 하므로 읽기/쓰기 권한이 모두 필요하다.
    
- 동작 원리: 앱이 `read()`를 호출하면 커널 드라이버가 버튼이 연결된 GPIO 핀의 전압을 체크해서 알려w준다. 앱이 다시 `write()`를 호출하면 드라이버가 LED가 연결된 GPIO 핀에 전압을 인가하거나 차단한다.
    
- 데이터 타입: 하드웨어 제어 시 가장 작은 단위인 `char`(1바이트)를 사용하여 '0' 또는 '1'의 데이터를 주고받는 것이 일반적이다.
```
#### gpio_driver.c
```d
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Y.B. Cho LiuWei");
MODULE_DESCRIPTION("GPIO Driver for LED and Button");

/* 가상 장치 파일 정보 */
#define DRIVER_NAME "my_gpio"
#define DRIVER_CLASS "my_gpio_class"

/* GPIO 핀 번호 설정 (라즈베리 파이 4B 기준) */
#define GPIO_LED 4      // LED 연결 핀
#define GPIO_BTN 17     // 버튼 연결 핀 

static dev_t my_device_nr;
static struct class *my_class;
static struct cdev my_device;

/**
 * @brief 장치에서 데이터를 읽어올 때 (버튼 상태 확인) 
 */
static ssize_t driver_read(struct file *instance, char __user *user_buffer, size_t count, loff_t *offs) {
    int to_copy, not_copied, delta;
    char tmp;

    to_copy = min(count, sizeof(tmp));
    
    // 버튼 상태 읽기 (0 또는 1)
    tmp = gpio_get_value(GPIO_BTN) + '0'; 

    // 커널 데이터를 사용자 공간으로 복사 
    not_copied = copy_to_user(user_buffer, &tmp, to_copy);
    delta = to_copy - not_copied;

    return delta;
}

/**
 * @brief 장치에 데이터를 쓸 때 (LED 제어) 
 */
static ssize_t driver_write(struct file *instance, const char __user *user_buffer, size_t count, loff_t *offs) {
    int to_copy, not_copied, delta;
    char tmp;

    to_copy = min(count, sizeof(tmp));

    // 사용자 공간에서 데이터를 가져옴
    not_copied = copy_from_user(&tmp, user_buffer, to_copy);
    delta = to_copy - not_copied;

    // 전달받은 값에 따라 LED ON/OFF
    if (tmp == '1') {
        gpio_set_value(GPIO_LED, 1);
    } else if (tmp == '0') {
        gpio_set_value(GPIO_LED, 0);
    } else {
        printk("Invalid Input!\n");
    }

    return delta;
}

static int driver_open(struct inode *device_file, struct file *instance) {
    printk("led_button open was called!\n");
    return 0;
}

static int driver_close(struct inode *device_file, struct file *instance) {
    printk("led_button close was called!\n");
    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = driver_open,
    .release = driver_close,
    .read = driver_read,
    .write = driver_write
};

static int __init ModuleInit(void) {
    printk("Hello, Kernel!\n");

    /* 1. 장치 번호 할당 */
    if (alloc_chrdev_region(&my_device_nr, 0, 1, DRIVER_NAME) < 0) {
        printk("Device Nr. could not be allocated!\n");
        return -1;
    }

    /* 2. 장치 클래스 생성 */
    if ((my_class = class_create(THIS_MODULE, DRIVER_CLASS)) == NULL) {
        unregister_chrdev_region(my_device_nr, 1);
        return -1;
    }

    /* 3. 장치 노드 생성 */
    if (device_create(my_class, NULL, my_device_nr, NULL, DRIVER_NAME) == NULL) {
        class_destroy(my_class);
        unregister_chrdev_region(my_device_nr, 1);
        return -1;
    }

    /* 4. 문자 장치 초기화 및 등록 */
    cdev_init(&my_device, &fops);
    if (cdev_add(&my_device, my_device_nr, 1) < 0) {
        device_destroy(my_class, my_device_nr);
        class_destroy(my_class);
        unregister_chrdev_region(my_device_nr, 1);
        return -1;
    }

    /* 5. GPIO 설정 */
    if (gpio_request(GPIO_LED, "rpi-gpio-4")) {
        printk("Can not allocate GPIO 4\n");
        return -1;
    }
    gpio_direction_output(GPIO_LED, 0);

    if (gpio_request(GPIO_BTN, "rpi-gpio-17")) {
        printk("Can not allocate GPIO 17\n");
        gpio_free(GPIO_LED);
        return -1;
    }
    gpio_direction_input(GPIO_BTN);

    return 0;
}

static void __exit ModuleExit(void) {
    gpio_free(GPIO_LED);
    gpio_free(GPIO_BTN); 
    cdev_del(&my_device); 
    device_destroy(my_class, my_device_nr);
    class_destroy(my_class); 
    unregister_chrdev_region(my_device_nr, 1); 
    printk("Goodbye, Kernel\n");
}

module_init(ModuleInit);
module_exit(ModuleExit);

# 드라이버 주요 메커니즘
1. GPIO 제어: `gpio_set_value`로 출력(LED)을 제어하고, `gpio_get_value`로 입력(Button)을 감지.
    
2. 사용자 인터페이스: `read` 호출 시 버튼 상태를 반환하고, `write` 호출 시 LED 상태를 업데이트.
    
3. 자동 노드 생성: `class_create`와 `device_create`를 사용하여 드라이버 로드 시 `/dev/my_gpio`가 자동으로 생성되도록 구현.

# 핵심포인트
- `copy_to_user` / `copy_from_user`: 커널 메모리와 사용자 메모리는 분리되어 있다. 따라서 단순 대입 연산으로는 데이터를 주고받을 수 없으며, 반드시 이 함수들을 사용해야 시스템이 안전하게 데이터를 복사한다
    
- `alloc_chrdev_region`: 이전 실습처럼 주 번호를 수동(Static)으로 정하지 않고, 커널이 남는 번호를 자동으로 할당하도록 하는 방식. 충돌 방지를 위해 이 동적 할당 방식을 주로 사용.
    
- GPIO 관리: `gpio_request`를 통해 핀 사용권을 얻고, `gpio_free`로 반납하는 과정은 다른 드라이버와의 자원 충돌을 막기 위해 필수적이이다.
```

