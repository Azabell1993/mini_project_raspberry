## 라즈베리파이를 사용한 스마트 홈 시스템
- 이 프로젝트는 라즈베리파이를 활용하여 LED, PIR(적외선) 센서, 모터, 카메라 등을 제어하는 스마트 홈 IoT 시스템을 구현한 것입니다. 웹 인터페이스를 통해 사용자가 실시간으로 하드웨어를 제어할 수 있습니다.
  
![image](https://github.com/user-attachments/assets/9d1e0c48-9afd-4109-8871-9d504ff45866)  
  
![image](https://github.com/user-attachments/assets/c87a76ad-70e4-4c91-aea0-07aefe4373e5)  


### 코드 구조
> 폴더명 [mini_project_led_on_off_실시간 웹 처리] 참조
```
pi@azabellraspberry:~/Desktop/pjt2 $ tree
.
├── camera_streamy.py
├── index.html
├── launch.sh
├── smart_home_launcher.sh
├── start.sh
└── web_cap_server.cpp
```

### 실행 순서
1. launch.sh
2. start.sh OR smart_home_launcher.sh

### 주요 기능
- LED 제어: 웹 인터페이스에서 LED를 켜고 끌 수 있습니다.  
- PIR 센서: 움직임을 감지하고 서버로 데이터를 전송합니다.  
- 모터 제어: PWM을 이용하여 모터의 방향과 속도를 제어할 수 있습니다.  
- 실시간 스트리밍: 웹 화면에서 카메라 영상을 실시간으로 확인할 수 있습니다.  
- GPIO 관리: 다양한 장치에 유동적으로 핀 구성을 적용할 수 있습니다.  
- 뮤텍스(Mutex) 사용: 공유 자원을 안전하게 처리하기 위해 뮤텍스를 사용하여 동시성을 관리합니다.  

### 하드웨어 및 핀 연결
- PIR 센서 (BCM_GPIO 40): PIN 29  
- LED (BCM_GPIO 20): PIN 28  
- 모터 PWM 제어 핀: PIN 1  
- 모터 방향 제어 핀 A: PIN 2   
- 모터 방향 제어 핀 B: PIN 4  
- GND: 모든 센서 및 모터는 공통 GND에 연결  

### 사용한 기술
- 프로그래밍 언어: C++, Python

### 라이브러리
- wiringPi: GPIO 핀 제어
- softPwm: 모터 속도 제어 (소프트웨어 PWM)
- Pthread: 스레드 생성 및 관리
- Libcamera: 카메라 제어
- GStreamer: 비디오 스트리밍
- 웹 프레임워크: HTML/CSS, Flask (HTTP 요청 처리)

### 스트리밍 로직
```
$ libcamera-hello --list-cameras
Available cameras
-----------------
0 : ov5647 [2592x1944 10-bit GBRG] (/base/axi/pcie@120000/rp1/i2c@80000/ov5647@36)
    Modes:  'SEBRG10_CSI2P' :   640x480 [58.92 fps - (16, 0)/2560x1920 crop]
                                1296x972 [43.25 fps - (0, 0)/2560x1944 crop]
                                1920x1080 [30.62 fps - (348, 434)/1928x1080 crop]
                                2592x1944 [15.63 fps - (0, 0)/2560x1944 crop]
```

- 서버 스트리밍 시작시
```
sudo gst-launch-1.0 libcamerasrc camera-name="/base/axi/pcie@120000/rp1/i2c@80000/ov5647@36" 
! video/x-raw,colorimetry=bt709,format=NV12,width=1920,height=1080,framerate=30/1 ! queue 
! jpegenc ! queue ! multipartmux ! queue ! tcpserversink host=0.0.0.0 port=5000
```
- 클라이언트에서 받기
```
gst-launch-1.0 tcpclientsrc host=192.168.0.105 port=5000 ! multipartdemux ! jpegdec ! autovideosinkcd
```

> 서버 스트리밍: 라즈베리파이 카메라 모듈에서 비디오를 캡처하고 GStreamer를 이용해 실시간으로 전송.

### 서버 요청 처리
```
/load : LED를 켜고, 카메라 스트리밍 시작
/unload : LED를 끄고 스트리밍 종료
/pinout : GPIO 핀 상태 조회
/modules : 로드된 커널 모듈 정보 확인
/pir : PIR 센서 상태 조회
/left : 모터 좌회전 실행
/right : 모터 우회전 실행
```

### 보완해야 할 점
- 좀비 프로세스 문제: 테스트 완료 후 좀비 프로세스가 발생할 수 있으며, 이를 처리하기 위한 프로세스 관리가 필요합니다.
- 코드 분할: 현재 코드가 단일 파일로 작성되어 있어 유지보수를 위해 코드의 모듈화를 고려해야 합니다.
