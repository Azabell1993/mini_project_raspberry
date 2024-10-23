#!/bin/bash

# 포트가 사용 중인지 확인하는 함수
check_port() {
    port=$1
    if sudo lsof -i :$port > /dev/null; then
        echo "포트 $port가 이미 사용 중입니다. 종료합니다."
        exit 1
    fi
}

# 두 프로세스를 종료하고 정리하는 함수
cleanup() {
    echo "두 프로세스를 종료합니다..."
    kill $web_cap_server_pid $camera_stream_pid
    wait $web_cap_server_pid $camera_stream_pid 2>/dev/null
    echo "두 프로세스가 모두 종료되었습니다."
}

# SIGINT와 SIGTERM 신호가 들어오면 cleanup 함수 실행
trap cleanup SIGINT SIGTERM

# 포트 8080, 5050, 5000이 사용 중인지 확인
check_port 8080
check_port 5050
check_port 5000

# web_cap_server 시작 (web_cap_server.cpp에서 컴파일된 프로그램 실행)
echo "포트 8080에서 web_cap_server를 시작합니다..."
./web_cap_server &
web_cap_server_pid=$!
sleep 2  # 서버가 시작될 시간을 확보

# web_cap_server가 성공적으로 시작되었는지 확인
if ! ps -p $web_cap_server_pid > /dev/null; then
    echo "web_cap_server 시작에 실패했습니다."
    exit 1
fi

# camera_streamy.py 시작 (포트 5050에서 GStreamer를 사용)
echo "포트 5050에서 camera_streamy.py를 시작합니다..."
python3 camera_streamy.py &
camera_stream_pid=$!
sleep 2  # 스트리밍이 시작될 시간을 확보

# camera_streamy.py가 성공적으로 시작되었는지 확인
if ! ps -p $camera_stream_pid > /dev/null; then
    echo "camera_streamy.py 시작에 실패했습니다."
    kill $web_cap_server_pid  # camera_streamy.py가 실패하면 web_cap_server도 종료
    exit 1
fi

echo "두 프로세스가 모두 실행 중입니다."
wait $web_cap_server_pid $camera_stream_pid
