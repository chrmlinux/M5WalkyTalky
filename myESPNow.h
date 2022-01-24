//============================================> ESPNow
//
// M5Stick-mic-3.ino
// +-----+
// |     |
// |     | Rec:
// |     | I2S Mic -> soundBuffer -> soundStorage
// |     |
// |     | Play:
// |M5   | soundStorage -> soundBuffer -> I2S Speaker
// |Stick|
// +-----+
//
// M5WalkyTalky.ino
// +-----+                                            +-----+
// |     |                                            |     |
// |     | Rec:                                       |     |
// |     | I2S Mic -> soundBuffer -> soundStorage     |     |
// |     |                                            |     |
// |     |                                      :Play |     |
// |Talk | soundStorage -> soundBuffer -> I2S Speaker |Listn|
// |   er|                                            |   er|
// +-----+                                            +-----+
//

#include <esp_now.h>
#include <WiFi.h>
esp_now_peer_info_t slave;
#define ESPNOW_MAXSEND (250)        // ESPNow送信最大値
#define ESPNOW_SEND_DELAY (3)       // 最低の待ち時間
#define STX (0x02) // 転送開始
#define ETX (0x03) // 転送終了

// MacAdrsを表示する
void dispAdrs(const uint8_t *mac_addr)
{
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  M5.Lcd.println(macStr);
}

void prePlay()
{
  // 初期雑音を消す
  memset(soundStorage, 0, sizeof(soundStorage));
  recPos = 128;
  i2sPlay();
}

// タイトル表示
void titleDisp()
{
  M5.Lcd.print(" ");
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.println(" M5WalkyTalky ");
  M5.Lcd.setTextColor(BLACK, WHITE);
  M5.Lcd.println(" BtnA Rec/Speak" );
  M5.Lcd.println(" BtnB Play" );
}

// データ送信コールバック
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  //  dispAdrs(mac_addr);
  //  M5.Lcd.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

// データ受信コールバック
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *recv, int recvcnt) {
  static int row_ = 0;
  //  dispAdrs(mac_addr);
  Serial.printf("[recv] recvcnt:%d\n", recvcnt);

  if (recvcnt == 1) {
    // 転送された長さが1Byte?
    if (recv[0] == STX) {
      // STX？
      row_ = 0;
      recPos = 0;
      memset(soundStorage, 0x0, sizeof(soundStorage));
    } else {
      // ETX？
      Serial.printf("[recv] row:%d recPos:%d\n", row_, recPos);
      digitalWrite(10, !HIGH);
      M5.Lcd.setCursor(0, 24);
      M5.Lcd.println(" Play...   ");
      M5.Lcd.setCursor(0, 32);
      M5.Lcd.printf(" Recv <= %5d Byte", recPos);
      i2sPlay();
      M5.Lcd.setCursor(0, 24);
      M5.Lcd.println("           ");
      digitalWrite(10, !LOW);
    }
  } else {
    // recvcntが1以外
    memcpy(&soundStorage[recPos], recv, recvcnt);
    recPos += recvcnt;
    row_ ++;
  }
}

void resultCheck(int cnt, esp_err_t result)
{
  switch (result) {
    case ESP_OK:
      break;
    case ESP_ERR_ESPNOW_NOT_INIT:
      Serial.printf("%d:ESPNOW not Init.\n", cnt);
      break;
    case ESP_ERR_ESPNOW_ARG:
      Serial.printf("%d:Invalid Argument.\n", cnt);
      break;
    case ESP_ERR_ESPNOW_INTERNAL:
      Serial.printf("%d:Internal Error.\n", cnt);
      break;
    case ESP_ERR_ESPNOW_NO_MEM:
      Serial.printf("%d:ESP_ERR_ESPNOW_NO_MEM.\n", cnt);
      break;
    case ESP_ERR_ESPNOW_NOT_FOUND:
      Serial.printf("%d:Peer not found.\n", cnt);
      break;
    default:
      Serial.printf("%d:Not sure what happened.\n", cnt);
      break;
  }
}

void sendESPNow()
{
  uint8_t dt[1] = {0};
  size_t sendPos = 0;
  int mod = recPos % ESPNOW_MAXSEND;
  int row = recPos / ESPNOW_MAXSEND;

  esp_err_t result;
  Serial.printf("[send] row:%d mod:%d\n", row, mod);
  M5.Lcd.setCursor(0, 32);
  M5.Lcd.printf(" Send => %5d Byte", recPos);

  dt[0] = STX;
  result = esp_now_send(slave.peer_addr, dt, 1);
  delay(ESPNOW_SEND_DELAY);
  resultCheck(-1, result);

  int i;
  for (i = 0; i < row; i++) {
    result = esp_now_send(slave.peer_addr, &soundStorage[sendPos], ESPNOW_MAXSEND);
    delay(ESPNOW_SEND_DELAY);
    resultCheck(i, result);
    sendPos += ESPNOW_MAXSEND;
  }
  if (mod) {
    result = esp_now_send(slave.peer_addr, &soundStorage[sendPos], mod);
    delay(ESPNOW_SEND_DELAY);
    resultCheck(i, result);
    sendPos += mod;
  }
  dt[0] = ETX;
  result = esp_now_send(slave.peer_addr, dt, 1);
  delay(ESPNOW_SEND_DELAY);
  resultCheck(-1, result);
  Serial.printf("[send] sendPos %d\n", sendPos);
}

void setupESPNow()
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) {
    Serial.print("ESPNow Init Success\n");
  } else {
    Serial.print("ESPNow Init Failed\n");
    ESP.restart();
  }
  pinMode(10, OUTPUT);
  digitalWrite(10, !LOW);
  memset(&slave, 0x0, sizeof(slave));
  for (int i = 0; i < sizeof(slave.peer_addr); ++i) {
    slave.peer_addr[i] = (uint8_t)0xff;
  }
  esp_err_t addStatus = esp_now_add_peer(&slave);
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
}

//<============================================ ESPNow
