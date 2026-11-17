# ST77916 원형 LCD 사진 표시

## 샘플 사진 띄우기

1. ZIP을 풀고 `ST77916_Photo/ST77916_Photo.ino`를 Arduino IDE에서 열어.
2. 기존 ESP32-S3 보드와 포트 설정으로 업로드해. 라이브러리는 기존에 쓰던 Arduino_GFX를 사용해.
3. 샘플 사진이 부팅 후 한 번 올라와. 시리얼 모니터를 115200으로 열어 둔 다음 리셋하면 `TE sync`와 `Photo write` 로그를 볼 수 있어. GPIO18의 TE 펄스가 50ms 안에 잡히지 않으면 비동기로 사진을 띄워.

## 내 사진으로 바꾸기

1. `RGB565_Converter.html`을 브라우저에서 열고 JPG, PNG, WebP, BMP 중 하나를 선택해. 변환은 브라우저 안에서 이루어져.
2. 원형 미리보기에서 구도를 보고 `image_rgb565.h 다운로드`를 눌러.
3. `빨강과 파랑 바꾸기`는 해제한 상태로 내보내고, `ST77916_Photo` 폴더 안의 동명 헤더를 교체한 뒤 다시 업로드해. 이전 버전에서 이 옵션을 켜고 만든 헤더도 기본 RGB565로 다시 변환해.

## 연결과 데이터 형식

| 신호 | ESP32-S3 GPIO |
|---|---:|
| CS | 10 |
| SCK | 17 |
| IO0 / IO1 / IO2 / IO3 | 4 / 5 / 6 / 7 |
| RST | 9 |
| 백라이트 | 11 |
| TE | 18 |

변환한 사진은 위에서 아래, 왼쪽에서 오른쪽 순서로 저장한 360×360 RGB565 픽셀이야. 플래시에 들어가는 픽셀 데이터는 한 장당 259,200바이트야. 스케치는 TE를 기다린 뒤 40MHz QSPI로 한 장을 보내고, LCD가 화면 메모리에 받은 사진을 계속 표시해.

샘플은 James S. Davis가 촬영한 미 해군의 Grace Hopper 사진을 크롭한 버전이야. [원본 사진](https://commons.wikimedia.org/wiki/File:Grace_Hopper.jpg)

[PixelForge](https://github.com/R-S-Nithesh/PixelForge)도 임베디드 화면용 RGB565 헤더를 내보낼 수 있어. 외부 변환기를 쓰려면 360×360, RGB565, 바이트 스왑 해제를 선택하고 배열 이름을 `IMAGE_PIXELS`로 맞춰. 헤더에 `IMAGE_WIDTH`와 `IMAGE_HEIGHT`도 각각 360으로 선언해야 해.
