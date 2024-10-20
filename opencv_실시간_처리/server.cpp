#include <iostream>
#include <gst/gst.h>
#include <thread>
#include <mutex>
#include <memory>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

std::mutex print_mutex;

class SmartPtr {
public:
    SmartPtr(GstElement* ptr) : ptr(ptr), ref_count(new int(1)), mtx(new std::mutex) {}

    SmartPtr(const SmartPtr& other) {
        std::lock_guard<std::mutex> lock(*other.mtx);
        ptr = other.ptr;
        ref_count = other.ref_count;
        mtx = other.mtx;
        ++(*ref_count);
    }

    ~SmartPtr() {
        release();
    }

    SmartPtr& operator=(const SmartPtr& other) {
        if (this != &other) {
            release();
            std::lock_guard<std::mutex> lock(*other.mtx);
            ptr = other.ptr;
            ref_count = other.ref_count;
            mtx = other.mtx;
            ++(*ref_count);
        }
        return *this;
    }

    GstElement* get() const {
        return ptr;
    }

private:
    GstElement* ptr;
    int* ref_count;
    std::mutex* mtx;

    void release() {
        std::lock_guard<std::mutex> lock(*mtx);
        if (--(*ref_count) == 0) {
            gst_object_unref(ptr);
            delete ref_count;
            delete mtx;
        }
    }
};

void safe_print(const std::string& msg) {
    std::lock_guard<std::mutex> lock(print_mutex);
    std::cout << msg << std::endl;
}

void handle_message(SmartPtr& pipeline, GstBus* bus) {
    bool running = true;
    while (running) {
        GstMessage* msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GstMessageType(GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_STATE_CHANGED));
        if (msg != nullptr) {
            GError* err;
            gchar* debug_info;

            switch (GST_MESSAGE_TYPE(msg)) {
                case GST_MESSAGE_ERROR:
                    gst_message_parse_error(msg, &err, &debug_info);
                    safe_print("Error received from element " + std::string(GST_OBJECT_NAME(msg->src)) + ": " + std::string(err->message));
                    g_clear_error(&err);
                    g_free(debug_info);
                    running = false;
                    break;
                case GST_MESSAGE_EOS:
                    safe_print("End of stream");
                    running = false;
                    break;
                case GST_MESSAGE_STATE_CHANGED:
                    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline.get())) {
                        GstState old_state, new_state, pending_state;
                        gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                        if (new_state == GST_STATE_PLAYING) {
                            safe_print("Client connected and streaming started.");
                        } else if (new_state == GST_STATE_NULL) {
                            safe_print("Client disconnected or streaming stopped.");
                        }
                    }
                    break;
                default:
                    break;
            }
            gst_message_unref(msg);
        }
    }
}

void gstreamer_server() {
    gst_init(nullptr, nullptr);
    SmartPtr pipeline(gst_parse_launch("libcamerasrc ! video/x-raw,colorimetry=bt709,format=NV12,width=1280,height=720,framerate=30/1 ! queue ! jpegenc ! multipartmux ! tcpserversink host=0.0.0.0 port=5000", nullptr));

    if (!pipeline.get()) {
        safe_print("Failed to create the GStreamer pipeline.");
        return;
    }

    gst_element_set_state(pipeline.get(), GST_STATE_PLAYING);
    safe_print("Server running and waiting for client connections...");

    GstBus* bus = gst_element_get_bus(pipeline.get());
    handle_message(pipeline, bus);
    gst_object_unref(bus);

    gst_element_set_state(pipeline.get(), GST_STATE_NULL);
}

int main() {
    std::thread server_thread(gstreamer_server);
    server_thread.join();
    return 0;
}

