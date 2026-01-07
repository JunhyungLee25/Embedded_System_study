### 1. seg_example.c (프로그램 1)

이 코드는 실행 인자로 받은 숫자 데이터를 `/dev/my_segment`라는 장치 파일에 2바이트 크기로 써서 하드웨어를 제어하는 테스트 프로그램입니다.


```d
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

/**
 * seg_example1.c 
 * 사용자가 입력한 숫자 값을 7-세그먼트 디바이스 드라이버로 전달하는 프로그램이다.
 */
int main(int argc, char **argv) {
    short buff; // 세그먼트에 보낼 2바이트 데이터 저장 변수
    // 1. 디바이스 노드 파일을 읽기/쓰기 모드로 연다.
    int dev = open("/dev/my_segment", O_RDWR);

    // 실행 인자가 부족한 경우 사용법을 출력하고 종료한다.
    if (argc < 2) {
        printf("put arg 0x0000 or int\n");
        return -1;
    }

    // 2. 파일 오픈 성공 여부를 확인한다. (성공 시 양수, 실패 시 -1 반환)
    if(dev == -1){
        printf("Opening was not possible!\n");
        return -1;
    }
    printf("Opening was succesfull!\n");

    // 3. 입력된 문자열이 16진수(0x 시작)인지 10진수인지 판별하여 숫자로 변환한다.
    if(argv[1][0] == '0' && (argv[1][1] == 'x' || argv[1][1] == 'X'))
        // 16진수 문자열을 unsigned short형 정수로 변환한다.
        buff = (unsigned short)strtol(&argv[1][2], NULL, 16);
    else
        // 10진수 문자열을 정수로 변환한다.
        buff = (unsigned short)strtol(&argv[1][0], NULL, 10);
    
    // 4. 변환된 데이터(2바이트)를 write 시스템 콜을 통해 드라이버에 전달한다.
    write(dev, &buff, 2);

    // 5. 사용이 끝난 디바이스 파일을 닫는다.
    close(dev);
    return 0;
}

# strtol: #include<stlib.h>에 내장되어 있는 함수로 진법으로 표기된 문자열을 정수로 변환시켜준다.
사용법: strtol(문자열, 끝 포인터, 진법);

# 핵심 포인트
- 장치 파일 열기 (open): `open("/dev/my_segment", ...)`을 호출하면, 커널 내부에 등록된 `my_segment` 드라이버의 `open` 함수가 실행된다.
    
- 데이터 타입 일치: 임베디드 시스템에서는 하드웨어 레지스터 크기나 데이터 포맷에 맞춰 유저 공간의 데이터 크기를 정확히 맞춰야 한다. 여기서는 2바이트 통신을 전제로 하고 있다. -> 제어하기 위한 7-segment는 4자리의 숫자를 표시한다. 각 자릿수를 4비트로 표현한다고 가정하면 16bits가 필요하다. short는 2byte=16bit 이므로 하드웨어가 요구하는 데이터 크기와 정확히 일치한다. 또한 buff에 write할 때, write(...,2)를 하였는데 이때, buff가 int 자료형이면 시스템의 엔디안 방식에 따라 원하는 값이 아닌 엉뚱한 값이 전송될 수 있다. 데이터의 무결성을 보장하기 위해 전송할 크기와 변수의 크기를 일치시키는 것이 중요하다.
    
- 정수 변환 (strtol): 사용자가 터미널에서 입력한 값은 '문자열'이다. 이를 커널이 이해할 수 있는 '숫자 데이터'로 바꾸기 위해 `strtol` 함수를 사용.
    
- 시스템 콜 인터페이스: `write(dev, &buff, 2)` 함수는 유저 공간의 메모리에 있는 `buff` 값을 커널 공간의 드라이버로 복사해 줍니다. 이때 드라이버의 `write` 함수가 내부적으로 호출되며 실제 세그먼트의 LED가 켜지게 됩니다. 
```

---

### 2. seg_example2.c (프로그램 2)

키보드 설정을 변경하여 실시간 입력을 받으면서, 4개의 세그먼트 위치(`D1~D4`)에 각각 지정된 숫자 패턴을 순차적으로 출력하는 동적 제어 프로그램이다.

```d
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

// 터미널 설정을 저장하기 위한 구조체 (키보드 입력 제어용)
static struct termios init_setting, new_setting;

// 7-세그먼트 숫자 패턴 (0~9, Common-Anode 기준 예상)
char seg_num[10] = {0xc0, 0xf9, 0xa4, 0xb0, 0x99, 0x92, 0x82, 0xd8, 0x80, 0x90};
// 점(Dot)이 포함된 숫자 패턴
char seg_dnum[10] = {0x40, 0x79, 0x24, 0x30, 0x19, 0x12, 0x02, 0x58, 0x00, 0x10};

// 세그먼트의 위치를 지정하는 선택 신호 (Digit Select)
#define D1 0x01
#define D2 0x02
#define D3 0x03
#define D4 0x04

/**
 * 키보드 입력 방식을 비정규 모드(Non-canonical)로 설정
 * 엔터키 입력 없이도 즉시 문자를 읽어올 수 있게 함
 */
void init_keyboard() {
    tcgetattr(STDIN_FILENO, &init_setting); // 현재 설정 저장
    new_setting = init_setting;
    new_setting.c_lflag &= ~ICANON; // 버퍼링 사용 안 함
    new_setting.c_lflag &= ~ECHO;   // 입력한 키를 화면에 표시 안 함
    new_setting.c_cc[VMIN] = 0;     // 최소 입력 문자 수 0 (Non-blocking)
    new_setting.c_cc[VTIME] = 0;    // 대기 시간 0
    tcsetattr(STDIN_FILENO, TCSANOW, &new_setting);
}

// 프로그램 종료 시 키보드 설정을 원래대로 복구
void close_keyboard() {
    tcsetattr(STDIN_FILENO, TCSANOW, &init_setting);
}

// 현재 눌린 키가 있으면 읽어오고, 없으면 -1 반환
char get_key() {
    char ch = -1;
    if (read(STDIN_FILENO, &ch, 1) != 1)
        ch = -1;
    return ch;
}

void print_menu() {
    printf("\n---------- menu <7-Segment Control> ----------\n");
    printf("[r] : program reset\n");
    printf("[q] : Quit program\n");
    printf("---------------------------------------------------\n");
}

int main() {
    unsigned short data[4]; // 4자리에 출력할 데이터를 담는 배열
    char key;
    int tmp_n;
    int delay_time;

    // 1. 디바이스 드라이버 오픈
    int dev = open("/dev/my_segment", O_RDWR);

    if(dev == -1){
        printf("Opening was not possible!\n");
        return -1;
    }
    printf("device opening was succesfull!\n");

    init_keyboard(); // 실시간 키 입력을 위한 설정 시작
    print_menu();
    tmp_n = 0;
    delay_time = 1000000;

    /**
     * 2. 데이터 구성 (비트 연산)
     * 패턴(seg_num)을 상위 비트로 밀고, 위치(D1~D4)를 하위 비트와 OR 연산하여 
     * 하나의 16비트(2바이트) 패킷으로 만듦
     */
    data[0] = (seg_num[1] << 4 | D1); // 1번째 자리에 '1' 표시
    data[1] = (seg_num[2] << 4 | D2); // 2번째 자리에 '2' 표시
    data[2] = (seg_num[3] << 4 | D3); // 3번째 자리에 '3' 표시
    data[3] = (seg_num[4] << 4 | D4); // 4번째 자리에 '4' 표시

    while (1) {
        key = get_key(); // 키보드 입력 체크
        if (key == 'q') {
            printf("Quit program.\n");
            break;
        } else if (key == 'r') {
            delay_time = 1000000;
            tmp_n = 0;
        }
        
        // 3. 디바이스 드라이버에 데이터 쓰기 (한 자리씩 번갈아 가며 출력)
        write(dev, &data[tmp_n], 2);
        usleep(100000); // 0.1초 대기 (숫자가 잔상 효과로 보이게 됨)

        tmp_n++;
        if(tmp_n > 3) {
            tmp_n = 0;
            // 루프가 돌수록 딜레이를 줄이는 로직 (점점 빨라짐)
            if(delay_time > 5000) {
                delay_time /= 2;
            }
        }
    }

    // 4. 종료 처리 및 리소스 해제
    close_keyboard();
    unsigned short clear_val = 0x0000;
    write(dev, &clear_val, 2); // 세그먼트 끄기
    close(dev);
    return 0;
}

# 핵심 포인트
- Termios (키보드 제어):
	- 임베디드 제어 프로그램에서는 사용자가 'Enter'를 누를 때까지 기다리기 보다는 즉시 반응하도록 해야한다. `termios` 설정을 통해 키가 눌리는 즉시 반응하도록 인터랙티브한 UI를 구현하였다.
        
- 비트 연산 (`<< 4 | D1`):
	- `seg_num[1] << 4`: 숫자 '1'에 해당하는 패턴 값을 상위 4비트(또는 그 이상) 위치로 이동시킨다.
        
    - `| D1`: 하위 비트에 1번 세그먼트를 켜라는 선택 신호를 합친다.
        
    - 이렇게 합쳐진 2바이트 데이터를 드라이버에 던지면, 하드웨어는 "어떤 숫자를(상위)", "어디에(하위)" 켤지 한 번에 알 수 있다.
        
- 멀티플렉싱 (Multiplexing):
    - `while` 루프 안에서 `tmp_n`을 0부터 3까지 바꾼다. 이는 사실 세그먼트 4개를 동시에 켜는 것이 아니라, 1번→2번→3번→4번 순서대로 아주 빠르게 번갈아 가며 켜는 것이다. 사람의 눈에는 잔상 효과 때문에 4자리가 모두 켜져 있는 것처럼 보이게 된다.
      
      
```

#### 질문 1. `data[0] = (seg_num[1] << 4 | D1)`의 정확한 동작 방식

- 대답:  
  bit 0~3은 어떤 자릿수(D1~D4)를 켤 것인지를 정하는 부분이고 bit 4~11은 해당 자릿수에 어떤 숫자 패턴(seg_nun 패턴)을 출력할 것인지에 대해서 정하는 부분이다.
  따라서, D1은 하위 4비트 위치에 그대로 들어가고 `seg_num[1]`은 4번째 비트부터 시작하여 들어가게 된다. 결국 16비트 데이터 안에 `어디에` `어떤 패턴`에 대한 정보가 겹치지 않게 들어가게 된다.

#### 질문 2. `write(dev, 0x0000,2)`가 아닌 변수를 사용한 이유.

- 대답 : 
  write의 원형을 보면 `ssize_t write(int fd, const void *buf, size_t count)`인데 여기서 두 번째 인자인 `buf`는 데이터가 저장되어 있는 메모리의 주소여야한다.
  만약 상수(0x0000)을 직접 넣게 되면 컴파일러는 0x0000를 주소로 받아들이기 때문에 segmantation fault가 발생하거나, EFAULT 에러를 반환하게 된다. 따라서 변수에 0을 저장하고 변수의 주소를 전달해주어야 한다.

### seg_driver.c

이 코드는 유저 영역에서 보낸 16비트 데이터를 받아 각 비트 상태에 따라 라즈베리 파이의 GPIO 핀 12개를 각각 제어하는 문자 디바이스 드라이버이다.

```d
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Johannes 4 GNU/Linux");
MODULE_DESCRIPTION("A simple gpio Driver for segments");

/* 가상 장치 파일 정보 */
#define DRIVER_NAME "my_segment"
#define DRIVER_CLASS "MyModulesClass_seg"

static dev_t my_device_nr;
static struct class *my_class;
static struct cdev my_device;

/**
 * 유저가 전송한 데이터를 하드웨어(GPIO) 신호로 변환환
 */
static ssize_t driver_write(struct file *File, const char *user_buffer, size_t count, loff_t *offs) {
    int to_copy, not_copied, delta;
    unsigned short value = 0;

    // 복사할 크기 결정
    to_copy = min(count, sizeof(value));

    // 유저 공간의 데이터를 커널 공간 변수 'value'로 안전하게사
    not_copied = copy_from_user(&value, user_buffer, to_copy);

    /* 비트 마스킹을 통한 GPIO 제어
    * value의 각 비트(0~11)가 1인지 0인지 검사하여 해당 GPIO 핀을 High(1)/Low(0)로 설정한다. 
      * 예: (value & (1 << 0)) -> value의 0번 비트가 1이면 GPIO 2번 핀을 켠다.
    */
    if (value & (1 << 0)) {gpio_set_value(2, 1);} 
    else {gpio_set_value(2, 0);}

    if (value & (1 << 1)) {gpio_set_value(3, 1);} 
    else {gpio_set_value(3, 0);}

    if (value & (1 << 2)) {gpio_set_value(4, 1);}
    else {gpio_set_value(4, 0);}

    if (value & (1 << 3)) {gpio_set_value(17, 1);} 
    else {gpio_set_value(17, 0);}

    if (value & (1 << 4)) {gpio_set_value(21, 1);} 
    else {gpio_set_value(21, 0);}

    if (value & (1 << 5)) {gpio_set_value(20, 1);}
	else {gpio_set_value(20, 0);}

    if (value & (1 << 6)) {gpio_set_value(16, 1);} 
    else {gpio_set_value(16, 0);}

    if (value & (1 << 7)) {
        gpio_set_value(12, 1);
    } else {
        gpio_set_value(12, 0);
    }

    if (value & (1 << 8)) {
        gpio_set_value(7, 1);
    } else {
        gpio_set_value(7, 0);
    }

    if (value & (1 << 9)) {
        gpio_set_value(8, 1);
    } else {
        gpio_set_value(8, 0);
    }

    if (value & (1 << 10)) {
        gpio_set_value(25, 1);
    } else {
        gpio_set_value(25, 0);
    }

    if (value & (1 << 11)) {
        gpio_set_value(24, 1);
    } else {
        gpio_set_value(24, 0);
    }
	// 반환값 계산 (실제로 복사된 바이트 수)
    delta = to_copy - not_copied;
    return delta;
}

static int driver_open(struct inode *device_file, struct file *instance) {
    printk("segment - open was called!\n");
    return 0;
}

static int driver_close(struct inode *device_file, struct file *instance) {
    printk("segment - close was called!\n");
    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = driver_open,
    .release = driver_close,
    //.read = driver_read,
    .write = driver_write
};
/** 
  * [초기화 함수] 드라이버 로드 시 시스템 리소스 확보 및 장치 등록 
  */
static int __init ModuleInit(void) {
    printk("Hello, Kernel!\n");

    /* 1. 주번호 동적 할당: 커널로부터 비어있는 주번호를 자동으로옴 */
    if (alloc_chrdev_region(&my_device_nr, 0, 1, DRIVER_NAME) < 0) {
        printk("Device Nr. could not be allocated!\n");
        return -1;
    }
    printk("read_write - Device Nr. Major : %d, Minor : %d was registered!\n", my_device_nr >> 20, my_device_nr && 0xfffff);

    /* 2. 장치 클래스 생성: /sys/class/ 하위에 드이버 정보를 등록 */
    if ((my_class = class_create(THIS_MODULE, DRIVER_CLASS)) == NULL) {
        printk("Device class can not created!\n");
        goto ClassError;
    }

    /* 3. 장치 파일 생성: /dev/my_segment 파일을 자동으로 만들어줌*/
    if (device_create(my_class, NULL, my_device_nr, NULL, DRIVER_NAME) == NULL) {
        class_destroy(my_class);
        printk("Can not creat device file!\n");
        goto FileError;
    }

    /* 4. 문자 장치(cdev) 초기화 및 커널 등록: 시스템 콜 fops와 연결결*/ 
    cdev_init(&my_device, &fops);
    if (cdev_add(&my_device, my_device_nr, 1) < 0) {
        printk("Can not create device file!\n");
        goto AddError;
    }
    
    /* 5. GPIO 요청 및 방향 설정: 커널에 해당 핀 사용을 알리고 출력 모드로 설정*/
    /* Set D1~4 segments GPIO*/
    // GPIO 2 init
    if (gpio_request(2, "rpi-gpio-2")) {
        printk("Can not allocate GPIO 2\n");
        goto AddError;
    }

    /* Set GPIO 2 direction */
    if (gpio_direction_output(2, 0)) {
        printk("Can not set GPIO 2 to output\n");
        goto Gpio2Error;
    }

    // GPIO 3 init
    if (gpio_request(3, "rpi-gpio-3")) {
        printk("Can not allocate GPIO 3\n");
        goto AddError;
    }

    /* Set GPIO 3 direction */
    if (gpio_direction_output(3, 0)) {
        printk("Can not set GPIO 3 to output\n");
        goto Gpio3Error;
    }

    // GPIO 4 init
    if (gpio_request(4, "rpi-gpio-4")) {
        printk("Can not allocate GPIO 4\n");
        goto AddError;
    }

    /* Set GPIO 4 direction */
    if (gpio_direction_output(4, 0)) {
        printk("Can not set GPIO 4 to output\n");
        goto Gpio4Error;
    }

    // GPIO 17 init
    if (gpio_request(17, "rpi-gpio-17")) {
        printk("Can not allocate GPIO 17\n");
        goto AddError;
    }

    /* Set GPIO 17 direction */
    if (gpio_direction_output(17, 0)) {
        printk("Can not set GPIO 17 to output\n");
        goto Gpio17Error;
    }

    /* Set A~DP segments GPIO*/
    // GPIO 21 init
    if (gpio_request(21, "rpi-gpio-21")) {
        printk("Can not allocate GPIO 21\n");
        goto AddError;
    }

    /* Set GPIO 21 direction */
    if (gpio_direction_output(21, 0)) {
        printk("Can not set GPIO 21 to output\n");
        goto Gpio21Error;
    }

    // GPIO 20 init
    if (gpio_request(20, "rpi-gpio-20")) {
        printk("Can not allocate GPIO 20\n");
        goto AddError;
    }

    /* Set GPIO 20 direction */
    if (gpio_direction_output(20, 0)) {
        printk("Can not set GPIO 20 to output\n");
        goto Gpio20Error;
    }

    // GPIO 16 init
    if (gpio_request(16, "rpi-gpio-16")) {
        printk("Can not allocate GPIO 16\n");
        goto AddError;
    }

    /* Set GPIO 16 direction */
    if (gpio_direction_output(16, 0)) {
        printk("Can not set GPIO 16 to output\n");
        goto Gpio16Error;
    }

    // GPIO 12 init
    if (gpio_request(12, "rpi-gpio-12")) {
        printk("Can not allocate GPIO 12\n");
        goto AddError;
    }

    /* Set GPIO 12 direction */
    if (gpio_direction_output(12, 0)) {
        printk("Can not set GPIO 12 to output\n");
        goto Gpio12Error;
    }

    // GPIO 7 init
    if (gpio_request(7, "rpi-gpio-7")) {
        printk("Can not allocate GPIO 7\n");
        goto AddError;
    }

    /* Set GPIO 7 direction */
    if (gpio_direction_output(7, 0)) {
        printk("Can not set GPIO 7 to output\n");
        goto Gpio7Error;
    }

    // GPIO 8 init
    if (gpio_request(8, "rpi-gpio-8")) {
        printk("Can not allocate GPIO 8\n");
        goto AddError;
    }

    /* Set GPIO 8 direction */
    if (gpio_direction_output(8, 0)) {
        printk("Can not set GPIO 8 to output\n");
        goto Gpio8Error;
    }

    // GPIO 25 init
    if (gpio_request(25, "rpi-gpio-25")) {
        printk("Can not allocate GPIO 25\n");
        goto AddError;
    }

    /* Set GPIO 25 direction */
    if (gpio_direction_output(25, 0)) {
        printk("Can not set GPIO 25 to output\n");
        goto Gpio25Error;
    }

    // GPIO 24 init
    if (gpio_request(24, "rpi-gpio-24")) {
        printk("Can not allocate GPIO 24\n");
        goto AddError;
    }

    /* Set GPIO 24 direction */
    if (gpio_direction_output(24, 0)) {
        printk("Can not set GPIO 24 to output\n");
        goto Gpio24Error;
    }

    return 0;

Gpio2Error:
    gpio_free(2);
Gpio3Error:
    gpio_free(3);
Gpio4Error:
    gpio_free(4);
Gpio17Error:
    gpio_free(17);
Gpio21Error:
    gpio_free(21);
Gpio20Error:
    gpio_free(20);
Gpio16Error:
    gpio_free(16);
Gpio12Error:
    gpio_free(12);
Gpio7Error:
    gpio_free(7);
Gpio8Error:
    gpio_free(8);
Gpio25Error:
    gpio_free(25);
Gpio24Error:
    gpio_free(24);
AddError:
    device_destroy(my_class, my_device_nr);
FileError:
    class_destroy(my_class);
ClassError:
    unregister_chrdev_region(my_device_nr,1);
    return -1;
}

static void __exit ModuleExit(void) {
    gpio_set_value(2,0);
    gpio_set_value(3,0);
    gpio_set_value(4,0);
    gpio_set_value(17,0);
    gpio_set_value(21,0);
    gpio_set_value(20,0);
    gpio_set_value(16,0);
    gpio_set_value(12,0);
    gpio_set_value(7,0);
    gpio_set_value(8,0);
    gpio_set_value(25,0);
    gpio_set_value(24,0);
    gpio_free(2);
    gpio_free(3);
    gpio_free(4);
    gpio_free(17);
    gpio_free(21);
    gpio_free(20);
    gpio_free(16);
    gpio_free(12);
    gpio_free(7);
    gpio_free(8);
    gpio_free(25);
    gpio_free(24); 
    cdev_del(&my_device); 
    device_destroy(my_class, my_device_nr);
    class_destroy(my_class); 
    unregister_chrdev_region(my_device_nr, 1); 
    printk("Goodbye, Kernel\n");
}

module_init(ModuleInit);
module_exit(ModuleExit);

# 핵심 포인트
- 동적 주번호 할당 (`alloc_chrdev_region`): 다른 장치와 번호가 충돌하는 것을 방지하기 위해 이 함수를 사용하여 커널이 남는 번호를 주도록 설계했다.
    
- 자동 노드 생성 (`class_create`, `device_create`): 이 부분이 있으면 유저가 일일이 `mknod` 명령어를 칠 필요가 없다. 드라이버가 로드되자마자 `/dev/my_segment`가 나타나죠.
    
- Error Handling (Goto Labels): 에러가 발생하면 즉시 아래쪽에 준비된 '해제 구역'으로 점프한다. 'goto' 방식을 사용하였기에 중간에 실패하더라도 역순으로 해제하기 때문에 시스템이 꼬이지 않고 원상복구된다.
    
- Bitwise 제어 로직: `value & (1 << n)` 방식을 사용한다. 유저가 보낸 하나의 `short` 데이터 안에 12개의 스위치 상태를 압축해서 보냈고, 드라이버는 이를 하나씩 풀어서 하드웨어 핀에 적용한다.
```


### Makefile

```d
# 1. 커널 모듈 오브젝트 파일 정의
# 빌드 후 'seg_driver.ko'라는 커널 모듈 파일이 생성되도록 설정한다.
obj-m += seg_driver.o

# 2. 결과물 및 소스 파일 변수 설정
# 유저 애플리케이션의 이름과 소스 코드를 변수로 관리하여 유지보수를 편리하게 한다.
RESULT1 = seg_example1
RESULT2 = seg_example2
SRC1 = $(RESULT1).c
SRC2 = $(RESULT2).c

# 3. 환경 변수 설정
# PWD: 현재 작업 중인 디렉토리의 절대 경로를 자동으로 가져온다.
# KDIR: 라즈베리 파이 커널 소스가 위치한 경로 (사용자 환경에 맞춰 수정됨).
PWD := $(shell pwd)
KDIR = /home/linux123/working/kernel

# 4. 전체 빌드 타겟 (all)
# 'make' 명령어를 입력했을 때 'modules'와 'apps' 두 타겟을 차례로 실행한다.
all: modules apps

# 5. 커널 모듈 빌드 타겟
# 커널 빌드 시스템(Kbuild)을 호출하여 디바이스 드라이버를 컴파일한다.
# -C: 커널 소스 경로로 이동하여 해당 위치의 Makefile을 실행한다.
# M=$(PWD): 현재 디렉토리의 소스를 사용하여 모듈을 만들라는 옵션이다.
modules: 
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# 6. 유저 애플리케이션 빌드 타겟
# 크로스 컴파일러(aarch64-linux-gnu-gcc)를 사용하여 64비트 ARM 바이너리를 생성한다.
apps:
	aarch64-linux-gnu-gcc -o $(RESULT1) $(SRC1)
	aarch64-linux-gnu-gcc -o $(RESULT2) $(SRC2)

# 7. 정리 규칙 (clean)
# 빌드 과정에서 생성된 중간 파일(.o, .mod 등)과 실행 파일을 모두 삭제한다.
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(RESULT1) $(RESULT2)
```


## 실습

### 실습 결과 분석

드라이버의 비트 구조:

- **Bit 0~3**: Digit 1, 2, 3, 4 선택 (1이면 해당 자리 활성화)
    
- **Bit 4~11**: Segment A, B, C, D, E, F, G, DP 제어 (**0이면 켜짐**, 1이면 꺼짐)
    

#### ① `# ./seg_example` (인자 없음)

- **결과**: `put arg 0x0000 or int` 메시지 출력 후 종료.
    
- **이유**: 소스 코드의 `if (argc < 2)` 조건에 걸려 사용법을 알려주고 프로그램이 실행되지 않는다.
    

#### ② `# ./seg_example 0x000f`

- **비트**: `0000 0000 0000 1111` (2진수)
    
- **분석**:
    
    - 하위 4비트(`1111`): D1, D2, D3, D4 모두 **활성화**.
        
    - 상위 8비트(`0000 0000`): 모든 세그먼트(A~DP) 비트가 0이므로 **모두 점등**.
        
- **결과**: **모든 자리에 숫자 '8'과 점(.)이 꽉 채워져서 나온다.** (`8. 8. 8. 8.`)
    

#### ③ `# ./seg_example 0x00ff`

- **비트**: `0000 0000 1111 1111` (2진수)
    
- **분석**:
    
    - 하위 4비트(`1111`): D1, D2, D3, D4 모두 **활성화**.
        
    - 중간 4비트(`1111`): A, B, C, D 세그먼트 비트가 1이므로 **모두 소등**.
        
    - 상위 4비트(`0000`): E, F, G, DP 세그먼트 비트가 0이므로 **모두 점등**.
        
- **결과**: `ㅏ` 모양
    

#### ④ `# ./seg_example 0x0f0f`

- **비트**: `0000 1111 0000 1111` (2진수)
    
- **분석**:
    
    - 하위 4비트(`1111`): D1~D4 **활성화**.
        
    - 중간 4비트(`0000`): A, B, C, D 세그먼트 비트가 0이므로 **모두 점등**.
        
    - 상위 4비트(`1111`): E, F, G, DP 세그먼트 비트가 1이므로 **모두 소등**.
        
- **결과**: 역 C 모양
    

#### ⑤ `# ./seg_example 15`

- **비트**: 10진수 15는 16진수로 `0x000f`입니다.
    
- **결과**: ②번(`0x000f`)과 동일하게 모든 자리에 '8.'이 출력된다.

|**비트**|**11**|**10**|**9**|**8**|**7**|**6**|**5**|**4**|**3**|**2**|**1**|**0**|
|---|---|---|---|---|---|---|---|---|---|---|---|---|
|**역할**|DP|G|F|E|D|C|**B**|**A**|D4|D3|D2|D1|
|**값**|2048|1024|512|256|128|64|**32**|**16**|8|4|2|1|

## Homework

아래의 조건을 만족하는 up/down counter를 설계하시오.
1. counte는 segments에 숫자를 출력하며, 0000~9999 까지 표시한다.
2. 0000에서 count down시 9999로, 9999에서 count up시 0000으로 이어진다.
3. keyboard `[u], [d], ,[p], [q]`로  각각  count up, count down, count setting, program quit로 동작한다.
4. 두개의 button으로 각각 count up, count down으로 동작한다.
5. segments와 button을 각각 별도의 커널모듈로 제작하고, 하나의 응용프로그램으로  실행한다.

### Makefile

```d
obj-m += hw_seg_driver.o 
obj-m += hw_btn_driver.o
APP = hw_app
PWD := $(shell pwd)

all: modules apps
# 커널 모듈 빌드
modules: 
	make -C /home/linux123/working/kernel M=$(PWD) modules
# 유저 응용 프로그램 빌드(gcc)
apps:
	aarch64-linux-gnu-gcc -o $(APP) $(APP).c

clean:
	make -C /home/linux123/working/kernel M=$(PWD) clean
	rm -f $(APP)
```

- obj-m에 `.o`파일을 추가하면 동일한 이름의 `.c`파일을 찾아 컴파일한 뒤 `.ko`(Kernel Object) 파일을 생성한다.
- `M=$(PWD)`: 커널 빌드 시스템에서 실제 드라이버 소스 위치를 알려주고 컴파일 결과가 동일한 위치에 생성되도록 한다.
- 유저 어플리케이션은 라즈베리파이에서 실행되도록 크로스 컴파일을 진행한다.
### seg_driver.c

```d
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Super User Teacher");
MODULE_DESCRIPTION("7-Segment Display Driver");

#define DRIVER_NAME "my_segment" // /dev/ 아래 생길 이름
#define DRIVER_CLASS "MyModulesClass_seg" // sysfs에 등록될 클래스 이름

static dev_t my_device_nr; // 장치 번호 저장 변수
static struct class *my_class; 
static struct cdev my_device;

// GPIO 핀 정의 (자릿수 4개, 세그먼트 8개 순서)
static unsigned int seg_gpios[] = {2, 3, 4, 17, 21, 20, 16, 12, 7, 8, 25, 24};
static const char *seg_names[] = {"D1", "D2", "D3", "D4", "A", "B", "C", "D", "E", "F", "G", "DP"};

// write() 호출 시 실행
static ssize_t driver_write(struct file *File, const char *user_buffer, size_t count, loff_t *offs) {
    unsigned short value = 0;
    int i;

	// 유저 공간의 데이터를 커널 공간의 value 변수로 안전하게 복사
    if (copy_from_user(&value, user_buffer, sizeof(unsigned short))) return -EFAULT;

    // 12개 비트를 순회하며 각 GPIO에 상태 0 or 1 적용
    for (i = 0; i < 12; i++) {
        gpio_set_value(seg_gpios[i], (value >> i) & 0x01);
    }
    return sizeof(unsigned short);
}

static int driver_open(struct inode *device_file, struct file *instance) { return 0; }
static int driver_close(struct inode *device_file, struct file *instance) { return 0; }

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = driver_open,
    .release = driver_close,
    .write = driver_write
};

static int __init ModuleInit(void) {
    int i;
    if (alloc_chrdev_region(&my_device_nr, 0, 1, DRIVER_NAME) < 0) return -1;
    if ((my_class = class_create(THIS_MODULE, DRIVER_CLASS)) == NULL) goto ClassError;
    if (device_create(my_class, NULL, my_device_nr, NULL, DRIVER_NAME) == NULL) goto FileError;

    cdev_init(&my_device, &fops);
    if (cdev_add(&my_device, my_device_nr, 1) < 0) goto AddError;

    // GPIO 일괄 요청 및 설정
    for (i = 0; i < 12; i++) {
        if (gpio_request(seg_gpios[i], seg_names[i])) goto GpioError;
        gpio_direction_output(seg_gpios[i], 0);
    }
    return 0;

GpioError:
    while (i--) gpio_free(seg_gpios[i]);
AddError: device_destroy(my_class, my_device_nr);
FileError: class_destroy(my_class);
ClassError: unregister_chrdev_region(my_device_nr, 1);
    return -1;
}

static void __exit ModuleExit(void) {
    int i;
    for (i = 0; i < 12; i++) {
        gpio_set_value(seg_gpios[i], 0);
        gpio_free(seg_gpios[i]);
    }
    cdev_del(&my_device);
    device_destroy(my_class, my_device_nr);
    class_destroy(my_class);
    unregister_chrdev_region(my_device_nr, 1);
}

module_init(ModuleInit);
module_exit(ModuleExit);
```

드라이버 설계 핵심: 유저 프로그램은 어떤 GPIO가 몇 번인지 몰라도 16비트 값만 넘기면 드라이버가 알아서 해석해 전기 신호로 바꾼다.

### btn_driver.c (물리적인 버튼)

```d
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Super User Teacher");
MODULE_DESCRIPTION("GPIO Button Driver for Up/Down Counter");

#define DRIVER_NAME "my_button"
#define DRIVER_CLASS "MyModulesClass_btn"

static dev_t btn_dev;
static struct class *btn_class;
static struct cdev btn_cdev;

// 버튼 GPIO 핀 정의 (23: Up, 22: Down)
static unsigned int btn_gpios[] = {23, 22};
static const char *btn_names[] = {"BTN_UP", "BTN_DOWN"};

/**
 * read 시스템 콜: 현재 버튼의 눌림 상태를 1바이트 데이터로 전송
 */
 // read() 호출 시 실행
static ssize_t btn_read(struct file *File, char __user *user_buffer, size_t count, loff_t *offs) {
    unsigned char btn_state = 0;
    int i;

    // 각 GPIO 핀의 값을 읽어 비트 단위로 저장
    for (i = 0; i < 2; i++) {
        // gpio_get_value가 1이면 눌리지 않음, 0이면 눌림 (Pull-up 기준)
        // 여기서는 눌렸을 때 해당 비트를 1로 설정
        if (gpio_get_value(btn_gpios[i]) == 0) {
            btn_state |= (1 << i);
        }
    }

    if (copy_to_user(user_buffer, &btn_state, sizeof(btn_state))) return -EFAULT;
    return sizeof(btn_state);
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = btn_read,
};

static int __init btn_init(void) {
    int i;
    if (alloc_chrdev_region(&btn_dev, 0, 1, DRIVER_NAME) < 0) return -1;
    if ((btn_class = class_create(THIS_MODULE, DRIVER_CLASS)) == NULL) goto ClassError;
    if (device_create(btn_class, NULL, btn_dev, NULL, DRIVER_NAME) == NULL) goto FileError;

    cdev_init(&btn_cdev, &fops);
    if (cdev_add(&btn_cdev, btn_dev, 1) < 0) goto AddError;

    // GPIO 설정: 입력 모드로 설정
    for (i = 0; i < 2; i++) {
        if (gpio_request(btn_gpios[i], btn_names[i])) goto GpioError;
        gpio_direction_input(btn_gpios[i]); // 입력 방향 설정
    }
    return 0;

GpioError:
    while (i--) gpio_free(btn_gpios[i]);
AddError: device_destroy(btn_class, btn_dev);
FileError: class_destroy(btn_class);
ClassError: unregister_chrdev_region(btn_dev, 1);
    return -1;
}

static void __exit btn_exit(void) {
    int i;
    for (i = 0; i < 2; i++) gpio_free(btn_gpios[i]);
    cdev_del(&btn_cdev);
    device_destroy(btn_class, btn_dev);
    class_destroy(btn_class);
    unregister_chrdev_region(btn_dev, 1);
}

module_init(btn_init);
module_exit(btn_exit);
```

드라이버 설계 핵심: GPIO를 입력 모드로 설정하고, 하드웨어의 전압 상태를 비트 데이터로 변환하여 유저 공간으로 안전하게 보낸다.


### count_app.c (키보드+버튼)

```d
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

unsigned char seg_num[10] = {0xc0, 0xf9, 0xa4, 0xb0, 0x99, 0x92, 0x82, 0xd8, 0x80, 0x90};
#define D1 0x01
#define D2 0x02
#define D3 0x04
#define D4 0x08

static struct termios init_setting, new_setting;

void init_keyboard() {
    tcgetattr(STDIN_FILENO, &init_setting);
    new_setting = init_setting;
    new_setting.c_lflag &= ~(ICANON | ECHO);// 입력 글자 즉시 읽기
    new_setting.c_cc[VMIN] = 0; // 입력이 없어도 기다리지 않고 즉시 반환
    new_setting.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_setting);
}

void close_keyboard() {
    tcsetattr(STDIN_FILENO, TCSANOW, &init_setting);
}

int main() {
	char key;
    int seg_fd, btn_fd;
    int count = 0;
    int i;
    unsigned char btn_state, prev_btn_state = 0;
    unsigned short display_data[4];

    seg_fd = open("/dev/my_segment", O_RDWR);
    btn_fd = open("/dev/my_button", O_RDONLY);

    if (seg_fd < 0 || btn_fd < 0) {
        perror("Device Open Failed");
        return -1;
    }

    init_keyboard();
    printf("Counter Started! [u]:Up, [d]:Down, [p]:Reset, [q]:Quit\n");

    while (1) {
        // 1. 키보드 입력 처리
        if (read(STDIN_FILENO, &key, 1) == 1) {
            if (key == 'q') break;
            else if (key == 'u') count = (count + 1) % 10000;
            else if (key == 'd') count = (count + 9999) % 10000;
            else if (key == 'p') count = 0;
        }

        // 2. 버튼 입력 처리 (에지 트리거: 눌리는 순간만 감지)
        if (read(btn_fd, &btn_state, sizeof(btn_state)) > 0) {
            // Up 버튼 (0번 비트) 체크: 이전엔 0이었고 지금은 1인 경우
            if ((btn_state & 0x01) && !(prev_btn_state & 0x01)) {
                count = (count + 1) % 10000;
            }
            // Down 버튼 (1번 비트) 체크
            if ((btn_state & 0x02) && !(prev_btn_state & 0x02)) {
                count = (count + 9999) % 10000;
            }
            prev_btn_state = btn_state;
        }

        // 3. 자릿수 분해 및 디스플레이 데이터 생성
        int temp_count = count;
        int digits[4] = {temp_count/1000, (temp_count/100)%10, (temp_count/10)%10, temp_count%10};
        int d_sel[4] = {D1, D2, D3, D4};

        for (i = 0; i < 4; i++) {
            display_data[i] = (seg_num[digits[i]] << 4) | d_sel[i];
            
            // 4. 멀티플렉싱 출력 (잔상 효과)
            write(seg_fd, &display_data[i], 2);
            usleep(2000); // 2ms씩 각 자리를 켬 (전체 약 125Hz)
        }
    }

    close_keyboard();
    unsigned short off = 0;
    write(seg_fd, &off, 2); // 종료 시 끄기
    close(seg_fd);
    close(btn_fd);
    return 0;
}
```

### up/down이 단순한 mod연산으로 가능한 이유

**핵심 동작 원리**

#### 1 자릿수 분리와 패턴 매핑

- **자릿수 추출**: 4자리 정수(예: 3312)를 산술 연산(`/`, `%`)을 통해 개별 숫자(3, 3, 1, 2)로 쪼갠다.
    
- **패턴 매핑**: 쪼개진 숫자를 미리 정의된 `seg_num[]` 배열의 인덱스로 사용하여 해당 숫자의 LED 모양(비트 패턴)을 가져온다.
    

#### 2 멀티플렉싱(Multiplexing)과 잔상 효과

- 7-세그먼트의 데이터 선은 4자리가 공유하므로 한 번에 한 자리만 켤 수 있다.
    
- 자릿수 선택 신호(D1~D4)를 아주 빠르게(약 2ms 간격) 순차적으로 변경하며 해당 모양을 출력한다.
    
- 인간의 눈에 발생하는 잔상 효과(POV)를 이용해 4자리가 동시에 켜진 것처럼 보이게 한다.
    

#### 3 Up/Down 순환 로직

- **Up Count**: `count = (count + 1) % 10000` 공식을 사용하여 9999 다음 0이 나오게 한다.
    
- **Down Count**: `count = (count + 9999) % 10000` 공식을 사용하여 0 이전 9999가 나오게 한다. (음수 방지)

### 진행 과정

### 1단계: 개발 PC에서 컴파일 수행 (Cross-Compilation)

makefile 이용해서 커널 드라이버 및 응용프로그램 빌드 

```d
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
```

---

### 2단계: 개발 PC에서 Base64 인코딩

빌드된 3개의 파일을 텍스트 형태인 `base64`로 변환한다.

```d
base64 hw_seg_driver.ko > seg.txt
base64 hw_btn_driver.ko > btn.txt
base64 hw_app > app.txt
```

- 각 `.txt` 파일의 내용을 열어서 전체 복사할 준비를 한다.
    
---

### 3단계: 라즈베리 파이로 전송 및 디코딩

시리얼 터미널(minicom 등)을 통해 라즈베리 파이에 접속하여 다음 과정을 수행한다.

#### 3.1 텍스트 파일 생성 및 붙여넣기

각 파일에 대해 아래 명령어를 실행하고 복사한 내용을 붙여넣는다.

```d
# 세그먼트 드라이버
cat > seg.txt
(복사한 seg.txt 내용 붙여넣기)
[Ctrl + D] 누름

# 버튼 드라이버
cat > btn.txt
(복사한 btn.txt 내용 붙여넣기)
[Ctrl + D] 누름

# 응용 프로그램
cat > app.txt
(복사한 app.txt 내용 붙여넣기)
[Ctrl + D] 누름
```

#### 3.2 바이너리 파일로 복원 (Decoding)

텍스트 파일을 다시 실행 가능한 파일로 복원한다.

```d
base64 -d seg.txt > hw_seg_driver.ko
base64 -d btn.txt > hw_btn_driver.ko
base64 -d app.txt > hw_app
```

---

### 4단계: 드라이버 로드 및 응용 프로그램 실행

#### 4.1 권한 설정 및 드라이버 로드

응용 프로그램에 실행 권한을 부여하고 커널 모듈을 로드한다.

```d
chmod +x hw_app
sudo insmod hw_seg_driver.ko
sudo insmod hw_btn_driver.ko
```

- `hw_seg_driver.ko`는 GPIO 2, 3, 4, 17, 21, 20, 16, 12, 7, 8, 25, 24번 핀을 사용한다.
    
- `hw_btn_driver.ko`는 GPIO 23, 22번 핀을 사용한다.
    

#### 4.2 하드웨어 버튼 연결

버튼 드라이버의 로직(Active Low)에 맞춰 다음과 같이 연결한다.

- **Up 버튼**: GPIO 23(16번 핀)과 GND 사이에 연결한다.
    
- **Down 버튼**: GPIO 22(15번 핀)과 GND 사이에 연결한다.
    

#### 4.3 프로그램 실행

```d
./hw_app
```

- `/dev/my_segment`와 `/dev/my_button` 장치 파일을 열어 동작을 시작한다.
    
- 키보드 'u', 'd', 'p', 'q' 또는 물리 버튼을 사용하여 카운트를 확인한다.


## Homework 분석

- 입출력 분리
	기존 방식: 하나의 파일 안에서 GPIO 설정과 로직을 모두 처리함
	homework: hw_seg_driver는 쓰기 전용, hw_btn_driver는 읽기 전용으로 역할을 분리하였다.  

- Non-blocking 입력 감시 
	기존 방식: 키보드 입력을 기다리면 프로그램이 멈춰 세그먼트가 꺼지는 문제가 발생할 수 있었다.
	homework: `new_setting.c_cc[VMIN] = 0` 설정을 통해 키보드 입력이 없어도 즉시 다음 코드로 넘어가도록 설계하여, 세그먼트의 잔상 효과가 끊기지 않게 유지한다.

드라이버는 하드웨어 인터페이스(GPIO)만 담당한다.
앱은 수학적 연산과 입력 동기화를 담당한다.
사용자는 키보드와 버튼 중 편한 방식을 선택하여 기기를 제어한다.
