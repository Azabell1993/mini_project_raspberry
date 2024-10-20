#include <iostream>
#include <gst/gst.h>
#include <thread>
#include <mutex>
#include <memory>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>

std::mutex print_mutex;
std::atomic<bool> keep_running(true); // 서버 상태를 감시하는 변수

class SmartPtr
{
public:
    SmartPtr(GstElement *ptr) : ptr(ptr), ref_count(new int(1)), mtx(new std::mutex) {}

    SmartPtr(const SmartPtr &other)
    {
        std::lock_guard<std::mutex> lock(*other.mtx);
        ptr = other.ptr;
        ref_count = other.ref_count;
        mtx = other.mtx;
        ++(*ref_count);
    }

    ~SmartPtr()
    {
        release();
    }

    SmartPtr &operator=(const SmartPtr &other)
    {
        if (this != &other)
        {
            release();
            std::lock_guard<std::mutex> lock(*other.mtx);
            ptr = other.ptr;
            ref_count = other.ref_count;
            mtx = other.mtx;
            ++(*ref_count);
        }
        return *this;
    }

    GstElement *get() const
    {
        return ptr;
    }

private:
    GstElement *ptr;
    int *ref_count;
    std::mutex *mtx;

    void release()
    {
        std::lock_guard<std::mutex> lock(*mtx);
        if (--(*ref_count) == 0)
        {
            gst_object_unref(ptr);
            delete ref_count;
            delete mtx;
        }
    }
};

void safe_print(const std::string &msg)
{
    std::lock_guard<std::mutex> lock(print_mutex);
    std::cout << msg << std::endl;
}

bool is_server_available(const std::string &server_ip, int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        safe_print("Failed to create socket.");
        return false;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0)
    {
        safe_print("Invalid address/ Address not supported.");
        close(sock);
        return false;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        close(sock);
        return false;
    }

    close(sock);
    return true;
}

void monitor_server(const std::string &server_ip, int server_port)
{
    while (keep_running)
    {
        if (!is_server_available(server_ip, server_port))
        {
            safe_print("Server disconnected. Shutting down client...");
            keep_running = false; // 서버가 더 이상 사용 가능하지 않음
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(5)); // 주기적으로 서버 상태 확인
    }
}

void gstreamer_client(const std::string &server_ip)
{
    gst_init(nullptr, nullptr);
    std::string pipeline_desc = "tcpclientsrc host=" + server_ip + " port=5000 ! multipartdemux ! jpegdec ! autovideosink";
    SmartPtr pipeline(gst_parse_launch(pipeline_desc.c_str(), nullptr));

    if (!pipeline.get())
    {
        safe_print("Failed to create the GStreamer pipeline.");
        return;
    }

    gst_element_set_state(pipeline.get(), GST_STATE_PLAYING);
    safe_print("Client connected and receiving stream...");

    while (keep_running)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1)); // 서버가 계속 켜져 있는 동안 스트리밍 유지
    }

    gst_element_set_state(pipeline.get(), GST_STATE_NULL);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <server_ip>" << std::endl;
        return 1;
    }

    std::string server_ip = argv[1];
    int server_port = 5000; // GStreamer 서버 포트

    if (is_server_available(server_ip, server_port))
    {
        std::thread monitor_thread(monitor_server, server_ip, server_port); // 서버 상태를 감시하는 스레드
        std::thread client_thread(gstreamer_client, server_ip);             // GStreamer 클라이언트 스레드

        client_thread.join();
        monitor_thread.join();
    }
    else
    {
        safe_print("Cannot connect to the server. Exiting.");
    }

    return 0;
}
