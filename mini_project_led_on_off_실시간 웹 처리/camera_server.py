import socket
from flask import Flask, Response
import subprocess
import time

app = Flask(__name__)

# GStreamer 스트리밍 시작 함수
def start_gstreamer():
    # GStreamer 명령어를 Python subprocess로 실행
    gst_command = [
        "gst-launch-1.0", 
        "libcamerasrc", 
        'camera-name="/base/axi/pcie@120000/rp1/i2c@80000/ov5647@36"', 
        "!", "video/x-raw,colorimetry=bt709,format=NV12,width=1920,height=1080,framerate=30/1", 
        "!", "queue", 
        "!", "jpegenc", 
        "!", "queue", 
        "!", "multipartmux", 
        "!", "tcpserversink", "host=0.0.0.0", "port=5000"
    ]

    # GStreamer 명령을 백그라운드에서 실행
    process = subprocess.Popen(gst_command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return process

# 스트림 생성 함수
def generate_stream():
    # TCP 클라이언트로 GStreamer TCP 서버에 연결
    tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    tcp_socket.connect(('127.0.0.1', 5000))  # GStreamer가 실행 중인 TCP 서버에 연결

    buffer = b''
    while True:
        # TCP 소켓에서 데이터를 수신
        data = tcp_socket.recv(4096)
        if not data:
            break
        buffer += data

        # JPEG 프레임의 시작과 끝을 찾음
        start_idx = buffer.find(b'\xff\xd8')  # JPEG 파일 시작 표시
        end_idx = buffer.find(b'\xff\xd9')  # JPEG 파일 끝 표시

        if start_idx != -1 and end_idx != -1:
            frame = buffer[start_idx:end_idx + 2]  # 완전한 JPEG 프레임 추출
            buffer = buffer[end_idx + 2:]  # 버퍼에서 사용한 데이터 제거
            print(f"Sending a frame of size: {len(frame)} bytes")  # 디버그용 출력

            # Flask 스트림으로 프레임 전송
            yield (b'--frame\r\n'
                   b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n\r\n')

@app.route('/video')
def video_feed():
    # GStreamer 스트림 반환
    return Response(generate_stream(), mimetype='multipart/x-mixed-replace; boundary=frame')

if __name__ == '__main__':
    # GStreamer 파이프라인 시작
    gst_process = start_gstreamer()
    time.sleep(2)  # GStreamer가 시작될 때까지 대기

    try:
        # Flask 서버 실행 (포트 5050)
        app.run(host='0.0.0.0', port=5050)
    finally:
        # Flask 서버 종료 시 GStreamer도 종료
        gst_process.terminate()
        gst_process.wait()
