#!/bin/bash

# 포트가 사용 중인지 확인하는 함수
function check_port() {
    port=$1
    if sudo lsof -i :$port > /dev/null; then
        dialog --msgbox "Port $port is already in use. Exiting..." 10 30
        exit 1
    fi
}

# 두 프로세스를 종료하고 정리하는 함수
function cleanup() {
    dialog --infobox "Terminating both processes..." 5 30
    kill $web_cap_server_pid $camera_stream_pid
    wait $web_cap_server_pid $camera_stream_pid 2>/dev/null
    dialog --msgbox "Both processes have been terminated." 10 30
}

# SIGINT와 SIGTERM 신호가 들어오면 cleanup 함수 실행
trap cleanup SIGINT SIGTERM

# web_cap_server 시작 함수
function start_web_cap_server() {
    check_port 8080
    dialog --infobox "Starting web_cap_server on port 8080..." 5 40
    ./web_cap_server &
    web_cap_server_pid=$!
    sleep 2  # 서버가 시작될 시간을 확보

    # web_cap_server가 성공적으로 시작되었는지 확인
    if ! ps -p $web_cap_server_pid > /dev/null; then
        dialog --msgbox "Failed to start web_cap_server." 10 30
        exit 1
    fi
    dialog --msgbox "web_cap_server is running successfully." 10 30
}

# camera_streamy.py 시작 함수
function start_camera_streamy() {
    check_port 5050
    dialog --infobox "Starting camera_streamy.py on port 5050..." 5 40
    python3 camera_streamy.py &
    camera_stream_pid=$!
    sleep 2  # 스트리밍이 시작될 시간을 확보

    # camera_streamy.py가 성공적으로 시작되었는지 확인
    if ! ps -p $camera_stream_pid > /dev/null; then
        dialog --msgbox "Failed to start camera_streamy.py." 10 30
        kill $web_cap_server_pid  # camera_streamy.py가 실패하면 web_cap_server도 종료
        exit 1
    fi
    dialog --msgbox "camera_streamy.py is running successfully." 10 30
}

# 대시보드 화면 출력
function show_dashboard() {
    while true; do
        choice=$(dialog --clear --backtitle "AZABELL System" \
            --title "Process Dashboard" \
            --menu "Choose an option:" \
            15 50 5 \
            1 "Start web_cap_server" \
            2 "Start camera_streamy.py" \
            3 "Stop both processes" \
            4 "Exit" \
            3>&1 1>&2 2>&3)

        case $choice in
            1)
                start_web_cap_server
                ;;
            2)
                start_camera_streamy
                ;;
            3)
                cleanup
                ;;
            4)
                break
                ;;
            *)
                break
                ;;
        esac
    done
    clear
}

# 스크립트 실행
show_dashboard
