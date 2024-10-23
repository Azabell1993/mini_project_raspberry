#include <iostream>
#include <fstream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <wiringPi.h>
#include <atomic>
#include <softPwm.h>

#define PORT 8080
#define PIR 29       //BCM_GPIO 40
#define LED 28       //BCM_GPIO 20
#define MOTOR_PWM_PIN 1  // 모터 PWM 제어 핀
#define MOTOR_DIR_PIN_A 2  // 모터 방향 제어 핀 A
#define MOTOR_DIR_PIN_B 4  // 모터 방향 제어 핀 B

std::atomic<bool> led_on(false);        // LED 상태 저장
std::atomic<int> pir_status(0);         // PIR 센서 상태 저장
std::atomic<bool> is_streaming(false);  // 스트리밍 상태 저장
std::atomic<bool> motor_active(false);  // 모터 활성화 상태
std::mutex mtx;                         // 뮤텍스

// 함수 선언
void get_pinout(int client_fd);
void get_loaded_modules(int client_fd);
void search_module(int client_fd, const std::string &term);
void send_html(int client_fd, const char *filename);
// void serve_image(int client_fd);
void *handle_client(void *arg);
void send_pir_status(int client_fd);
void motorPwmControl(int gpio, int speed);
void pir_sensor_loop();

int direction = 0; // 기본 방향: 정방향 (0: 정방향, 1: 역방향)


using namespace std;

// 모터 제어를 위한 함수
void motorControlThread(int speed, int duration) {
    motor_active = true;
    motorPwmControl(MOTOR_PWM_PIN, speed);  // 모터 속도 설정
    delay(duration);                        // 일정 시간 동안 동작
    motorPwmControl(MOTOR_PWM_PIN, 0);      // 모터 정지
    motor_active = false;
}

// 모터 속도 제어 함수 (softPwmWrite로만 제어)
void motorPwmControl(int gpio, int speed) {
    softPwmWrite(gpio, speed);    /* PWM 값을 출력하여 모터 속도 제어 */
    printf("Motor running at speed: %d\n", speed);
}

// 모터 방향 제어 함수
void motorDirectionControl(int inputA, int inputB, int direction) {
    pinMode(inputA, OUTPUT);      /* Input A 핀의 출력 설정 */
    pinMode(inputB, OUTPUT);      /* Input B 핀의 출력 설정 */

    if (direction == 0) {
        // 좌회전 (정방향)
        digitalWrite(inputA, HIGH);
        digitalWrite(inputB, LOW);
        printf("Motor turning left.\n");
    } else if (direction == 1) {
        // 우회전 (역방향)
        digitalWrite(inputA, LOW);
        digitalWrite(inputB, HIGH);
        printf("Motor turning right.\n");
    }
}

// LED 켜기
void turn_on_led() {
    mtx.lock();
    int result = system("sudo modprobe azabell_led");
    if (result == 0) {
        led_on = true;
        std::thread pir_thread(pir_sensor_loop);
        pir_thread.detach();    // 메인 스레드와 분리하여 백그라운드에서 실행
    } else {
        cerr << "Error: Failed to load azabell_led module." << endl;
    }
    mtx.unlock();
}

// LED 끄기
void turn_off_led() {
    mtx.lock();
    int result = system("sudo modprobe -r azabell_led");
    if (result == 0) {
        led_on = false;  // LED 상태를 false로 설정
        digitalWrite(LED, LOW);
    } else {
        cerr << "Error: Failed to unload azabell_led module." << endl;
    }
    mtx.unlock();
}

// 적외선 센서 상태 전송
void send_pir_status(int client_fd) {
    string status = pir_status == 1 ? "Detected" : "Not Detected";

    string header = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n";
    send(client_fd, header.c_str(), header.size(), 0);
    send(client_fd, status.c_str(), status.size(), 0);
}

// 적외선 센서 루프
void pir_sensor_loop() {
    if (wiringPiSetup() == -1) return;

    pinMode(PIR, INPUT);
    pinMode(LED, OUTPUT);

    while (led_on)  // LED가 켜져 있을 때만 실행
    {
        if (digitalRead(PIR)) {
            printf("Detected\n");
            pir_status = 1;  // PIR 감지 됨
            digitalWrite(LED, HIGH);
            delay(1000);
        } else {
            printf("Not Detected\n");
            pir_status = 0;  // PIR 감지 안 됨
            digitalWrite(LED, LOW);
            delay(1000);
        }
    }
}

// HTML 파일 전송
void send_html(int client_fd, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        // 파일이 없을 경우 404 응답
        string not_found = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nFile not found";
        send(client_fd, not_found.c_str(), not_found.size(), 0);
        close(client_fd);
        return;
    }

    // HTTP 헤더 전송
    string header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
    send(client_fd, header.c_str(), header.size(), 0);

    // 파일 내용을 클라이언트로 전송
    char file_buffer[1024];
    while (fgets(file_buffer, sizeof(file_buffer), file) != NULL) {
        send(client_fd, file_buffer, strlen(file_buffer), 0);
    }

    fclose(file);
    close(client_fd);
}

// 이미지 파일 전송
// void serve_image(int client_fd) {
//     std::ifstream file("/tmp/captured_image.jpg", std::ios::binary);
//     if (!file) {
//         // 파일이 없을 경우 404 응답
//         string not_found = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n404 Not Found";
//         send(client_fd, not_found.c_str(), not_found.size(), 0);
//         close(client_fd);
//         return;
//     }

//     // HTTP 헤더 전송
//     string header = "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\n\r\n";
//     send(client_fd, header.c_str(), header.size(), 0);

//     // 이미지 데이터를 클라이언트로 전송
//     char buffer[1024];
//     while (file.read(buffer, sizeof(buffer))) {
//         send(client_fd, buffer, file.gcount(), 0);
//     }
//     file.close();
//     close(client_fd);
// }

// GPIO 핀아웃 정보 제공
void get_pinout(int client_fd) {
    string pinout_info;
    int result = system("pinout > /tmp/pinout.txt");
    if (result == 0) {
        ifstream pinout_file("/tmp/pinout.txt");
        pinout_info.assign((istreambuf_iterator<char>(pinout_file)), istreambuf_iterator<char>());
    } else {
        pinout_info = "Error: Could not retrieve GPIO pinout.";
    }

    string header = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n";
    send(client_fd, header.c_str(), header.size(), 0);
    send(client_fd, pinout_info.c_str(), pinout_info.size(), 0);
}

// 로드된 모듈 정보 제공
void get_loaded_modules(int client_fd) {
    string module_info;
    int result = system("lsmod > /tmp/modules.txt");
    if (result == 0) {
        ifstream module_file("/tmp/modules.txt");
        module_info.assign((istreambuf_iterator<char>(module_file)), istreambuf_iterator<char>());
    } else {
        module_info = "Error: Could not retrieve module list.";
    }

    string header = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n";
    send(client_fd, header.c_str(), header.size(), 0);
    send(client_fd, module_info.c_str(), module_info.size(), 0);
}

// 모듈 검색 기능
void search_module(int client_fd, const string &term) {
    string search_result;
    string command = "find /usr/src/6.6.59-azabell_pi_kernel-v9+/drivers/char/ -name '*" + term + "*.ko' > /tmp/search_result.txt";
    int result = system(command.c_str());
    if (result == 0) {
        ifstream result_file("/tmp/search_result.txt");
        search_result.assign((istreambuf_iterator<char>(result_file)), istreambuf_iterator<char>());
        if (search_result.empty()) {
            search_result = "No matching module found.";
        }
    } else {
        search_result = "Error: Could not perform search.";
    }

    string header = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n";
    send(client_fd, header.c_str(), header.size(), 0);
    send(client_fd, search_result.c_str(), search_result.size(), 0);
}

// libcamera를 사용해 이미지를 캡처하는 함수
void capture_image() {
    // 이미지 캡처
    int result = system("libcamera-still -o /tmp/captured_image.jpg --width 640 --height 480 --nopreview");
    if (result != 0) {
        cerr << "Error: Failed to capture image using libcamera." << endl; // 이미지 캡처 실패 시 에러 출력
        return;
    }

    // 캡처된 이미지를 웹에서 접근 가능한 디렉토리로 복사
    result = system("cp /tmp/captured_image.jpg /home/pi/Desktop/pjt2/captured_image.jpg");
    if (result != 0) {
        cerr << "Error: Failed to copy image to web directory." << endl; // 이미지 복사 실패 시 에러 출력
    }
}

// 서버 요청 처리
void *handle_client(void *arg) {
    int client_socket = *((int *)arg);
    char buffer[1024] = {0};
    read(client_socket, buffer, 1024);

    int gno = 1;  // 모터의 PWM 제어 핀 (wPi 5에 연결)
    int speed = 0; // 기본 속도 설정

    if (strstr(buffer, "GET /index.html") != nullptr) {
        send_html(client_socket, "index.html");
    }
    // else if (strstr(buffer, "GET /captured_image.jpg") != nullptr) {
    //    serve_image(client_socket);
    //}
    else if (strstr(buffer, "GET /load") != nullptr) {
        turn_on_led();
        capture_image();
        string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nLED 모듈이 로드되었습니다.";
        send(client_socket, response.c_str(), response.size(), 0);
    } else if (strstr(buffer, "GET /unload") != nullptr) {
        turn_off_led();
        string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nLED 모듈이 언로드되었습니다.";
        send(client_socket, response.c_str(), response.size(), 0);
    } else if (strstr(buffer, "GET /pinout") != nullptr) {
        get_pinout(client_socket);
    } else if (strstr(buffer, "GET /modules") != nullptr) {
        get_loaded_modules(client_socket);
    } else if (strstr(buffer, "GET /search?term=") != nullptr) {
        // 검색어를 추출
        string searchTerm(buffer + strlen("GET /search?term="));
        searchTerm = searchTerm.substr(0, searchTerm.find(" "));
        search_module(client_socket, searchTerm);
    } else if (strstr(buffer, "GET /pir") != nullptr) {
        send_pir_status(client_socket);  // 적외선 센서 상태 전송
    }
    
    else if (strstr(buffer, "GET /left") != nullptr && !motor_active) {
        // 좌회전
        motorDirectionControl(MOTOR_DIR_PIN_A, MOTOR_DIR_PIN_B, 0);
        std::thread motor_thread(motorControlThread, 50, 100); // 속도 50, 100ms 동안 동작
        motor_thread.detach();  // 스레드를 분리하여 실행
        string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n좌회전 중입니다.";
        send(client_socket, response.c_str(), response.size(), 0);
    } else if (strstr(buffer, "GET /right") != nullptr && !motor_active) {
        // 우회전
        motorDirectionControl(MOTOR_DIR_PIN_A, MOTOR_DIR_PIN_B, 1);
        std::thread motor_thread(motorControlThread, 50, 100); // 속도 50, 100ms 동안 동작
        motor_thread.detach();  // 스레드를 분리하여 실행
        string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n우회전 중입니다.";
        send(client_socket, response.c_str(), response.size(), 0);
    } 
    
    else {
        string response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nCommand not found";
        send(client_socket, response.c_str(), response.size(), 0);
    }

    close(client_socket);
    return nullptr;
}

// 메인 서버 함수
int main() {
    wiringPiSetup();  // wiringPi 초기화
    pinMode(MOTOR_DIR_PIN_A, OUTPUT);
    pinMode(MOTOR_DIR_PIN_B, OUTPUT);
    softPwmCreate(MOTOR_PWM_PIN, 0, 100);  // 모터 PWM 초기화

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pthread_t tid;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("accept failed");
            close(server_fd);
            exit(EXIT_FAILURE);
        }
        pthread_create(&tid, nullptr, handle_client, (void *)&new_socket);
    }

    return 0;
}
