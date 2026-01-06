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
    new_setting.c_lflag &= ~(ICANON | ECHO);
    new_setting.c_cc[VMIN] = 0;
    new_setting.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_setting);
}

void close_keyboard() {
    tcsetattr(STDIN_FILENO, TCSANOW, &init_setting);
}

int main() {
    int seg_fd, btn_fd;
    int count = 0;
    unsigned char btn_state, prev_btn_state = 0;
    char key;
    unsigned short display_data[4];
    int i;

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
            else if (key == 'p') count = 0; // count setting: 0으로 초기화
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