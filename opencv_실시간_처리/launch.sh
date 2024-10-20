g++ server.cpp -o server $(pkg-config --cflags --libs opencv4 gstreamer-1.0 gstreamer-app-1.0)
g++ client.cpp -o client $(pkg-config --cflags --libs opencv4 gstreamer-1.0 gstreamer-app-1.0)