#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"
#include <Wire.h>
#include <hd44780.h>                       // main hd44780 header
#include <hd44780ioClass/hd44780_I2Cexp.h> // i2c expander i/o class header

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22


StaticJsonDocument<768> doc;
hd44780_I2Cexp lcd; // declare lcd object: auto locate & auto config expander chip
WiFiClientSecure client;

const char* ssid = "NSA";
const char* password = "orange";

const char* host = "northcentralus.api.cognitive.microsoft.com"; //edit for your chosen server
const char* Ocp_Apim_Subscription_Key = "see-tutorial-for-key";
const int Port = 443;
const char* boundry = "dgbfhfh";

const int LCD_COLS = 16;
const int LCD_ROWS = 2;

const int trigger_button_pin = 3;
long trigger_button_millis = 0;

void setup()
{
  Serial.begin(115200);
  Wire.begin(2, 15);
  lcd.begin(LCD_COLS, LCD_ROWS);

  Serial.printf("Connecting to the Wifi [%s]...\r\n", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");

  pinMode(trigger_button_pin, INPUT);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound())
  {
    config.frame_size = FRAMESIZE_SXGA; // FRAMESIZE_+QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
    config.jpeg_quality = 10;
    config.fb_count = 2;
  }
  else
  {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Initialise Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_VGA);

  connectToServer();
}

void connectToServer()
{
  Serial.printf("Connecting to %s:%d... ", host, Port);
  if (!client.connect(host, Port))
  {
    Serial.println("Failure in connection with the server");
    return;
  }
}

void sendPhotoToServer()
{

  lcd.clear();
  lcd.print("Analysis");
  lcd.setCursor(0, 1);
  lcd.print("requested.");
  
  String start_request = "";
  String end_request = "";
  
  start_request = start_request + "--" + boundry + "\r\n";
  start_request = start_request + "Content-Disposition: form-data; name=\"file\"; filename=\"CAM.jpg\"\r\n";
  start_request = start_request + "Content-Type: image/jpg\r\n";
  start_request = start_request + "\r\n";

  end_request = end_request + "\r\n";
  end_request = end_request + "--" + boundry + "--" + "\r\n";

  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb)
  {
    Serial.println("Camera capture failed");
    return;
  }

  delay(100);

  int contentLength = (int)fb->len + start_request.length() + end_request.length();

  String headers = "POST https://northcentralus.api.cognitive.microsoft.com/vision/v3.1/describe?maxCandidates=1&language=en HTTP/1.1\r\n"; //edit for your server
  headers = headers + "Host: " + host + "\r\n";
  headers = headers + "User-Agent: ESP32" + "\r\n";
  headers = headers + "Accept: */*\r\n";
  headers = headers + "Content-Type: multipart/form-data; boundary=" + boundry + "\r\n";
  headers = headers + "Ocp-Apim-Subscription-Key: " + Ocp_Apim_Subscription_Key + "\r\n";
  headers = headers + "Content-Length: " + contentLength + "\r\n";
  headers = headers + "\r\n";
  client.print(headers);
  Serial.print(headers);
  client.flush();
  
  lcd.setCursor(10, 1);
  lcd.print(".");

  Serial.print(start_request);
  client.print(start_request);
  client.flush();

  int iteration = fb->len / 1024;
  for (int i = 0; i < iteration; i++)
  {
    client.write(fb->buf, 1024);
    fb->buf += 1024;
    client.flush();
  }
  size_t remain = fb->len % 1024;
  client.write(fb->buf, remain);
  client.flush();
  client.print(end_request);
  
  lcd.setCursor(11, 1);
  lcd.print(".");
  
  // header response
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  // response body
  String description;
  while (client.available()) {
    char c = client.read();
    description = description + c;
    Serial.write(c);
  }

  client.flush();

  esp_camera_fb_return(fb);

  deserializeJson(doc, description);
  const char* description_captions_0_text = doc["description"]["captions"][0]["text"];
  char descriptionWithFullStop[100];   // array to hold the result.
  strcpy(descriptionWithFullStop, description_captions_0_text); // copy string one into the result.
  strcat(descriptionWithFullStop, ".");
  float description_captions_0_confidence = doc["description"]["captions"][0]["confidence"];
  Serial.println(descriptionWithFullStop);

  // TODO - Use confidence to add 'Might be...', 'Looks like...', 'I'm sure...' to phrase

  lcd.clear();
  lcd.lineWrap();
  lcd.print(description_captions_0_text);

  // Or wrap rather than scroll
  //  int scrollStepsNeeded =  strlen(description_captions_0_text) - LCD_COLS;
  //  for (int i = 1; i <= scrollStepsNeeded; i++) {
  //    lcd.scrollDisplayLeft();
  //    delay(200);
  //  }

}

void loop()
{

  // add in a delay to avoid repeat presses
  if (digitalRead(trigger_button_pin) == HIGH && millis() - trigger_button_millis > 1000) {
    trigger_button_millis = millis();
    Serial.println("button pressed");
    sendPhotoToServer();
  }
}
