/*
  WiFiEsp test: ClientTest
  http://www.kccistc.net/
  작성일 : 2022.12.19
  작성자 : IoT 임베디드 KSH
*/
#define DEBUG
//#define DEBUG_WIFI

#include <WiFiEsp.h>
#include <SoftwareSerial.h>
#include <MsTimer2.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define AP_SSID "embB"
#define AP_PASS "embB1234"
// #define SERVER_NAME "10.10.14.74"
#define SERVER_NAME "10.10.14.62"
#define SERVER_PORT 5000
#define LOGID "LJH_UBU"
#define PASSWD "PASSWD"

#define WIFIRX 6  //6:RX-->ESP8266 TX
#define WIFITX 7  //7:TX -->ESP8266 RX

#define CMD_SIZE 50   // 명령어 길이           <---------- 50 ------------>
#define ARR_CNT 5     // 최대 개수 명령어 개수 [LJH_UBU]LED@ON@dsadkj@dasjkd
                      //                          1     2  3   4      5
#define pinRGB_R 4
#define pinRGB_G 5
#define pinBUZZER 8

bool timerIsrFlag = false;
boolean lastButton = LOW;     // 버튼의 이전 상태 저장
boolean currentButton = LOW;    // 버튼의 현재 상태 저장
boolean ledOn = false;      // LED의 현재 상태 (on/off)

char sendBuf[CMD_SIZE];
char lcdLine1[17] = "Smart IoT By KSH";   // 18byte를 넘으면 버퍼오버플로우 남. 주의할것
char lcdLine2[17] = "WiFi Connecting!";

unsigned int secCount;
unsigned int myservoTime = 0;

char getSensorId[10];
bool updatTimeFlag = false;
typedef struct {
  int year;
  int month;
  int day;
  int hour;
  int min;
  int sec;
} DATETIME;
DATETIME dateTime = {0, 0, 0, 12, 0, 0};
SoftwareSerial wifiSerial(WIFIRX, WIFITX);
WiFiEspClient client;
LiquidCrystal_I2C lcd(0x27, 16, 2);

int led_color_value = 255;  // led 색상 값
const int buttonPinA = 2;   // 버튼 핀 번호

void setup() {
  lcd.init();
  lcd.backlight();
  lcdDisplay(0, 0, lcdLine1);
  lcdDisplay(0, 1, lcdLine2);

#ifdef DEBUG
  Serial.begin(115200); //DEBUG
#endif
  wifi_Setup();

  MsTimer2::set(1000, timerIsr); // 1000ms period
  MsTimer2::start();
  
  pinMode(pinRGB_R, OUTPUT);   // RGB Red 핀 출력으로 설정
  pinMode(pinRGB_G, OUTPUT);   // RGB Green 핀 출력으로 설정
  pinMode(pinBUZZER, OUTPUT);  // 부저핀 출력으로 설정
  pinMode(buttonPinA, INPUT_PULLUP);  // 버튼 핀

}




void close() {
  
  for (int i = 0; i < 3; i++) {
    if (i == 0) {
      tone(pinBUZZER, 523);
      // noTone(pinBUZZER);
      delay(300);
    } else if (i == 1) {
      tone(pinBUZZER, 349);
      delay(300);
      // noTone(pinBUZZER);
    } else if (i == 2) {
      tone(pinBUZZER, 262);
      delay(300);
      noTone(pinBUZZER);
    }
  }
  
  analogWrite(pinRGB_G, 0);
  analogWrite(pinRGB_R, led_color_value);
}

void accept() {
  
  for (int i = 0; i < 3; i++) {
    if (i == 0) {
      tone(pinBUZZER, 262);
      // noTone(pinBUZZER);
      delay(300);
    } else if (i == 1) {
      tone(pinBUZZER, 349);
      delay(300);
      // noTone(pinBUZZER);
    } else if (i == 2) {
      tone(pinBUZZER, 523);
      delay(300);
      noTone(pinBUZZER);
    }
  }
  
  analogWrite(pinRGB_R, 0);
  analogWrite(pinRGB_G, led_color_value);
}

void carPassFail(boolean currentButton)
{
  if (currentButton)
  {
    currentButton = false;
    sprintf(sendBuf,"[MBB_UBU]BUTTON@ON\n");
    client.write(sendBuf, strlen(sendBuf));
    
  }
  else
  {
    sprintf(sendBuf,"[MBB_UBU]BUTTON@OFF\n");
    client.write(sendBuf, strlen(sendBuf));
  }
}

boolean debounce(boolean last)
{
  boolean current = digitalRead(buttonPinA);  // 버튼 상태 읽기
  if (last != current)      // 이전 상태와 현재 상태가 다르면...
  {
    delay(5);         // 5ms 대기
    current = digitalRead(buttonPinA);  // 버튼 상태 다시 읽기
  }
  return current;       // 버튼의 현재 상태 반환
}


void loop() {
  if (client.available()) {
    socketEvent();
  }

  if (timerIsrFlag) //1초에 한번씩 실행
  {
    timerIsrFlag = false;
    
    sprintf(lcdLine1, "%02d.%02d  %02d:%02d:%02d", dateTime.month, dateTime.day, dateTime.hour, dateTime.min, dateTime.sec );   // 16문자 넘기지 말라
    lcdDisplay(0, 0, lcdLine1);
    if (updatTimeFlag)
    {
      client.print("[GETTIME]\n");
      updatTimeFlag = false;
    }
  } // 101부터 162까지의 코드 내에서 쓸데 없는 코드는 줄여라 안 그러면 딜레이가 되어 반응이 느려진다

  currentButton = debounce(lastButton);   // 디바운싱된 버튼 상태 읽기
  if (lastButton == LOW && currentButton == HIGH)  // 버튼을 누르면...
  {
    sprintf(sendBuf,"[MBB_UBU]BUTTON@ON\n");
    client.write(sendBuf, strlen(sendBuf));
    client.flush();
  }
  if (lastButton == HIGH && currentButton == LOW)
  {
    sprintf(sendBuf,"[MBB_UBU]BUTTON@OFF\n");
    client.write(sendBuf, strlen(sendBuf));
    client.flush();
  }
  lastButton = currentButton;     // 이전 버튼 상태를 현재 버튼 상태로 설정
  
}


void socketEvent()
{
  int i = 0;
  char * pToken;
  char * pArray[ARR_CNT] = {0};
  char recvBuf[CMD_SIZE] = {0};
  int len;

  sendBuf[0] = '\0';
  len = client.readBytesUntil('\n', recvBuf, CMD_SIZE); // \n이 들어오기 전까지 recvBUF에 내용을 넣는다.
  client.flush();
#ifdef DEBUG
  Serial.print("recv : ");
  Serial.print(recvBuf);
#endif
  pToken = strtok(recvBuf, "[@]");
  while (pToken != NULL)
  {
    pArray[i] =  pToken;
    if (++i >= ARR_CNT)
      break;
    pToken = strtok(NULL, "[@]");
  }
  //[KSH_ARD]LED@ON : pArray[0] = "KSH_ARD", pArray[1] = "LED", pArray[2] = "ON"
  if ((strlen(pArray[1]) + strlen(pArray[2])) < 16)
  {
    sprintf(lcdLine2, "%s %s", pArray[1], pArray[2]);
    lcdDisplay(0, 1, lcdLine2); // 첫글자, 1번째 줄, 내용
  }
  if (!strncmp(pArray[1], " New", 4)) // New Connected
  {
#ifdef DEBUG
    Serial.write('\n');
#endif
    strcpy(lcdLine2, "Server Connected");
    lcdDisplay(0, 1, lcdLine2);
    updatTimeFlag = true;
    return ;
  }
  else if (!strncmp(pArray[1], " Alr", 4)) //Already logged
  {
#ifdef DEBUG
    Serial.write('\n');
#endif
    client.stop();
    server_Connect();
    return;
  }
  else if(!strcmp(pArray[1],"RFID"))
  {
    if (!strcmp(pArray[2], "PASS"))   // 통과
    {
      strcpy(lcdLine2, "Card check");
      lcdDisplay(0, 1, lcdLine2);
      accept();
      //sprintf(sendBuf, "[%s]%s@%s\n", pArray[0], pArray[1], pArray[2]);
      //Serial.println(sendBuf);
    }
    else if(!strcmp(pArray[2], "FAIL")) // 통과 실패
    {
      strcpy(lcdLine2, "Card not check");
      lcdDisplay(0, 1, lcdLine2);
      close();
      //sprintf(sendBuf, "[%s]%s@%s\n", pArray[0], pArray[1], pArray[2]);
      //Serial.println(sendBuf);
    }
    return;
  }
  else if(!strcmp(pArray[0],"GETTIME")) {  //GETTIME
    dateTime.year = (pArray[1][0]-0x30) * 10 + pArray[1][1]-0x30 ;
    dateTime.month =  (pArray[1][3]-0x30) * 10 + pArray[1][4]-0x30 ;
    dateTime.day =  (pArray[1][6]-0x30) * 10 + pArray[1][7]-0x30 ;
    dateTime.hour = (pArray[1][9]-0x30) * 10 + pArray[1][10]-0x30 ;
    dateTime.min =  (pArray[1][12]-0x30) * 10 + pArray[1][13]-0x30 ;
    dateTime.sec =  (pArray[1][15]-0x30) * 10 + pArray[1][16]-0x30 ;

#ifdef DEBUG
    sprintf(sendBuf,"\nTime %02d.%02d.%02d %02d:%02d:%02d\n\r",dateTime.year,dateTime.month,dateTime.day,dateTime.hour,dateTime.min,dateTime.sec );
    Serial.println(sendBuf);
#endif
    return;
  } 
  else
    return;
  Serial.println("sds");
  client.write(sendBuf, strlen(sendBuf));   // 지금까지 저장된 명령은 sendBuf에 보낸다.
  // 절대 연속으로 보내지 말고 @를 이용해서 여러 문자를 섞어 명령어를 보내라
  // 윗 줄에 sprintf()한 것을 상대
  // 기종(서버, 라즈베리 파이, 아두이노 등) 생각하지말고 상대방한테 보낸다고 생각
  // client.write(sendBuf, strlen(sendBuf));  // 시리얼 와이파이 특성 상 너무 느려서 최소 0.5초 정도 지난 후에 보내야 함
  client.flush();

#ifdef DEBUG
  Serial.print(", send : ");
  Serial.print(sendBuf);
#endif
}
void timerIsr()
{
  timerIsrFlag = true;
  secCount++;
  clock_calc(&dateTime);
}
void clock_calc(DATETIME *dateTime)
{
  int ret = 0;
  dateTime->sec++;          // increment second

  if(dateTime->sec >= 60)                              // if second = 60, second = 0
  { 
      dateTime->sec = 0;
      dateTime->min++; 
             
      if(dateTime->min >= 60)                          // if minute = 60, minute = 0
      { 
          dateTime->min = 0;
          dateTime->hour++;                               // increment hour
          if(dateTime->hour == 24) 
          {
            dateTime->hour = 0;
            updatTimeFlag = true;
          }
       }
    }
}

void wifi_Setup() {
  wifiSerial.begin(19200);
  wifi_Init();
  server_Connect();
}
void wifi_Init()
{
  do {
    WiFi.init(&wifiSerial);
    if (WiFi.status() == WL_NO_SHIELD) {
#ifdef DEBUG_WIFI
      Serial.println("WiFi shield not present");
#endif
    }
    else
      break;
  } while (1);

#ifdef DEBUG_WIFI
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(AP_SSID);
#endif
  while (WiFi.begin(AP_SSID, AP_PASS) != WL_CONNECTED) {
#ifdef DEBUG_WIFI
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(AP_SSID);
#endif
  }
  sprintf(lcdLine1, "ID:%s", LOGID);
  lcdDisplay(0, 0, lcdLine1);
  sprintf(lcdLine2, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  lcdDisplay(0, 1, lcdLine2);

#ifdef DEBUG_WIFI
  Serial.println("You're connected to the network");
  printWifiStatus();
#endif
}
int server_Connect()
{
#ifdef DEBUG_WIFI
  Serial.println("Starting connection to server...");
#endif

  if (client.connect(SERVER_NAME, SERVER_PORT)) {
#ifdef DEBUG_WIFI
    Serial.println("Connect to server");
#endif
    client.print("["LOGID":"PASSWD"]");
  }
  else
  {
#ifdef DEBUG_WIFI
    Serial.println("server connection failure");
#endif
  }
}
void printWifiStatus()
{
  // print the SSID of the network you're attached to

  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
void lcdDisplay(int x, int y, char * str)
{
  int len = 16 - strlen(str);
  lcd.setCursor(x, y);
  lcd.print(str);
  for (int i = len; i > 0; i--)
    lcd.write(' ');
}