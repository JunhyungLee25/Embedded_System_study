#include <stdio.h>    
#include <stdlib.h>    
#include <string.h>    
#include <unistd.h>   
#include <fcntl.h>      

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
    }

    close(dev);
    return 0;
}