// g++ -o web_cap_server web_cap_server.cpp $(pkg-config --cflags --libs opencv4 gstreamer-1.0 gstreamer-app-1.0) -lwiringPi -lpthread

#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fstream>
#include <thread>
#include <mutex>
#include <atomic>
#include <arpa/inet.h>
#include <cstring>
#include <cstdlib>

#define PORT 8080

using namespace std;

std::atomic<bool> is_streaming(false); // 스트리밍 상태를 제어하는 변수

// LED를 켜는 함수 (modprobe 사용)
void turn_on_led()
{
    int result = system("sudo modprobe azabell_led"); // azabell_led 모듈 로드
    if (result != 0)
    {
        cerr << "Error: Failed to load azabell_led module." << endl; // 모듈 로드 실패 시 에러 출력
    }
}

// LED를 끄는 함수 (modprobe 사용)
void turn_off_led()
{
    int result = system("sudo modprobe -r azabell_led"); // azabell_led 모듈 언로드
    if (result != 0)
    {
        cerr << "Error: Failed to unload azabell_led module." << endl; // 모듈 언로드 실패 시 에러 출력
    }
}

// libcamera를 사용해 이미지를 캡처하는 함수
void capture_image()
{
    // 이미지 캡처
    int result = system("libcamera-still -o /tmp/captured_image.jpg --width 640 --height 480 --nopreview");
    if (result != 0)
    {
        cerr << "Error: Failed to capture image using libcamera." << endl; // 이미지 캡처 실패 시 에러 출력
        return;
    }

    // 캡처된 이미지를 웹에서 접근 가능한 디렉토리로 복사
    result = system("cp /tmp/captured_image.jpg /home/pi/Desktop/pjt2/captured_image.jpg");
    if (result != 0)
    {
        cerr << "Error: Failed to copy image to web directory." << endl; // 이미지 복사 실패 시 에러 출력
    }
}

// 캡처된 이미지를 클라이언트에 전송하는 함수
void serve_image(int client_fd)
{
    // 이미지 파일을 읽음
    std::ifstream file("/tmp/captured_image.jpg", std::ios::binary);
    if (!file)
    {
        // 파일이 없을 경우 404 응답
        string not_found = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n404 Not Found";
        send(client_fd, not_found.c_str(), not_found.size(), 0);
        close(client_fd);
        return;
    }

    // HTTP 헤더 전송
    string header = "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\n\r\n";
    send(client_fd, header.c_str(), header.size(), 0);

    // 이미지 데이터를 클라이언트로 전송
    char buffer[1024];
    while (file.read(buffer, sizeof(buffer)))
    {
        send(client_fd, buffer, file.gcount(), 0);
    }
    file.close();
    close(client_fd); // 클라이언트 소켓 닫기
}

// HTML 파일을 전송하는 함수
void send_html(int client_fd, const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        // 파일이 없을 경우 404 응답
        send(client_fd, "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nFile not found", strlen("HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nFile not found"), 0);
        return;
    }

    // HTTP 헤더 전송
    send(client_fd, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n", strlen("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"), 0);

    // 파일 내용을 클라이언트로 전송
    char file_buffer[1024];
    while (fgets(file_buffer, sizeof(file_buffer), file) != NULL)
    {
        send(client_fd, file_buffer, strlen(file_buffer), 0);
    }

    fclose(file);
}

// 클라이언트 요청을 처리하는 함수
void *handle_client(void *arg)
{
    int client_socket = *((int *)arg);
    char buffer[1024] = {0};
    read(client_socket, buffer, 1024); // 클라이언트로부터 요청을 읽음

    printf("Received: %s\n", buffer);

    // /index.html 요청 처리
    if (strstr(buffer, "GET /index.html") != nullptr)
    {
        send_html(client_socket, "index.html"); // HTML 파일 전송
    }
    // /captured_image.jpg 요청 처리
    else if (strstr(buffer, "GET /captured_image.jpg") != nullptr)
    {
        serve_image(client_socket); // 캡처된 이미지 전송
    }
    // /load 요청 처리 (LED 켜기 및 이미지 캡처)
    else if (strstr(buffer, "GET /load") != nullptr)
    {
        turn_on_led();   // LED 켜기
        capture_image(); // 이미지 캡처
        string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nLED turned ON and image captured";
        send(client_socket, response.c_str(), response.size(), 0);
    }
    // /unload 요청 처리 (LED 끄기)
    else if (strstr(buffer, "GET /unload") != nullptr)
    {
        turn_off_led(); // LED 끄기
        string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nLED turned OFF";
        send(client_socket, response.c_str(), response.size(), 0);
    }
    else
    {
        // 알 수 없는 명령에 대한 404 응답
        string response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nCommand not found";
        send(client_socket, response.c_str(), response.size(), 0);
    }

    return nullptr;
}

// 메인 서버 함수
int main()
{
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pthread_t tid;

    // 서버 소켓 생성
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // 주소 구조체 설정
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // 소켓에 주소를 바인딩
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 클라이언트 연결 대기
    if (listen(server_fd, 3) < 0)
    {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    // 클라이언트 연결 수락 및 처리
    while (1)
    {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("accept failed");
            close(server_fd);
            exit(EXIT_FAILURE);
        }
        pthread_create(&tid, nullptr, handle_client, (void *)&new_socket); // 클라이언트 요청 처리 스레드 생성
    }

    return 0;
}