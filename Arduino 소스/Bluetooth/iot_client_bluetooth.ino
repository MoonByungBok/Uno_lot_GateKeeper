/*
  blue test: 
  http://www.kccistc.net/
  작성일 : 2022.12.19
  작성자 : IoT 임베디드 KSH
*/
#include <SoftwareSerial.h>
#include <Wire.h>
#include <DHT.h>
#include <MsTimer2.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);

//추가한거
//서보모터
#include <Servo.h>  // 서보 라이브러리
//RC522 모듈
#include <SPI.h>
#include <MFRC522.h>

#define DEBUG

//내가 추가한거
//초음파 거리 센서
#define TRIGGER_PIN 3     // 초음파 센서의 트리거 핀(초음파 보내는 핀)
#define ECHO_PIN 2        // 초음파 센서의 에코 핀(초음파 받는 핀)
#define MAX_DISTANCE 100  // 최대 측정 거리 (센서의 성능에 따라 조정)

//RC522 모듈
#define SS_PIN 10  // spi 통신을 위한 SS(chip select)핀 설정
#define RST_PIN 9  // 리셋 핀 설정

//서보모터
#define SERVO_PIN 6  //서보 PIN
Servo myservo;       //서보 라이브러리

//RGB LED 모듈
#define RED 5
#define GREEN 4


/* 등록된 RF CARD ID */
#define PICC_0 0xE0
#define PICC_1 0xEB
#define PICC_2 0x98
#define PICC_3 0x10

#define DHTTYPE DHT11
#define ARR_CNT 5
#define CMD_SIZE 60
char lcdLine1[17] = "Smart IoT By MBB";
char lcdLine2[17] = "";
char sendBuf[CMD_SIZE];
char recvId[10] = "MBB_UBU";  // SQL 저장 클라이이언트 ID
bool timerIsrFlag = false;
unsigned int secCount;
SoftwareSerial BTSerial(8, 7);  // TX, RX

//추가한거
//초음파 거리센서
float duration, distance;
bool distanceFlag = false;
char distanceStr[10];  // 변환된 거리 값을 저장할 문자열
//RC522 모듈
MFRC522 rfid(SS_PIN, RST_PIN);  // 'rfid' 이름으로 클래스 객체 선언
MFRC522::MIFARE_Key key;
byte nuidPICC[4];  // 카드 ID들을 저장(비교)하기 위한 배열(변수)선언
bool rfidFlag = false;
bool sendFlag = true;


void setup() {
#ifdef DEBUG
  Serial.begin(115200);
  Serial.println("setup() start!");
#endif
  lcd.init();
  lcd.backlight();
  lcdDisplay(0, 0, lcdLine1);
  lcdDisplay(0, 1, lcdLine2);

  //추가한거
  //초음파 거리센서
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  //RC522
  SPI.begin();      // SPI 통신 시작
  rfid.PCD_Init();  // RFID(MFRC522) 초기화
  //pinMode (SERVO_PIN, OUTPUT);
  myservo.attach(SERVO_PIN);  //서보 시작
  myservo.write(0);           //초기 서보 모터를 0도로 위치 시킴

  //RGB LED
  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);
  digitalWrite(RED, LOW);
  digitalWrite(GREEN, LOW);



  BTSerial.begin(9600);           // set the data rate for the SoftwareSerial port
  MsTimer2::set(1000, timerIsr);  // 1000ms period
  MsTimer2::start();
}

void restartRFIDRecognition() {
  //RC522
  //SPI.begin();      // SPI 통신 시작
  //rfid.PCD_Init();  // RFID(MFRC522) 초기화

  if(!rfid.PICC_IsNewCardPresent())
  {
    return;
  }
  if(!rfid.PICC_ReadCardSerial())
  {
    return;
  }
  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  Serial.println(rfid.PICC_GetTypeName(piccType));

  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&  
    piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
    piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("Your tag is not of type MIFARE Classic."));
    return;
  }

  if (rfid.uid.uidByte[0] != PICC_0 || 
  rfid.uid.uidByte[1] != PICC_1 || 
  rfid.uid.uidByte[2] != PICC_2 || 
  rfid.uid.uidByte[3] != PICC_3) {
    //rfidFlag = false;
    Serial.println("false");
    analogWrite(RED, 255);
    analogWrite(GREEN, 0);
    sprintf(sendBuf,"[LJH_UBU]RFID@FAIL\n"); 
    BTSerial.write(sendBuf, strlen(sendBuf));
  } else {
    //rfidFlag = true;
    Serial.println("true");
    analogWrite(GREEN, 255);
    analogWrite(RED, 0);
    sprintf(sendBuf,"[LJH_UBU]RFID@PASS\n");
    BTSerial.write(sendBuf, strlen(sendBuf));
  }
  
  // Halt PICC
  rfid.PICC_HaltA();
  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
  //carPassFail(rfidFlag);
  /*
  rfid.PICC_HaltA();      // 이전 카드 세션 종료
  rfid.PCD_Init();        // 리더 초기화
  
  if (rfid.PICC_IsNewCardPresent()) {
    if (rfid.PICC_ReadCardSerial()) {
      if (rfid.uid.uidByte[0] == PICC_0 && rfid.uid.uidByte[1] == PICC_1 && rfid.uid.uidByte[2] == PICC_2 && rfid.uid.uidByte[3] == PICC_3) {
        rfidFlag = true;
      } else {
        rfidFlag = false;
      }
      carPassFail(rfidFlag);
      rfid.PICC_HaltA();      // 새로운 카드 세션 종료
    }
  }
  */
}


void loop() {
  if (BTSerial.available())
    bluetoothEvent();

  if (timerIsrFlag) {
    timerIsrFlag = false;

    digitalWrite(TRIGGER_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIGGER_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_PIN, LOW);

    duration = pulseIn(ECHO_PIN, HIGH);
    distance = ((float)(340 * duration) / 10000) / 2;

    dtostrf(distance, 4, 1, distanceStr);  // 소수점 2자리까지 출력 (전체 4자리)
    
    sprintf(lcdLine2, "%s",distanceStr); 
    lcdDisplay(0, 1, lcdLine2);
        
#ifdef DEBUG
    //Serial.println(lcdLine2);
#endif
    
  }
  if(distance <= 10)
  {
    restartRFIDRecognition();   
  } 

#ifdef DEBUG
  if (Serial.available())
    BTSerial.write(Serial.read());
#endif
}

void bluetoothEvent() {
  int i = 0;
  char *pToken;
  char *pArray[ARR_CNT] = { 0 };
  char recvBuf[CMD_SIZE] = { 0 };
  int len = BTSerial.readBytesUntil('\n', recvBuf, sizeof(recvBuf) - 1);


#ifdef DEBUG
  Serial.print("Recv : ");
  Serial.println(recvBuf);
#endif

  pToken = strtok(recvBuf, "[@]");
  while (pToken != NULL) {
    pArray[i] = pToken;
    if (++i >= ARR_CNT)
      break;
    pToken = strtok(NULL, "[@]");
  }
  //recvBuf : [XXX_BTM]LED@ON
  //pArray[0] = "XXX_LIN"   : 송신자 ID
  //pArray[1] = "LED"
  //pArray[2] = "ON"
  //pArray[3] = 0x0
  if ((strlen(pArray[1]) + strlen(pArray[2])) < 16) {
    sprintf(lcdLine2, "%s %s", pArray[1], pArray[2]);
    lcdDisplay(0, 1, lcdLine2);
  }

  if (!strcmp(pArray[1], "BUTTON")) {
    if (!strcmp(pArray[2], "ON")) {
      analogWrite(GREEN, 255);
      analogWrite(RED, 0);
      myservo.write(90);
      sprintf(lcdLine2, "Goodbye.");  
      lcdDisplay(0, 1, lcdLine2);
      sendFlag = true;
      char sqlBuf[CMD_SIZE] = { 0 };
      sprintf(sqlBuf, "[%s]%s\n", "MBB_SQL", "SENSOR");
      BTSerial.write(sqlBuf);
    } else if (!strcmp(pArray[2], "OFF")) {
      analogWrite(GREEN, 0);
      analogWrite(RED, 255);
      myservo.write(5);
      sprintf(lcdLine2, "Invalid card.");  
      lcdDisplay(0, 1, lcdLine2);
    }
    sprintf(sendBuf, "[%s]%s@%s\n", pArray[0], pArray[1], pArray[2]);
  }
  else if (!strncmp(pArray[1], " New", 4))  // New Connected
  {
    return;
  } else if (!strncmp(pArray[1], " Alr", 4))  //Already logged
  {
    return;
  }


#ifdef DEBUG
  Serial.print("Send : ");
  Serial.print(sendBuf);
#endif
  BTSerial.write(sendBuf);
}
void timerIsr() {
  timerIsrFlag = true;
  secCount++;
}
void lcdDisplay(int x, int y, char *str) {
  int len = 16 - strlen(str);
  lcd.setCursor(x, y);
  lcd.print(str);
  for (int i = len; i > 0; i--)
    lcd.write(' ');
}


void carPassFail(bool rfid)
{
  if (rfid)
  {
    analogWrite(GREEN, 255);
    analogWrite(RED, 0);
    sprintf(sendBuf,"[LJH_UBU]RFID@PASS\n");
    BTSerial.write(sendBuf, strlen(sendBuf));
  }
  else
  {
    //등록된 카드가 아니라면 시리얼 모니터로 ID 표시
    analogWrite(RED, 255);
    analogWrite(GREEN, 0);
    sprintf(sendBuf,"[LJH_UBU]RFID@FAIL\n");
    BTSerial.write(sendBuf, strlen(sendBuf));
  }
}