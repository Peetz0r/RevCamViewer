#include <Adafruit_GFX.h>    // Core graphics library
#include <ST7735_ESP8266.h> // Hardware-specific library
#include <SPI.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <Bounce2.h>
#include <JPEGDecoder.h>

const char* ssid = "revspace-pub-2.4ghz";
const char* password = "";
const char* mdns_name = "esp_tft";
IPAddress server(10,42,43,152);
const int port = 9999;
const unsigned long restart_timeout = 5*60*1000;

WiFiClient client;

#define TFT_CS     0
#define TFT_RST    0
#define TFT_DC     16
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

#define PIN_LEFT     5
#define PIN_RIGHT   4

#define NUM_CAMS  10

Bounce b_left   = Bounce();
Bounce b_right = Bounce();

uint8_t cam = 4;

void setCam(int relativeCam) {
  cam += relativeCam;
  if(cam < 1) cam = NUM_CAMS;
  if(cam > NUM_CAMS) cam = 1;
  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(36,8);
  tft.setTextColor(ST7735_GREEN);
  tft.setTextSize(12);
  tft.print(cam);
  delay(200);
}

void jpegInfo() {

  // Print information extracted from the JPEG file
  Serial.println("JPEG image info");
  Serial.println("===============");
  Serial.print("Width      :");
  Serial.println(JpegDec.width);
  Serial.print("Height     :");
  Serial.println(JpegDec.height);
  Serial.print("Components :");
  Serial.println(JpegDec.comps);
  Serial.print("MCU / row  :");
  Serial.println(JpegDec.MCUSPerRow);
  Serial.print("MCU / col  :");
  Serial.println(JpegDec.MCUSPerCol);
  Serial.print("Scan type  :");
  Serial.println(JpegDec.scanType);
  Serial.print("MCU width  :");
  Serial.println(JpegDec.MCUWidth);
  Serial.print("MCU height :");
  Serial.println(JpegDec.MCUHeight);
  Serial.println("===============");
  Serial.println("");
}

void showTime(uint32_t msTime) {
  //tft.setCursor(0, 0);
  //tft.setTextFont(1);
  //tft.setTextSize(2);
  //tft.setTextColor(TFT_WHITE, TFT_BLACK);
  //tft.print(F(" JPEG drawn in "));
  //tft.print(msTime);
  //tft.println(F(" ms "));
  Serial.print(F(" JPEG drawn in "));
  Serial.print(msTime);
  Serial.println(F(" ms "));
}

void renderJPEG(int xpos, int ypos) {

  //jpegInfo(); // Print information from the JPEG file (could comment this line out)

  uint16_t *pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;    // Width of MCU
  uint16_t mcu_h = JpegDec.MCUHeight;   // Height of MCU
  uint32_t mcu_pixels = mcu_w * mcu_h;  // Total number of pixels in an MCU

 // Serial.print("comp size = ");Serial.println(comp_size);
  
  uint32_t drawTime = millis(); // For comparison purpose the draw time is measured

  // Fetch data from the file, decode and display
  while (JpegDec.read()) {    // While there is more data in the file
    pImg = JpegDec.pImage ;   // Decode a MCU (Minimum Coding Unit, typically a 8x8 or 16x16 pixel block)

    int mcu_x = JpegDec.MCUx * mcu_w + xpos;  // Calculate coordinates of top left corner of current MCU
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    if ((mcu_x + mcu_w) <= tft.width() && (mcu_y + mcu_h) <= tft.height())
    {
      // Now set a MCU bounding window on the TFT to push pixels into (x, y, x + width - 1, y + height - 1)
      tft.setAddrWindow(mcu_x, mcu_y, mcu_x + mcu_w - 1, mcu_y + mcu_h - 1);

      // Push all MCU pixels to the TFT window
      uint32_t count = mcu_pixels;
      while (count--) {
        // Push each pixel to the TFT MCU area
        tft.pushColor(*pImg++);
      }

      // Push all MCU pixels to the TFT window, ~18% faster to pass an array pointer and length to the library
      // tft.pushColor16(pImg, mcu_pixels); //  To be supported in HX8357 library at a future date

    }
    else if ((mcu_y + mcu_h) >= tft.height()) JpegDec.abort(); // Image has run off bottom of screen so abort decoding
  }

  showTime(millis() - drawTime); // These lines are for sketch testing only
  //Serial.print(" Draw count:");
  //Serial.println(icount++);
}


void setup(void) {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  for(int i = 20; i > 0; i--) {
    Serial.println(i);
    delay(200);
  }
  
  Serial.println("Start");
  tft.initR(INITR_BLACKTAB);   // initialize a ST7735S chip, black tab
  Serial.println("Initialized");
  
  tft.setRotation(3);
  tft.fillScreen(ST7735_BLACK);

  Serial.print("Connecting to: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(50);
  }

  Serial.print("Done! IP: ");
  Serial.println(WiFi.localIP());
  MDNS.begin(mdns_name);

  pinMode(PIN_LEFT, INPUT_PULLUP);
  pinMode(PIN_RIGHT, INPUT_PULLUP);

  b_left.attach(PIN_LEFT);
  b_right.attach(PIN_RIGHT);

  b_left.interval(20);
  b_right.interval(20);
  
}

void loop() {
  String url = "http://10.42.14.8:9042/cam"+String(cam);

  Serial.println();
  Serial.println(url);
  
  HTTPClient http;
  http.begin(url);

  uint8_t http_status = http.GET();

  Serial.print("HTTP status: ");
  Serial.println(http_status);

  if(http_status != 200) {
    return;
  }

  WiFiClient * stream = http.getStreamPtr();
  uint16_t len = http.getSize();
  byte* buf = new byte[len];
  uint16_t i = 0;

  while(i < len) {
    if(stream->available()) {
      buf[i++] = stream->read();
    }
    yield();
  }

  Serial.print("Length: ");
  Serial.println(len);

  JpegDec.decodeArray(buf, len);

  renderJPEG(0, 0);

  Serial.println("done.");
  delete[] buf;

  for(int i = 0; i < 50; i++) {
      b_left.update();
      b_right.update();      
      if(b_left.fell()) {
        setCam(-1);
        break;
      }
      if(b_right.fell()) {
        setCam(1);
        break;
      }
      delay(20);
  }
}

