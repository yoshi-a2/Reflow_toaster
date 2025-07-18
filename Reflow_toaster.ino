#define SSR 28

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>
#include <JPEGDecoder.h>
#include <SPI.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/Tiny3x3a2pt7b.h>

#define TFT_WIDTH 320    //x方向ビット数
#define TFT_HEIGHT 240   //y方向ビット数
int page = 1;  //表示ページ
unsigned long time_start = 0;   //焼き開始時刻
unsigned long time_now = 0;
unsigned long time_now_loop = 0;
unsigned long time_start_to_now = 0;
unsigned long time_start_to_now_loop = 0;
unsigned long time_remaining = 0;
double data; //グラフプロット時に使用
int plot_X;
int plot_Y;
bool check_reflow_start = false;  //リフローが始まっているか
bool find_SD = false;   //SDカードが認識されているか
bool open_SD = false;  //SDカードの書き込みが終わっているか確認
int no1 = 0; //Emargencyの計算用
int no2 = 0;
int no3= 0;
int no_calc = 0;

int triangle_x;

int l = 0;

//int data_time_start_to_now_loop;
double data_time_loop;


double plot[250];
double data_time[250];
int num_loop = 0; //core0のloopが回った回数を数える


#define sound 0 //ボタンタッチ音

#define TFT_CS 22   // CS
#define TFT_RST 21   // Reset 
#define TFT_DC 20   // D/ C
#define TOUCH_SD_MISO 16
#define TOUCH_CS 17
#define TFT_TOUCH_SD_MOSI 19   // SDI(MOSI)
#define TFT_TOUCH_SD_SCK 18   // SCK
#define SD_CS 15
#define SD_FILENAME "/reflow_tempdata.csv"
#define JPEG_FILENAME "/TORICA_LOGO.jpg"

//グラフィックのインスタンス
Adafruit_ILI9341 tft = Adafruit_ILI9341(&SPI, TFT_DC, TFT_CS, TFT_RST);

//タッチパネルのインスタンス
XPT2046_Touchscreen ts(TOUCH_CS);

//SDのインスタンス
JPEGDecoder jpegDec;

// Fileクラスのインスタンスを宣言
File myFile;

//スプライト（16bit）
GFXcanvas16 canvas(TFT_WIDTH, TFT_HEIGHT);    // 通常タッチ画面用
GFXcanvas16 canvas1(TFT_WIDTH, TFT_HEIGHT);   //通常画像表示用


//変数宣言
bool SSR_ON = false;   // ランプ点灯状態格納用
bool change_page = false;   //ページを変えるスイッチ用


/******************** テキスト描画関数 ********************/
void drawText(int16_t x, int16_t y, const char* text, const GFXfont* font, uint16_t color) {
  canvas.setFont(font);       // フォント
  canvas.setTextColor(color); // 文字色
  canvas.setCursor(x, y);     // 表示座標
  canvas.println(text);       // 表示内容
}

/***************** manual用　ランプ点灯状態更新 & SSR制御関数 *****************/
void updateLamp() {
  if (SSR_ON == true) { // ランプ状態点灯なら
    canvas.fillCircle(70, 22, 17, ILI9341_RED);         // Heatimg点灯
    canvas.fillCircle(210, 22, 17, ILI9341_DARKGREY); // Wauting消灯
    digitalWrite(SSR, HIGH);

  }
  else { 
    canvas.fillCircle(70, 22, 17, ILI9341_DARKGREY); // Heating消灯
    canvas.fillCircle(210, 22, 17, ILI9341_BLUE);    // Waiting点灯
    digitalWrite(SSR, LOW);

  }
}

/***************** auto用　ランプ点灯状態更新関数 ***********************/
void auto_updateLamp(){
  if(SSR_ON == true){
    canvas.fillCircle(70, 22, 17, ILI9341_RED);         // Heatimg点灯
    canvas.fillCircle(210, 22, 17, ILI9341_DARKGREY); // Waiting消灯
  }
  if(SSR_ON == false){
    canvas.fillCircle(70, 22, 17, ILI9341_DARKGREY); // Heating消灯
    canvas.fillCircle(210, 22, 17, ILI9341_BLUE);    // Waiting点灯
  }
}


/******************** ボタン描画関数 ********************/
void drawButton(int x, int y, int w, int h, const char* label, const GFXfont* font, uint16_t bgColor, uint16_t labelColor) {
  canvas.fillRect(x, y, w, h, ILI9341_DARKGREY);      // 外枠
  canvas.fillRect(x + 3, y + 3, w-6, h-6, ILI9341_WHITE); // 境界線
  canvas.fillRect(x + 6, y + 6, w-12, h-12, bgColor);       // 操作部
  canvas.setFont(font); // 表示ラベル

  // テキストの幅と高さを取得
  int16_t textX, textY; // テキスト位置取得用
  uint16_t textWidth, textHeight; // テキストサイズ取得用
  canvas.getTextBounds(label, x, y, &textX, &textY, &textWidth, &textHeight);
  
  // 中央揃えのための新しいx, y座標の計算
  int16_t centeredX = x + (w - textWidth) / 2;               // xを中央へ
  int16_t centeredY = y + (h - textHeight) / 2 + textHeight; // yを下げて中央へ
  
  canvas.setTextColor(labelColor);        // 文字色
  canvas.setCursor(centeredX, centeredY); // 新しいカーソル位置を設定
  canvas.print(label);                    // テキストを描画
}

/******************** 進行状況の三角描画関数 ********************/
void triangle(int triangle_x){
  canvas.drawFastHLine(triangle_x, 156, 9, ILI9341_WHITE);
  canvas.drawFastHLine(triangle_x + 1, 157, 7, ILI9341_WHITE);
  canvas.drawFastHLine(triangle_x + 2, 158, 5, ILI9341_WHITE);
  canvas.drawFastHLine(triangle_x + 3, 159, 3, ILI9341_WHITE);
  canvas.drawFastHLine(triangle_x + 4, 160, 1, ILI9341_WHITE);
}

/******************** 1ページ目 ********************/
void page_1(){
  jpegDraw(JPEG_FILENAME);
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);

  delay(3000);
  page = 2;
}


/******************** 2ページ目 manual or auto ********************/
int page_2(double smoothed_celsius){
  SSR_ON = false;
  updateLamp();
  canvas.fillScreen(ILI9341_BLACK);   //背景色リセット
  drawText(92, 30, "Heating", &FreeSans9pt7b, ILI9341_WHITE);
  drawText(232, 30, "Waiting", &FreeSans9pt7b, ILI9341_WHITE);
  drawText(60, 210, "Temp:", &FreeSans12pt7b, ILI9341_WHITE);
  drawText(200, 210, "[degC]", &FreeSans12pt7b, ILI9341_WHITE);

  canvas.setTextColor(ILI9341_WHITE); 
  canvas.setFont(&FreeSans12pt7b);  // フォント指定
  canvas.setCursor(140, 210);          // 表示座標指定
  canvas.print(smoothed_celsius);

  // 平行線(x始点，y始点，長さ)
  canvas.drawFastHLine(0, 46, 320, ILI9341_WHITE);

  // Heating_Waitingランプ描画(x, y, 半径, 色)
  canvas.fillCircle(70, 22, 20, ILI9341_DARKGREY); // 外枠
  canvas.fillCircle(70, 22, 18, ILI9341_WHITE);    // 境界線
  canvas.fillCircle(210, 22, 20, ILI9341_DARKGREY); // 外枠
  canvas.fillCircle(210, 22, 18, ILI9341_WHITE);    // 境界線
  canvas.fillCircle(70, 22, 17, ILI9341_DARKGREY); // Heatingランプ部消灯
  canvas.fillCircle(210, 22, 17, ILI9341_BLUE); // Waitingランプ部点灯

  // ボタン描画（左上x, 左上y, wide, high, ラベル, フォント, ボタン色, ラベル色）
  drawButton(25, 80, 140, 100, "Manual", &FreeSansBold18pt7b, ILI9341_ORANGE, ILI9341_WHITE); // ONボタン
  drawButton(170, 80, 140, 100, "Auto", &FreeSansBold18pt7b, ILI9341_GREEN, ILI9341_WHITE); // OFFボタン

  if (myFile) { // ファイルが開けたら
    myFile.close(); // ファイルを閉じる
    drawText(50, 80, "SD card is recognized!!", &FreeSans12pt7b, ILI9341_WHITE);
    find_SD = true;
  } else { // ファイルが開けなければ
    //Serial.println("SDカードが見つかりません");
    drawText(500, 80, "SD card cannot be recognized...", &FreeSans12pt7b, ILI9341_WHITE);
    find_SD = false;
  }

  //スプライトをディスプレイ表示
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);


  if (ts.touched() == true) {  // タッチされていれば
    TS_Point tPoint = ts.getPoint();  // タッチ座標を取得

    // タッチx座標をTFT画面の座標に換算
    int16_t x = (tPoint.x-400) * TFT_WIDTH / (4095-550);  // タッチx座標をTFT画面の座標に換算
    int16_t y = (tPoint.y-230) * TFT_HEIGHT / (4095-420); // タッチy座標をTFT画面の座標に換算

    // ボタンタッチエリア検出
    if (x >= 25 && x <= 165 && y >= 80 && y <= 180){
      tone(sound,3000,100);
      page = 3;
    }    // 範囲内ならmanualに
    if (x >= 170 && x <= 310 && y >= 80 && y <= 180){
      tone(sound,3000,100);
      page = 4;
    } // 範囲内ならautoに

  }

  return page;
}

/******************** 3ページ目 manual操作画面 ********************/
int page_3(double smoothed_celsius){
  canvas.fillScreen(ILI9341_BLACK);   //背景色リセット
  drawText(92, 30, "Heating", &FreeSans9pt7b, ILI9341_WHITE);
  drawText(232, 30, "Waiting", &FreeSans9pt7b, ILI9341_WHITE);
  drawText(8, 85, "Temp:", &FreeSans18pt7b, ILI9341_WHITE);
  drawText(245, 88, "[degC]", &FreeSans12pt7b, ILI9341_WHITE);

  canvas.setTextColor(ILI9341_WHITE); 
  canvas.setFont(&FreeSans18pt7b);  // フォント指定
  canvas.setCursor(135, 85);          // 表示座標指定
  canvas.print(smoothed_celsius);

  // 平行線(x始点，y始点，長さ)
  canvas.drawFastHLine(0, 46, 320, ILI9341_WHITE);
  canvas.drawFastHLine(0, 100, 320, ILI9341_WHITE);

  // Heatingランプ描画(x, y, 半径, 色)
  canvas.fillCircle(70, 22, 20, ILI9341_DARKGREY); // 外枠
  canvas.fillCircle(70, 22, 18, ILI9341_WHITE);    // 境界線

  // Waitingランプ描画(x, y, 半径, 色)
  canvas.fillCircle(210, 22, 20, ILI9341_DARKGREY); // 外枠
  canvas.fillCircle(210, 22, 18, ILI9341_WHITE);    // 境界線
  updateLamp(); // ボタン点灯状態更新関数呼び出し


  // ボタン描画（左上x, 左上y, wide, high, ラベル, フォント, ボタン色, ラベル色）
  drawButton(25, 102, 140, 85, "ON", &FreeSansBold18pt7b, ILI9341_RED, ILI9341_WHITE); // OFFボタン
  drawButton(170, 102, 140, 85, "OFF", &FreeSansBold18pt7b, ILI9341_BLUE, ILI9341_WHITE); // ONボタン
  drawButton(60, 188, 200, 45, "Main Menu", &FreeSans18pt7b, ILI9341_ORANGE, ILI9341_WHITE); // cancelボタン

  //スプライトをディスプレイ表示
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);


  if (ts.touched() == true) {  // タッチされていれば
    TS_Point tPoint = ts.getPoint();  // タッチ座標を取得

    // タッチx座標をTFT画面の座標に換算
    int16_t x = (tPoint.x-400) * TFT_WIDTH / (4095-550);  // タッチx座標をTFT画面の座標に換算
    int16_t y = (tPoint.y-230) * TFT_HEIGHT / (4095-420); // タッチy座標をTFT画面の座標に換算

    // ボタンタッチエリア検出
    if (x >= 25 && x <= 165 && y >= 102 && y <= 187){
      SSR_ON = true;   // 範囲内ならランプ状態を点灯へ
      tone(sound,3000,100);
    }
    if (x >= 170 && x <= 310 && y >= 102 && y <= 187) {
      SSR_ON = false; // 範囲内ならランプ状態を消灯へ
      tone(sound,3000,100);
    }
    if (x >= 60 && x <= 260 && y >= 188 && y <= 233){
      page = 2; // 範囲内ならpage2へ
      tone(sound,3000,100);
    }

  }
  Serial.println(SSR_ON);
  return page;
}

/******************** 4ページ目 autoメニュー表示 ********************/
int page_4(){
  SSR_ON = false;
  canvas.fillScreen(ILI9341_BLACK);   //背景色リセット
  drawText(92, 30, "Heating", &FreeSans9pt7b, ILI9341_WHITE);
  drawText(232, 30, "Waiting", &FreeSans9pt7b, ILI9341_WHITE);

  drawText(20, 80, "Heat up to 100 degC", &FreeSans9pt7b, ILI9341_WHITE);
  drawText(230, 80, "30 sec", &FreeSans9pt7b, ILI9341_WHITE);
  drawText(20, 115, "Heat up to 150 degC", &FreeSans9pt7b, ILI9341_WHITE);
  drawText(230, 115, "90 sec", &FreeSans9pt7b, ILI9341_WHITE);
  drawText(20, 150, "Heat up to 235 degC", &FreeSans9pt7b, ILI9341_WHITE);
  drawText(230, 150, "90 sec", &FreeSans9pt7b, ILI9341_WHITE);


  // 平行線(x始点，y始点，長さ)
  canvas.drawFastHLine(0, 46, 320, ILI9341_WHITE);

  // Heating_Waitingランプ描画(x, y, 半径, 色)
  canvas.fillCircle(70, 22, 20, ILI9341_DARKGREY); // 外枠
  canvas.fillCircle(70, 22, 18, ILI9341_WHITE);    // 境界線
  canvas.fillCircle(210, 22, 20, ILI9341_DARKGREY); // 外枠
  canvas.fillCircle(210, 22, 18, ILI9341_WHITE);    // 境界線
  canvas.fillCircle(70, 22, 17, ILI9341_DARKGREY); // Heatingランプ部消灯
  canvas.fillCircle(210, 22, 17, ILI9341_BLUE); // Waitingランプ部点灯

  // ボタン描画（左上x, 左上y, wide, high, ラベル, フォント, ボタン色, ラベル色）
  drawButton(25, 170, 140, 55, "OK", &FreeSansBold18pt7b, ILI9341_CYAN, ILI9341_WHITE); // Cancelボタン
  drawButton(170, 170, 140, 55, "Cancel", &FreeSansBold18pt7b, ILI9341_CYAN, ILI9341_WHITE); // OKボタン

  //スプライトをディスプレイ表示
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);


  if (ts.touched() == true) {  // タッチされていれば
    TS_Point tPoint = ts.getPoint();  // タッチ座標を取得

    // タッチx座標をTFT画面の座標に換算
    int16_t x = (tPoint.x-400) * TFT_WIDTH / (4095-550);  // タッチx座標をTFT画面の座標に換算
    int16_t y = (tPoint.y-230) * TFT_HEIGHT / (4095-420); // タッチy座標をTFT画面の座標に換算

    // ボタンタッチエリア検出
    if (x >= 25 && x <= 165 && y >= 170 && y <= 225){
      page = 6;   // 範囲内ならpage6
      tone(sound,3000,100);
    }
    if (x >= 170 && x <= 310 && y >= 170 && y <= 225){
      page = 2; // 範囲内ならpage2
      tone(sound,3000,100);
    }
 

  }

  return page;
}

/******************** 5ページ目 ********************/
void page_5(){
  SSR_ON = false;
  canvas.fillScreen(ILI9341_BLACK);   //背景色リセット
 
    
}

/******************** 6ページ目 autoリフロースタート最終確認 ********************/
int page_6(double smoothed_celsius){
  SSR_ON = false;
  canvas.fillScreen(ILI9341_BLACK);   //背景色リセット
  drawText(92, 30, "Heating", &FreeSans9pt7b, ILI9341_WHITE);
  drawText(232, 30, "Waiting", &FreeSans9pt7b, ILI9341_WHITE);
  drawText(60, 230, "Temp:", &FreeSans12pt7b, ILI9341_WHITE);
  drawText(200, 230, "[degC]", &FreeSans12pt7b, ILI9341_WHITE);
  drawText(20, 100, "Start Auto-Reflow?", &FreeSans18pt7b, ILI9341_WHITE);

  canvas.setTextColor(ILI9341_WHITE); 
  canvas.setFont(&FreeSans12pt7b);  // フォント指定
  canvas.setCursor(140, 230);          // 表示座標指定
  canvas.print(smoothed_celsius);

  
  // 平行線(x始点，y始点，長さ)
  canvas.drawFastHLine(0, 46, 320, ILI9341_WHITE);

  // Heating_Waitingランプ描画(x, y, 半径, 色)
  canvas.fillCircle(70, 22, 20, ILI9341_DARKGREY); // 外枠
  canvas.fillCircle(70, 22, 18, ILI9341_WHITE);    // 境界線
  canvas.fillCircle(210, 22, 20, ILI9341_DARKGREY); // 外枠
  canvas.fillCircle(210, 22, 18, ILI9341_WHITE);    // 境界線
  canvas.fillCircle(70, 22, 17, ILI9341_DARKGREY); // Heatingランプ部消灯
  canvas.fillCircle(210, 22, 17, ILI9341_BLUE); // Waitingランプ部点灯


  // ボタン描画（左上x, 左上y, wide, high, ラベル, フォント, ボタン色, ラベル色）
  drawButton(25, 120, 140, 85, "Start", &FreeSansBold18pt7b, ILI9341_ORANGE, ILI9341_WHITE); // ONボタン
  drawButton(170, 120, 140, 85, "Cancel", &FreeSansBold18pt7b, ILI9341_GREEN, ILI9341_WHITE); // OFFボタン

  //スプライトをディスプレイ表示
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);


  if (ts.touched() == true) {  // タッチされていれば
    TS_Point tPoint = ts.getPoint();  // タッチ座標を取得

    // タッチx座標をTFT画面の座標に換算
    int16_t x = (tPoint.x-400) * TFT_WIDTH / (4095-550);  // タッチx座標をTFT画面の座標に換算
    int16_t y = (tPoint.y-230) * TFT_HEIGHT / (4095-420); // タッチy座標をTFT画面の座標に換算

    // ボタンタッチエリア検出
    if (x >= 25 && x <= 165 && y >= 120 && y <= 205){
      l = 0;
      time_start_to_now = 0.0;
      tone(sound,3000,100);
      page = 7;   // 範囲内ならpage7
    }
    if (x >= 170 && x <= 310 && y >= 120 && y <= 205){
      page = 4; // 範囲内ならpage4
      tone(sound,3000,100);
    }

  }

  
  return page;
}

/******************** 7ページ目 リフロー中 ********************/
int page_7(double smoothed_celsius){
  canvas.fillScreen(ILI9341_BLACK);   //背景色リセット
  drawText(92, 30, "Heating", &FreeSans9pt7b, ILI9341_WHITE);
  drawText(232, 30, "Waiting", &FreeSans9pt7b, ILI9341_WHITE);
  drawText(8, 85, "Temp:", &FreeSans18pt7b, ILI9341_WHITE);
  drawText(245, 88, "[degC]", &FreeSans12pt7b, ILI9341_WHITE);
  drawText(8, 130, "Phase:", &FreeSans18pt7b, ILI9341_WHITE);

  canvas.setTextColor(ILI9341_WHITE); 
  canvas.setFont(&FreeSans18pt7b);  // フォント指定
  canvas.setCursor(135, 85);          // 表示座標指定
  canvas.print(smoothed_celsius);

  time_now = millis();
  
  time_start_to_now = time_now - time_start;
  if(time_start_to_now >= 210000){
    time_remaining = 0;
  }
  else{
    time_remaining = 210000 - time_start_to_now;
  }
  
  canvas.setTextColor(ILI9341_WHITE); 
  canvas.setFont(&FreeSans9pt7b);  // フォント指定
  canvas.setCursor(3, 153);          // 表示座標指定
  canvas.print(time_start_to_now / 1000);    // 
  canvas.print("s");

  canvas.setFont(&FreeSans9pt7b);  // フォント指定
  canvas.setCursor(280, 153);          // 表示座標指定
  canvas.print(time_remaining / 1000);    // 
  canvas.print("s");
  

  
  if(0 <= time_start_to_now && time_start_to_now <= 30000){
    drawText(130, 130, "Heat up to 100", &FreeSans12pt7b, ILI9341_YELLOW);
  }
  
  if(30000 < time_start_to_now && time_start_to_now <= 120000){
    drawText(130, 130, "Heat up to 150", &FreeSans12pt7b, ILI9341_ORANGE); 
  }
  if(120000 < time_start_to_now && time_start_to_now <= 210000){
    drawText(130, 130, "Heat up to 235", &FreeSans12pt7b, ILI9341_RED);
  }
  if(210000 < time_start_to_now){
    check_reflow_start = false;
    canvas.fillRect(120, 30, 80, 30, ILI9341_BLACK);    //温度の表示を黒塗りで隠したい
    if(find_SD = true){
      writeSdData(data_time, plot);
    }
    ohuro();
    delay(8000);
    page = 12;
  }
  
  triangle_x = ((time_start_to_now / 1000) *297 / 210) + 8;
  triangle(triangle_x);


  // 平行線(x始点，y始点，長さ)
  canvas.drawFastHLine(0, 46, 320, ILI9341_WHITE);
  canvas.drawFastHLine(0, 100, 320, ILI9341_WHITE);
  canvas.drawFastHLine(0, 180, 320, ILI9341_WHITE);

  // スライドボリューム描画
  canvas.fillRect(10, 162, 300, 11, ILI9341_WHITE);    // 枠
  canvas.fillRect(12, 164, 42, 7, ILI9341_YELLOW);
  canvas.fillRect(54, 164, 127, 7, ILI9341_ORANGE);
  canvas.fillRect(181, 164, 127, 7, ILI9341_RED);

  // Heating-Waitingランプ描画(x, y, 半径, 色)
  canvas.fillCircle(70, 22, 20, ILI9341_DARKGREY); // 外枠
  canvas.fillCircle(70, 22, 18, ILI9341_WHITE);    // 境界線
  canvas.fillCircle(210, 22, 20, ILI9341_DARKGREY); // 外枠
  canvas.fillCircle(210, 22, 18, ILI9341_WHITE);    // 境界線
  auto_updateLamp(); // ボタン点灯状態更新関数呼び出し

  // ボタン描画（左上x, 左上y, wide, high, ラベル, フォント, ボタン色, ラベル色）
  drawButton(25, 185, 140, 50, "Graph", &FreeSans18pt7b, ILI9341_ORANGE, ILI9341_WHITE); // OFFボタン
  drawButton(170, 185, 140, 50, "Emergency", &FreeSansBold9pt7b, ILI9341_RED, ILI9341_YELLOW); // ONボタン

  //スプライトをディスプレイ表示
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);


  if (ts.touched() == true) {  // タッチされていれば
    TS_Point tPoint = ts.getPoint();  // タッチ座標を取得

    // タッチx座標をTFT画面の座標に換算
    int16_t x = (tPoint.x-400) * TFT_WIDTH / (4095-550);  // タッチx座標をTFT画面の座標に換算
    int16_t y = (tPoint.y-230) * TFT_HEIGHT / (4095-420); // タッチy座標をTFT画面の座標に換算

    // ボタンタッチエリア検出
    if (x >= 25 && x <= 165 && y >= 185 && y <= 235){
      page = 11;   // 範囲内ならpage11 グラフ
      tone(sound,3000,100);
    }
    if (x >= 170 && x <= 310 && y >= 185 && y <= 235){
      page = 2; // 範囲内ならpage8 Emargency
      tone(sound,3000,100);
    }

  }



  return page;
}


/******************** 8ページ目　**************************/
int page_8(){
  canvas.fillScreen(ILI9341_BLACK);   //背景色リセット
  
  drawText(92, 30, "Heating", &FreeSans9pt7b, ILI9341_WHITE);
  drawText(232, 30, "Waiting", &FreeSans9pt7b, ILI9341_WHITE);

  // 平行線(x始点，y始点，長さ)
  canvas.drawFastHLine(0, 46, 320, ILI9341_WHITE);

  // Heating-Waitingランプ描画(x, y, 半径, 色)
  canvas.fillCircle(70, 22, 20, ILI9341_DARKGREY); // 外枠
  canvas.fillCircle(70, 22, 18, ILI9341_WHITE);    // 境界線
  canvas.fillCircle(210, 22, 20, ILI9341_DARKGREY); // 外枠
  canvas.fillCircle(210, 22, 18, ILI9341_WHITE);    // 境界線
  updateLamp(); // ボタン点灯状態更新関数呼び出し



  // ボタン描画（左上x, 左上y, wide, high, ラベル, フォント, ボタン色, ラベル色）
  drawButton(60, 150, 200, 50, "Cancel", &FreeSans18pt7b, ILI9341_ORANGE, ILI9341_WHITE); // cancelボタン
  drawButton(60, 150, 200, 50, " ", &FreeSans9pt7b, ILI9341_BLACK, ILI9341_WHITE); // No.1ボタン
  drawText(92, 30, "No.1", &FreeSans9pt7b, ILI9341_WHITE);
  drawButton(60, 150, 200, 50, " ", &FreeSans9pt7b, ILI9341_BLACK, ILI9341_WHITE); // No.2ボタン
  drawText(92, 30, "No.2", &FreeSans9pt7b, ILI9341_WHITE);
  drawButton(60, 150, 200, 50, " ", &FreeSans9pt7b, ILI9341_BLACK, ILI9341_WHITE); // No.3ボタン
  drawText(92, 30, "No.3", &FreeSans9pt7b, ILI9341_WHITE);

  if (ts.touched() == true) {  // タッチされていれば
    TS_Point tPoint = ts.getPoint();  // タッチ座標を取得
    // タッチx座標をTFT画面の座標に換算
    int16_t x = (tPoint.x-400) * TFT_WIDTH / (4095-550);  // タッチx座標をTFT画面の座標に換算
    int16_t y = (tPoint.y-230) * TFT_HEIGHT / (4095-420); // タッチy座標をTFT画面の座標に換算

    // ボタンタッチエリア検出
    if (x >= 60 && x <= 260 && y >= 150 && y <= 200){
      page = 7;   // 範囲内ならpage7
      tone(sound,3000,100);
    }

    if (x >= 60 && x <= 260 && y >= 150 && y <= 200){
      no1 = 1;   // 範囲内ならNo.1
      tone(sound,3000,100);
      drawButton(60, 150, 200, 50, " ", &FreeSans9pt7b, ILI9341_RED, ILI9341_WHITE);
    }

    if (x >= 60 && x <= 260 && y >= 150 && y <= 200){
      no2 = 2;   // 範囲内ならNo.2
      tone(sound,3000,100);
      drawButton(60, 150, 200, 50, " ", &FreeSans9pt7b, ILI9341_RED, ILI9341_WHITE);
    }

    if (x >= 60 && x <= 260 && y >= 150 && y <= 200){
      no3 = 3;   // 範囲内ならNo.3
      tone(sound,3000,100);
      drawButton(60, 150, 200, 50, " ", &FreeSans9pt7b, ILI9341_RED, ILI9341_WHITE);
    }

  }

  no_calc = (no1 + no2) * no3;
  if(no_calc = 9)


  //スプライトをディスプレイ表示
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);

  return page;
}




/******************** 11ページ目 グラフ ********************/
int page_11(){
  canvas.fillScreen(ILI9341_BLACK);   //背景色リセット

  //グラフ軸
  canvas.drawFastHLine(5, 230, 315, ILI9341_WHITE);     //横軸
  canvas.drawFastVLine(5, 10, 220, ILI9341_WHITE);     //縦軸

  //グラフ目盛
  //縦軸
  canvas.drawFastHLine(4, 220, 3, ILI9341_WHITE);   //30
  canvas.drawFastHLine(4, 210, 3, ILI9341_WHITE);   //40
  canvas.drawFastHLine(3, 200, 5, ILI9341_WHITE);   //50
  drawText(8, 205, "50", &FreeSans9pt7b, ILI9341_WHITE);
  canvas.drawFastHLine(4, 190, 3, ILI9341_WHITE);   //60
  canvas.drawFastHLine(4, 180, 3, ILI9341_WHITE);    //70
  canvas.drawFastHLine(4, 170, 3, ILI9341_WHITE);   //80
  canvas.drawFastHLine(4, 160, 3, ILI9341_WHITE);   //90
  canvas.drawFastHLine(3, 150, 5, ILI9341_WHITE);   //100
  drawText(8, 155, "100", &FreeSans9pt7b, ILI9341_WHITE);
  canvas.drawFastHLine(4, 140, 3, ILI9341_WHITE);   //110
  canvas.drawFastHLine(4, 130, 3, ILI9341_WHITE);   //120
  canvas.drawFastHLine(4, 120, 3, ILI9341_WHITE);   //130
  canvas.drawFastHLine(4, 110, 3, ILI9341_WHITE);   //140
  canvas.drawFastHLine(4, 100, 3, ILI9341_WHITE);   //150
  canvas.drawFastHLine(4, 90, 3, ILI9341_WHITE);    //160
  canvas.drawFastHLine(4, 80, 3, ILI9341_WHITE);    //170
  canvas.drawFastHLine(4, 70, 3, ILI9341_WHITE);    //180
  canvas.drawFastHLine(4, 60, 3, ILI9341_WHITE);    //190
  canvas.drawFastHLine(3, 50, 5, ILI9341_WHITE);    //200
  drawText(8, 55, "200", &FreeSans9pt7b, ILI9341_WHITE);
  canvas.drawFastHLine(4, 40, 3, ILI9341_WHITE);    //210
  canvas.drawFastHLine(4, 30, 3, ILI9341_WHITE);    //220
  canvas.drawFastHLine(4, 20, 3, ILI9341_WHITE);    //230
  canvas.drawFastHLine(4, 10, 3, ILI9341_WHITE);    //240


  //横軸
  drawText(41, 225, "30", &FreeSans9pt7b, ILI9341_WHITE);
  canvas.drawFastVLine(50, 228, 5, ILI9341_WHITE);    //30
  canvas.drawFastVLine(95, 228, 5, ILI9341_WHITE);    //60
  canvas.drawFastVLine(140, 228, 5, ILI9341_WHITE);    //90
  drawText(170, 225, "120", &FreeSans9pt7b, ILI9341_WHITE);
  canvas.drawFastVLine(185, 228, 5, ILI9341_WHITE);   //120
  canvas.drawFastVLine(230, 228, 5, ILI9341_WHITE);    //150
  canvas.drawFastVLine(275, 228, 5, ILI9341_WHITE);    //180
  drawText(291, 225, "210", &FreeSans9pt7b, ILI9341_WHITE);
  canvas.drawFastVLine(319, 228, 5, ILI9341_WHITE);


  // グラフへのプロット
  if(num_loop != 1 && num_loop % 2 == 1){ //loopをまわした回数 = plot[]の要素数が2の倍数の時
    for(int a = 0; a <= (num_loop - 3); a += 2){
      data = (plot[a] + plot[a + 1]) / 2;
      plot_X = 5 + ((3*a) / 2) + 1;
      plot_Y = 230 - (data - 20.0 + 0.5);

      canvas.drawFastHLine(plot_X - 1, plot_Y - 1, 3, ILI9341_ORANGE);
      canvas.drawFastHLine(plot_X - 1, plot_Y, 3, ILI9341_ORANGE);
      canvas.drawFastHLine(plot_X - 1, plot_Y + 1, 3, ILI9341_ORANGE);
    }
  }
  
  
  if(num_loop != 0 && num_loop % 2 == 0){ //loopをまわした回数 = plot[]の要素数が2の倍数じゃない時
    for(int a = 0; a <= (num_loop - 2); a += 2){
      data = (plot[a] + plot[a + 1]) / 2;
      plot_X = 5 + ((3*a) / 2) + 1;
      plot_Y = 230 - (data - 20.0 + 0.5);

      canvas.drawFastHLine(plot_X - 1, plot_Y - 1, 3, ILI9341_ORANGE);
      canvas.drawFastHLine(plot_X - 1, plot_Y, 3, ILI9341_ORANGE);
      canvas.drawFastHLine(plot_X - 1, plot_Y + 1, 3, ILI9341_ORANGE);

    }
  }

  
  // ボタン描画（左上x, 左上y, wide, high, ラベル, フォント, ボタン色, ラベル色）
  drawButton(25, 100, 140, 50, "Cancel", &FreeSans18pt7b, ILI9341_ORANGE, ILI9341_WHITE); // page7に戻る
  

  //スプライトをディスプレイ表示
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);

  
  if (ts.touched() == true) {  // タッチされていれば
    TS_Point tPoint = ts.getPoint();  // タッチ座標を取得

    // タッチx座標をTFT画面の座標に換算
    int16_t x = (tPoint.x-400) * TFT_WIDTH / (4095-550);  // タッチx座標をTFT画面の座標に換算
    int16_t y = (tPoint.y-230) * TFT_HEIGHT / (4095-420); // タッチy座標をTFT画面の座標に換算

    // ボタンタッチエリア検出
    if (x >= 25 && x <= 165 && y >= 185 && y <= 235) {
      tone(sound,3000,100);
      page = 7;
    }   // 範囲内ならpage7


  }
  
  return page;
}

/******************* 12ページ目 リフロー終了画面 ********************/
int page_12(){
  canvas.fillScreen(ILI9341_BLACK);   //背景色リセット
  
  if(open_SD == true){
    drawText(80, 100, "Data saving...", &FreeSans18pt7b, ILI9341_WHITE);
    drawText(120, 50, "Don't turn off the power yet!!", &FreeSans18pt7b, ILI9341_WHITE);
  }
  else{
    drawText(92, 30, "Reflow is finished!!", &FreeSans12pt7b, ILI9341_WHITE);
    drawText(92, 30, "Temp data is saved to SD card.", &FreeSans12pt7b, ILI9341_WHITE);

    // ボタン描画（左上x, 左上y, wide, high, ラベル, フォント, ボタン色, ラベル色）
    drawButton(60, 150, 200, 50, "Main Menu", &FreeSans18pt7b, ILI9341_ORANGE, ILI9341_WHITE); // canselボタン

    if (ts.touched() == true) {  // タッチされていれば
      TS_Point tPoint = ts.getPoint();  // タッチ座標を取得

      // タッチx座標をTFT画面の座標に換算
      int16_t x = (tPoint.x-400) * TFT_WIDTH / (4095-550);  // タッチx座標をTFT画面の座標に換算
      int16_t y = (tPoint.y-230) * TFT_HEIGHT / (4095-420); // タッチy座標をTFT画面の座標に換算

      // ボタンタッチエリア検出
      if (x >= 60 && x <= 260 && y >= 150 && y <= 200){
        page = 2;   // 範囲内ならpage11 グラフ
        tone(sound,3000,100);
      }

    }

  }

  //スプライトをディスプレイ表示
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);
  return page;
}


/******************** 終了音 ********************************/
void ohuro(){
  tone(0, 806.964, 250); //ソ
  delay(250);
  tone(0, 718.923, 250);  //ファ
  delay(250);
  tone(0, 678.573, 500);  //ミ
  delay(500);

  tone(0, 806.964, 250); //ソ
  delay(250);
  tone(0, 1077.164, 250); //ド
  delay(250);
  tone(0, 1016.710, 500); //シ
  delay(500);
  
  tone(0, 806.964, 250);  //ソ
  delay(250);
  tone(0, 1209.079, 250); //レ
  delay(250);
  tone(0, 1077.164, 500); //ド
  delay(500);
  tone(0, 1357.146, 500); //ミ
  delay(500);
  delay(500);

  tone(0, 1077.164, 250); //ド
  delay(250);
  tone(0, 1016.710, 250); //シ
  delay(250);
  tone(0, 905.786, 500); //ラ
  delay(500);

  tone(0, 1437.846, 250); //ファ
  delay(250);
  tone(0, 1209.079, 250); //レ
  delay(250);
  tone(0, 1077.167, 500); //ド
  delay(500);
  tone(0, 1016.710, 500);  //シ
  delay(500);

  tone(0, 1077.164, 1000); //ド
  delay(1000);

}



/****************** SDカード書き込み **************************/
void writeSdData(double data_time[], double plot[]) {
  myFile = SD.open(SD_FILENAME, FILE_WRITE); // SDカードのファイルを開く
  
  // データ書き込み
  if (myFile) { // ファイルが開けたら
    open_SD = true;
    for(int d = 0; d <= num_loop; d++)
      myFile.printf("%f,%f\n", data_time[d], plot[d]); // テキストと整数データを書き込み
    myFile.close(); // ファイルを閉じる
    open_SD = false;
  } else { // ファイルが開けなければ
    Serial.println("ファイルを開けませんでした");
    
  }
}


/********************* JPEG表示 *************************/
void jpegDraw(const char* filename) {
  JpegDec.decodeSdFile(filename); // JPEGファイルをSDカードからデコード実行
  
  // シリアル出力、デコード画像情報(MCU [Minimum Coded Unit]：JPEG画像データの最小処理単位
  Serial.printf("Size : %d x %d\nMCU : %d x %d\n", JpegDec.width, JpegDec.height, JpegDec.MCUWidth, JpegDec.MCUHeight);
  Serial.printf("Components: %d\nMCU / row: %d\nMCU / col: %d\nScan type: %d\n\n", JpegDec.comps, JpegDec.MCUSPerRow, JpegDec.MCUSPerCol, JpegDec.scanType);  

  uint16_t *pImg;   // ピクセルデータ用のポインタ
  while (JpegDec.read()) {  // JPEGデータを読み込む
    pImg = JpegDec.pImage;  // 現在のピクセルデータのポインタを取得
    for (int h = 0; h < JpegDec.MCUHeight; h++) { // MCU高さ分ループ
      for (int w = 0; w < JpegDec.MCUWidth; w++) {  // MCU幅分ループ
        // 現在のピクセルのx, y座標を計算
        int x = JpegDec.MCUx * JpegDec.MCUWidth + w;  // x座標
        int y = JpegDec.MCUy * JpegDec.MCUHeight + h; // y座標
        if (x < JpegDec.width && y < JpegDec.height) {  // ピクセルが画像範囲内なら
            canvas.drawRGBBitmap(x, y, pImg, 1, 1); // 液晶画面にピクセルを描画
        }
        pImg += JpegDec.comps;  // ポインタを次のピクセルデータへ進める
      }
    }
  }

}




/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////




const int Pin_thermistor = 26; 
float celsius = 1;
float smoothed_celsius = 23.0; //平滑化後の温度 ←直温度取得1回目の値で初期化した方がいいか？
int duty = 0; //デューティー比 
float PWM_OUT_V; //PWM制御による平均出力電圧
int i;
int under_temp_V , over_temp_V = 0; //Voltageのtemp_V配列内近似値　温度低い方 = under_temp_V, 温度高い方 = over_temp_V，最近似値 = neaest_temp_V
int Voltage;
int num_temp_V = 0;
int Pin_thermistor_num = 0;

int left = 0;
int right = 1495; // 要素数-1
int mid;



/*
//↓PID用
const float P =2; //Pゲイン
const float I =9; //Iゲイン
const float D =0; //Dゲイン
float target_temp = 7.0; //目標温度
float diff_temp_before = 0.0; //微分初期値 ←計測1回目の目標との差[℃]に変更する
float diff_temp = 0.0; //目標温度との最新の差
float T = 0.01; //PID制御周期
float I_diff_temp = 0.0; //I要素（積分）
float D_diff_temp = 0.0; //D要素（微分）
float u = 0.0; //PIDからの出力[%]
//↑
*/


const int temp_V[] = {3856, 3855, 3854, 3853, 3851, 3850, 3849, 3848, 3847, 3846, 3845, 3843, 3842, 3841, 3840, 3839, 3838, 3836, 3835, 3834, 3833, 3832, 3830, 3829, 3828, 3827, 3825, 3824, 3823, 3822, 3821, 3819, 3818, 3817, 3816, 3814, 3813, 3812, 3810, 3809, 3808, 3807, 3805, 3804, 3803, 3801, 3800, 3799, 3797, 3796, 3795, 3793, 3792, 3791, 3789, 3788, 3787, 3785, 3784, 3783, 3781, 3780, 3778, 3777, 3776, 3774, 3773, 3771, 3770, 3769, 3767, 3766, 3764, 3763, 3761, 3760, 3759, 3757, 3756, 3754, 3753, 3751, 3750, 3748, 3747, 3745, 3744, 3742, 3741, 3739, 3738, 3736, 3735, 3733, 3732, 3730, 3728, 3727, 3725, 3724, 3722, 3721, 3719, 3717, 3716, 3714, 3713, 3711, 3710, 3708, 3706, 3705, 3703, 3701, 3700, 3698, 3696, 3695, 3693, 3691, 3690, 3688, 3686, 3685, 3683, 3681, 3680, 3678, 3676, 3675, 3673, 3671, 3669, 3668, 3666, 3664, 3662, 3661, 3659, 3657, 3655, 3654, 3652, 3650, 3648, 3646, 3645, 3643, 3641, 3639, 3637, 3636, 3634, 3632, 3630, 3628, 3626, 3625, 3623, 3621, 3619, 3617, 3615, 3613, 3611, 3610, 3608, 3606, 3604, 3602, 3600, 3598, 3596, 3594, 3592, 3590, 3588, 3586, 3584, 3582, 3580, 3578, 3576, 3574, 3572, 3570, 3568, 3566, 3564, 3562, 3560, 3558, 3556, 3554, 3552, 3550, 3548, 3546, 3544, 3542, 3540, 3538, 3536, 3534, 3531, 3529, 3527, 3525, 3523, 3521, 3519, 3517, 3514, 3512, 3510, 3508, 3506, 3504, 3501, 3499, 3497, 3495, 3493, 3490, 3488, 3486, 3484, 3482, 3479, 3477, 3475, 3473, 3470, 3468, 3466, 3464, 3461, 3459, 3457, 3455, 3452, 3450, 3448, 3445, 3443, 3441, 3438, 3436, 3434, 3431, 3429, 3427, 3424, 3422, 3420, 3417, 3415, 3413, 3410, 3408, 3405, 3403, 3401, 3398, 3396, 3393, 3391, 3388, 3386, 3384, 3381, 3379, 3376, 3374, 3371, 3369, 3366, 3364, 3361, 3359, 3356, 3354, 3351, 3349, 3346, 3344, 3341, 3339, 3336, 3334, 3331, 3329, 3326, 3323, 3321, 3318, 3316, 3313, 3311, 3308, 3305, 3303, 3300, 3298, 3295, 3292, 3290, 3287, 3285, 3282, 3279, 3277, 3274, 3271, 3269, 3266, 3263, 3261, 3258, 3255, 3253, 3250, 3247, 3244, 3242, 3239, 3236, 3234, 3231, 3228, 3225, 3223, 3220, 3217, 3214, 3212, 3209, 3206, 3203, 3201, 3198, 3195, 3192, 3189, 3187, 3184, 3181, 3178, 3175, 3173, 3170, 3167, 3164, 3161, 3158, 3156, 3153, 3150, 3147, 3144, 3141, 3138, 3135, 3133, 3130, 3127, 3124, 3121, 3118, 3115, 3112, 3109, 3106, 3104, 3101, 3098, 3095, 3092, 3089, 3086, 3083, 3080, 3077, 3074, 3071, 3068, 3065, 3062, 3059, 3056, 3053, 3050, 3047, 3044, 3041, 3038, 3035, 3032, 3029, 3026, 3023, 3020, 3017, 3014, 3011, 3008, 3005, 3002, 2999, 2996, 2993, 2989, 2986, 2983, 2980, 2977, 2974, 2971, 2968, 2965, 2962, 2959, 2956, 2952, 2949, 2946, 2943, 2940, 2937, 2934, 2931, 2927, 2924, 2921, 2918, 2915, 2912, 2909, 2905, 2902, 2899, 2896, 2893, 2890, 2886, 2883, 2880, 2877, 2874, 2870, 2867, 2864, 2861, 2858, 2854, 2851, 2848, 2845, 2842, 2838, 2835, 2832, 2829, 2825, 2822, 2819, 2816, 2813, 2809, 2806, 2803, 2800, 2796, 2793, 2790, 2787, 2783, 2780, 2777, 2773, 2770, 2767, 2764, 2760, 2757, 2754, 2751, 2747, 2744, 2741, 2737, 2734, 2731, 2727, 2724, 2721, 2718, 2714, 2711, 2708, 2704, 2701, 2698, 2694, 2691, 2688, 2684, 2681, 2678, 2674, 2671, 2668, 2664, 2661, 2658, 2654, 2651, 2648, 2644, 2641, 2638, 2634, 2631, 2628, 2624, 2621, 2618, 2614, 2611, 2608, 2604, 2601, 2597, 2594, 2591, 2587, 2584, 2581, 2577, 2574, 2570, 2567, 2564, 2560, 2557, 2554, 2550, 2547, 2543, 2540, 2537, 2533, 2530, 2527, 2523, 2520, 2516, 2513, 2510, 2506, 2503, 2499, 2496, 2493, 2489, 2486, 2483, 2479, 2476, 2472, 2469, 2466, 2462, 2459, 2455, 2452, 2449, 2445, 2442, 2438, 2435, 2432, 2428, 2425, 2421, 2418, 2415, 2411, 2408, 2404, 2401, 2398, 2394, 2391, 2387, 2384, 2381, 2377, 2374, 2370, 2367, 2364, 2360, 2357, 2353, 2350, 2347, 2343, 2340, 2336, 2333, 2330, 2326, 2323, 2319, 2316, 2313, 2309, 2306, 2302, 2299, 2296, 2292, 2289, 2285, 2282, 2279, 2275, 2272, 2269, 2265, 2262, 2258, 2255, 2252, 2248, 2245, 2241, 2238, 2235, 2231, 2228, 2225, 2221, 2218, 2214, 2211, 2208, 2204, 2201, 2198, 2194, 2191, 2187, 2184, 2181, 2177, 2174, 2171, 2167, 2164, 2161, 2157, 2154, 2151, 2147, 2144, 2140, 2137, 2134, 2130, 2127, 2124, 2120, 2117, 2114, 2110, 2107, 2104, 2100, 2097, 2094, 2090, 2087, 2084, 2080, 2077, 2074, 2070, 2067, 2064, 2061, 2057, 2054, 2051, 2047, 2044, 2041, 2037, 2034, 2031, 2028, 2024, 2021, 2018, 2014, 2011, 2008, 2005, 2001, 1998, 1995, 1991, 1988, 1985, 1982, 1978, 1975, 1972, 1969, 1965, 1962, 1959, 1956, 1952, 1949, 1946, 1943, 1939, 1936, 1933, 1930, 1927, 1923, 1920, 1917, 1914, 1910, 1907, 1904, 1901, 1898, 1894, 1891, 1888, 1885, 1882, 1878, 1875, 1872, 1869, 1866, 1863, 1859, 1856, 1853, 1850, 1847, 1844, 1840, 1837, 1834, 1831, 1828, 1825, 1822, 1818, 1815, 1812, 1809, 1806, 1803, 1800, 1797, 1793, 1790, 1787, 1784, 1781, 1778, 1775, 1772, 1769, 1766, 1762, 1759, 1756, 1753, 1750, 1747, 1744, 1741, 1738, 1735, 1732, 1729, 1726, 1723, 1720, 1717, 1713, 1710, 1707, 1704, 1701, 1698, 1695, 1692, 1689, 1686, 1683, 1680, 1677, 1674, 1671, 1668, 1665, 1662, 1659, 1656, 1653, 1650, 1648, 1645, 1642, 1639, 1636, 1633, 1630, 1627, 1624, 1621, 1618, 1615, 1612, 1609, 1606, 1603, 1601, 1598, 1595, 1592, 1589, 1586, 1583, 1580, 1577, 1575, 1572, 1569, 1566, 1563, 1560, 1557, 1554, 1552, 1549, 1546, 1543, 1540, 1537, 1535, 1532, 1529, 1526, 1523, 1521, 1518, 1515, 1512, 1509, 1507, 1504, 1501, 1498, 1495, 1493, 1490, 1487, 1484, 1482, 1479, 1476, 1473, 1471, 1468, 1465, 1462, 1460, 1457, 1454, 1451, 1449, 1446, 1443, 1441, 1438, 1435, 1433, 1430, 1427, 1424, 1422, 1419, 1416, 1414, 1411, 1408, 1406, 1403, 1400, 1398, 1395, 1393, 1390, 1387, 1385, 1382, 1379, 1377, 1374, 1372, 1369, 1366, 1364, 1361, 1359, 1356, 1353, 1351, 1348, 1346, 1343, 1341, 1338, 1336, 1333, 1330, 1328, 1325, 1323, 1320, 1318, 1315, 1313, 1310, 1308, 1305, 1303, 1300, 1298, 1295, 1293, 1290, 1288, 1285, 1283, 1280, 1278, 1275, 1273, 1271, 1268, 1266, 1263, 1261, 1258, 1256, 1254, 1251, 1249, 1246, 1244, 1241, 1239, 1237, 1234, 1232, 1229, 1227, 1225, 1222, 1220, 1218, 1215, 1213, 1211, 1208, 1206, 1203, 1201, 1199, 1196, 1194, 1192, 1190, 1187, 1185, 1183, 1180, 1178, 1176, 1173, 1171, 1169, 1167, 1164, 1162, 1160, 1157, 1155, 1153, 1151, 1148, 1146, 1144, 1142, 1139, 1137, 1135, 1133, 1131, 1128, 1126, 1124, 1122, 1120, 1117, 1115, 1113, 1111, 1109, 1106, 1104, 1102, 1100, 1098, 1096, 1093, 1091, 1089, 1087, 1085, 1083, 1081, 1078, 1076, 1074, 1072, 1070, 1068, 1066, 1064, 1062, 1059, 1057, 1055, 1053, 1051, 1049, 1047, 1045, 1043, 1041, 1039, 1037, 1035, 1033, 1030, 1028, 1026, 1024, 1022, 1020, 1018, 1016, 1014, 1012, 1010, 1008, 1006, 1004, 1002, 1000, 998, 996, 994, 992, 990, 988, 986, 985, 983, 981, 979, 977, 975, 973, 971, 969, 967, 965, 963, 961, 959, 958, 956, 954, 952, 950, 948, 946, 944, 942, 941, 939, 937, 935, 933, 931, 929, 927, 926, 924, 922, 920, 918, 916, 915, 913, 911, 909, 907, 906, 904, 902, 900, 898, 897, 895, 893, 891, 889, 888, 886, 884, 882, 881, 879, 877, 875, 874, 872, 870, 868, 867, 865, 863, 861, 860, 858, 856, 855, 853, 851, 850, 848, 846, 844, 843, 841, 839, 838, 836, 834, 833, 831, 829, 828, 826, 824, 823, 821, 819, 818, 816, 815, 813, 811, 810, 808, 806, 805, 803, 802, 800, 798, 797, 795, 794, 792, 791, 789, 787, 786, 784, 783, 781, 780, 778, 776, 775, 773, 772, 770, 769, 767, 766, 764, 763, 761, 760, 758, 757, 755, 754, 752, 751, 749, 748, 746, 745, 743, 742, 740, 739, 737, 736, 734, 733, 731, 730, 728, 727, 725, 724, 723, 721, 720, 718, 717, 715, 714, 713, 711, 710, 708, 707, 705, 704, 703, 701, 700, 698, 697, 696, 694, 693, 692, 690, 689, 687, 686, 685, 683, 682, 681, 679, 678, 677, 675, 674, 673, 671, 670, 669, 667, 666, 665, 663, 662, 661, 659, 658, 657, 655, 654, 653, 651, 650, 649, 648, 646, 645, 644, 642, 641, 640, 639, 637, 636, 635, 634, 632, 631, 630, 629, 627, 626, 625, 624, 622, 621, 620, 619, 617, 616, 615, 614, 613, 611, 610, 609, 608, 607, 605, 604, 603, 602, 601, 599, 598, 597, 596, 595, 594, 592, 591, 590, 589, 588, 577, 565, 554, 544, 533, 523, 513, 503, 493, 484, 474, 465, 456, 448, 439, 431, 423, 415, 407, 399, 392, 384, 377, 370, 363, 357, 350, 344, 337, 331, 325, 319, 313, 308, 302, 297, 291, 286, 281, 276, 271, 266, 262, 257, 252, 248, 244, 239, 235, 231, 227, 223, 220, 216, 212, 209, 205, 202, 198, 195, 192, 188, 185, 182, 179, 176, 173, 171, 168, 165, 162, 160, 157, 155, 152, 150, 148, 145, 143, 141, 139, 136, 134, 132, 130, 128, 126, 124, 122, 121, 119, 117, 115, 113, 112, 110, 109, 107, 105, 104, 102, 101, 99, 98, 97, 95, 94, 92, 91, 90, 89, 87, 86, 85, 84, 83, 81, 80, 79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 69, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 56, 55, 54, 53}; //各温度に対応する電圧値
const float temp[] = {15.0, 15.1, 15.2, 15.3, 15.4, 15.5, 15.6, 15.7, 15.8, 15.9, 16.0, 16.1, 16.2, 16.3, 16.4, 16.5, 16.6, 16.7, 16.8, 16.9, 17.0, 17.1, 17.2, 17.3, 17.4, 17.5, 17.6, 17.7, 17.8, 17.9, 18.0, 18.1, 18.2, 18.3, 18.4, 18.5, 18.6, 18.7, 18.8, 18.9, 19.0, 19.1, 19.2, 19.3, 19.4, 19.5, 19.6, 19.7, 19.8, 19.9, 20.0, 20.1, 20.2, 20.3, 20.4, 20.5, 20.6, 20.7, 20.8, 20.9, 21.0, 21.1, 21.2, 21.3, 21.4, 21.5, 21.6, 21.7, 21.8, 21.9, 22.0, 22.1, 22.2, 22.3, 22.4, 22.5, 22.6, 22.7, 22.8, 22.9, 23.0, 23.1, 23.2, 23.3, 23.4, 23.5, 23.6, 23.7, 23.8, 23.9, 24.0, 24.1, 24.2, 24.3, 24.4, 24.5, 24.6, 24.7, 24.8, 24.9, 25.0, 25.1, 25.2, 25.3, 25.4, 25.5, 25.6, 25.7, 25.8, 25.9, 26.0, 26.1, 26.2, 26.3, 26.4, 26.5, 26.6, 26.7, 26.8, 26.9, 27.0, 27.1, 27.2, 27.3, 27.4, 27.5, 27.6, 27.7, 27.8, 27.9, 28.0, 28.1, 28.2, 28.3, 28.4, 28.5, 28.6, 28.7, 28.8, 28.9, 29.0, 29.1, 29.2, 29.3, 29.4, 29.5, 29.6, 29.7, 29.8, 29.9, 30.0, 30.1, 30.2, 30.3, 30.4, 30.5, 30.6, 30.7, 30.8, 30.9, 31.0, 31.1, 31.2, 31.3, 31.4, 31.5, 31.6, 31.7, 31.8, 31.9, 32.0, 32.1, 32.2, 32.3, 32.4, 32.5, 32.6, 32.7, 32.8, 32.9, 33.0, 33.1, 33.2, 33.3, 33.4, 33.5, 33.6, 33.7, 33.8, 33.9, 34.0, 34.1, 34.2, 34.3, 34.4, 34.5, 34.6, 34.7, 34.8, 34.9, 35.0, 35.1, 35.2, 35.3, 35.4, 35.5, 35.6, 35.7, 35.8, 35.9, 36.0, 36.1, 36.2, 36.3, 36.4, 36.5, 36.6, 36.7, 36.8, 36.9, 37.0, 37.1, 37.2, 37.3, 37.4, 37.5, 37.6, 37.7, 37.8, 37.9, 38.0, 38.1, 38.2, 38.3, 38.4, 38.5, 38.6, 38.7, 38.8, 38.9, 39.0, 39.1, 39.2, 39.3, 39.4, 39.5, 39.6, 39.7, 39.8, 39.9, 40.0, 40.1, 40.2, 40.3, 40.4, 40.5, 40.6, 40.7, 40.8, 40.9, 41.0, 41.1, 41.2, 41.3, 41.4, 41.5, 41.6, 41.7, 41.8, 41.9, 42.0, 42.1, 42.2, 42.3, 42.4, 42.5, 42.6, 42.7, 42.8, 42.9, 43.0, 43.1, 43.2, 43.3, 43.4, 43.5, 43.6, 43.7, 43.8, 43.9, 44.0, 44.1, 44.2, 44.3, 44.4, 44.5, 44.6, 44.7, 44.8, 44.9, 45.0, 45.1, 45.2, 45.3, 45.4, 45.5, 45.6, 45.7, 45.8, 45.9, 46.0, 46.1, 46.2, 46.3, 46.4, 46.5, 46.6, 46.7, 46.8, 46.9, 47.0, 47.1, 47.2, 47.3, 47.4, 47.5, 47.6, 47.7, 47.8, 47.9, 48.0, 48.1, 48.2, 48.3, 48.4, 48.5, 48.6, 48.7, 48.8, 48.9, 49.0, 49.1, 49.2, 49.3, 49.4, 49.5, 49.6, 49.7, 49.8, 49.9, 50.0, 50.1, 50.2, 50.3, 50.4, 50.5, 50.6, 50.7, 50.8, 50.9, 51.0, 51.1, 51.2, 51.3, 51.4, 51.5, 51.6, 51.7, 51.8, 51.9, 52.0, 52.1, 52.2, 52.3, 52.4, 52.5, 52.6, 52.7, 52.8, 52.9, 53.0, 53.1, 53.2, 53.3, 53.4, 53.5, 53.6, 53.7, 53.8, 53.9, 54.0, 54.1, 54.2, 54.3, 54.4, 54.5, 54.6, 54.7, 54.8, 54.9, 55.0, 55.1, 55.2, 55.3, 55.4, 55.5, 55.6, 55.7, 55.8, 55.9, 56.0, 56.1, 56.2, 56.3, 56.4, 56.5, 56.6, 56.7, 56.8, 56.9, 57.0, 57.1, 57.2, 57.3, 57.4, 57.5, 57.6, 57.7, 57.8, 57.9, 58.0, 58.1, 58.2, 58.3, 58.4, 58.5, 58.6, 58.7, 58.8, 58.9, 59.0, 59.1, 59.2, 59.3, 59.4, 59.5, 59.6, 59.7, 59.8, 59.9, 60.0, 60.1, 60.2, 60.3, 60.4, 60.5, 60.6, 60.7, 60.8, 60.9, 61.0, 61.1, 61.2, 61.3, 61.4, 61.5, 61.6, 61.7, 61.8, 61.9, 62.0, 62.1, 62.2, 62.3, 62.4, 62.5, 62.6, 62.7, 62.8, 62.9, 63.0, 63.1, 63.2, 63.3, 63.4, 63.5, 63.6, 63.7, 63.8, 63.9, 64.0, 64.1, 64.2, 64.3, 64.4, 64.5, 64.6, 64.7, 64.8, 64.9, 65.0, 65.1, 65.2, 65.3, 65.4, 65.5, 65.6, 65.7, 65.8, 65.9, 66.0, 66.1, 66.2, 66.3, 66.4, 66.5, 66.6, 66.7, 66.8, 66.9, 67.0, 67.1, 67.2, 67.3, 67.4, 67.5, 67.6, 67.7, 67.8, 67.9, 68.0, 68.1, 68.2, 68.3, 68.4, 68.5, 68.6, 68.7, 68.8, 68.9, 69.0, 69.1, 69.2, 69.3, 69.4, 69.5, 69.6, 69.7, 69.8, 69.9, 70.0, 70.1, 70.2, 70.3, 70.4, 70.5, 70.6, 70.7, 70.8, 70.9, 71.0, 71.1, 71.2, 71.3, 71.4, 71.5, 71.6, 71.7, 71.8, 71.9, 72.0, 72.1, 72.2, 72.3, 72.4, 72.5, 72.6, 72.7, 72.8, 72.9, 73.0, 73.1, 73.2, 73.3, 73.4, 73.5, 73.6, 73.7, 73.8, 73.9, 74.0, 74.1, 74.2, 74.3, 74.4, 74.5, 74.6, 74.7, 74.8, 74.9, 75.0, 75.1, 75.2, 75.3, 75.4, 75.5, 75.6, 75.7, 75.8, 75.9, 76.0, 76.1, 76.2, 76.3, 76.4, 76.5, 76.6, 76.7, 76.8, 76.9, 77.0, 77.1, 77.2, 77.3, 77.4, 77.5, 77.6, 77.7, 77.8, 77.9, 78.0, 78.1, 78.2, 78.3, 78.4, 78.5, 78.6, 78.7, 78.8, 78.9, 79.0, 79.1, 79.2, 79.3, 79.4, 79.5, 79.6, 79.7, 79.8, 79.9, 80.0, 80.1, 80.2, 80.3, 80.4, 80.5, 80.6, 80.7, 80.8, 80.9, 81.0, 81.1, 81.2, 81.3, 81.4, 81.5, 81.6, 81.7, 81.8, 81.9, 82.0, 82.1, 82.2, 82.3, 82.4, 82.5, 82.6, 82.7, 82.8, 82.9, 83.0, 83.1, 83.2, 83.3, 83.4, 83.5, 83.6, 83.7, 83.8, 83.9, 84.0, 84.1, 84.2, 84.3, 84.4, 84.5, 84.6, 84.7, 84.8, 84.9, 85.0, 85.1, 85.2, 85.3, 85.4, 85.5, 85.6, 85.7, 85.8, 85.9, 86.0, 86.1, 86.2, 86.3, 86.4, 86.5, 86.6, 86.7, 86.8, 86.9, 87.0, 87.1, 87.2, 87.3, 87.4, 87.5, 87.6, 87.7, 87.8, 87.9, 88.0, 88.1, 88.2, 88.3, 88.4, 88.5, 88.6, 88.7, 88.8, 88.9, 89.0, 89.1, 89.2, 89.3, 89.4, 89.5, 89.6, 89.7, 89.8, 89.9, 90.0, 90.1, 90.2, 90.3, 90.4, 90.5, 90.6, 90.7, 90.8, 90.9, 91.0, 91.1, 91.2, 91.3, 91.4, 91.5, 91.6, 91.7, 91.8, 91.9, 92.0, 92.1, 92.2, 92.3, 92.4, 92.5, 92.6, 92.7, 92.8, 92.9, 93.0, 93.1, 93.2, 93.3, 93.4, 93.5, 93.6, 93.7, 93.8, 93.9, 94.0, 94.1, 94.2, 94.3, 94.4, 94.5, 94.6, 94.7, 94.8, 94.9, 95.0, 95.1, 95.2, 95.3, 95.4, 95.5, 95.6, 95.7, 95.8, 95.9, 96.0, 96.1, 96.2, 96.3, 96.4, 96.5, 96.6, 96.7, 96.8, 96.9, 97.0, 97.1, 97.2, 97.3, 97.4, 97.5, 97.6, 97.7, 97.8, 97.9, 98.0, 98.1, 98.2, 98.3, 98.4, 98.5, 98.6, 98.7, 98.8, 98.9, 99.0, 99.1, 99.2, 99.3, 99.4, 99.5, 99.6, 99.7, 99.8, 99.9, 100.0, 100.1, 100.2, 100.3, 100.4, 100.5, 100.6, 100.7, 100.8, 100.9, 101.0, 101.1, 101.2, 101.3, 101.4, 101.5, 101.6, 101.7, 101.8, 101.9, 102.0, 102.1, 102.2, 102.3, 102.4, 102.5, 102.6, 102.7, 102.8, 102.9, 103.0, 103.1, 103.2, 103.3, 103.4, 103.5, 103.6, 103.7, 103.8, 103.9, 104.0, 104.1, 104.2, 104.3, 104.4, 104.5, 104.6, 104.7, 104.8, 104.9, 105.0, 105.1, 105.2, 105.3, 105.4, 105.5, 105.6, 105.7, 105.8, 105.9, 106.0, 106.1, 106.2, 106.3, 106.4, 106.5, 106.6, 106.7, 106.8, 106.9, 107.0, 107.1, 107.2, 107.3, 107.4, 107.5, 107.6, 107.7, 107.8, 107.9, 108.0, 108.1, 108.2, 108.3, 108.4, 108.5, 108.6, 108.7, 108.8, 108.9, 109.0, 109.1, 109.2, 109.3, 109.4, 109.5, 109.6, 109.7, 109.8, 109.9, 110.0, 110.1, 110.2, 110.3, 110.4, 110.5, 110.6, 110.7, 110.8, 110.9, 111.0, 111.1, 111.2, 111.3, 111.4, 111.5, 111.6, 111.7, 111.8, 111.9, 112.0, 112.1, 112.2, 112.3, 112.4, 112.5, 112.6, 112.7, 112.8, 112.9, 113.0, 113.1, 113.2, 113.3, 113.4, 113.5, 113.6, 113.7, 113.8, 113.9, 114.0, 114.1, 114.2, 114.3, 114.4, 114.5, 114.6, 114.7, 114.8, 114.9, 115.0, 115.1, 115.2, 115.3, 115.4, 115.5, 115.6, 115.7, 115.8, 115.9, 116.0, 116.1, 116.2, 116.3, 116.4, 116.5, 116.6, 116.7, 116.8, 116.9, 117.0, 117.1, 117.2, 117.3, 117.4, 117.5, 117.6, 117.7, 117.8, 117.9, 118.0, 118.1, 118.2, 118.3, 118.4, 118.5, 118.6, 118.7, 118.8, 118.9, 119.0, 119.1, 119.2, 119.3, 119.4, 119.5, 119.6, 119.7, 119.8, 119.9, 120.0, 120.1, 120.2, 120.3, 120.4, 120.5, 120.6, 120.7, 120.8, 120.9, 121.0, 121.1, 121.2, 121.3, 121.4, 121.5, 121.6, 121.7, 121.8, 121.9, 122.0, 122.1, 122.2, 122.3, 122.4, 122.5, 122.6, 122.7, 122.8, 122.9, 123.0, 123.1, 123.2, 123.3, 123.4, 123.5, 123.6, 123.7, 123.8, 123.9, 124.0, 124.1, 124.2, 124.3, 124.4, 124.5, 124.6, 124.7, 124.8, 124.9, 125.0, 125.1, 125.2, 125.3, 125.4, 125.5, 125.6, 125.7, 125.8, 125.9, 126.0, 126.1, 126.2, 126.3, 126.4, 126.5, 126.6, 126.7, 126.8, 126.9, 127.0, 127.1, 127.2, 127.3, 127.4, 127.5, 127.6, 127.7, 127.8, 127.9, 128.0, 128.1, 128.2, 128.3, 128.4, 128.5, 128.6, 128.7, 128.8, 128.9, 129.0, 129.1, 129.2, 129.3, 129.4, 129.5, 129.6, 129.7, 129.8, 129.9, 130.0, 130.1, 130.2, 130.3, 130.4, 130.5, 130.6, 130.7, 130.8, 130.9, 131.0, 131.1, 131.2, 131.3, 131.4, 131.5, 131.6, 131.7, 131.8, 131.9, 132.0, 132.1, 132.2, 132.3, 132.4, 132.5, 132.6, 132.7, 132.8, 132.9, 133.0, 133.1, 133.2, 133.3, 133.4, 133.5, 133.6, 133.7, 133.8, 133.9, 134.0, 134.1, 134.2, 134.3, 134.4, 134.5, 134.6, 134.7, 134.8, 134.9, 135.0, 135.1, 135.2, 135.3, 135.4, 135.5, 135.6, 135.7, 135.8, 135.9, 136.0, 136.1, 136.2, 136.3, 136.4, 136.5, 136.6, 136.7, 136.8, 136.9, 137.0, 137.1, 137.2, 137.3, 137.4, 137.5, 137.6, 137.7, 137.8, 137.9, 138.0, 138.1, 138.2, 138.3, 138.4, 138.5, 138.6, 138.7, 138.8, 138.9, 139.0, 139.1, 139.2, 139.3, 139.4, 139.5, 139.6, 139.7, 139.8, 139.9, 140.0, 140.1, 140.2, 140.3, 140.4, 140.5, 140.6, 140.7, 140.8, 140.9, 141.0, 141.1, 141.2, 141.3, 141.4, 141.5, 141.6, 141.7, 141.8, 141.9, 142.0, 142.1, 142.2, 142.3, 142.4, 142.5, 142.6, 142.7, 142.8, 142.9, 143.0, 143.1, 143.2, 143.3, 143.4, 143.5, 143.6, 143.7, 143.8, 143.9, 144.0, 144.1, 144.2, 144.3, 144.4, 144.5, 144.6, 144.7, 144.8, 144.9, 145.0, 145.1, 145.2, 145.3, 145.4, 145.5, 145.6, 145.7, 145.8, 145.9, 146.0, 146.1, 146.2, 146.3, 146.4, 146.5, 146.6, 146.7, 146.8, 146.9, 147.0, 147.1, 147.2, 147.3, 147.4, 147.5, 147.6, 147.7, 147.8, 147.9, 148.0, 148.1, 148.2, 148.3, 148.4, 148.5, 148.6, 148.7, 148.8, 148.9, 149.0, 149.1, 149.2, 149.3, 149.4, 149.5, 149.6, 149.7, 149.8, 149.9, 150.0, 151.0, 152.0, 153.0, 154.0, 155.0, 156.0, 157.0, 158.0, 159.0, 160.0, 161.0, 162.0, 163.0, 164.0, 165.0, 166.0, 167.0, 168.0, 169.0, 170.0, 171.0, 172.0, 173.0, 174.0, 175.0, 176.0, 177.0, 178.0, 179.0, 180.0, 181.0, 182.0, 183.0, 184.0, 185.0, 186.0, 187.0, 188.0, 189.0, 190.0, 191.0, 192.0, 193.0, 194.0, 195.0, 196.0, 197.0, 198.0, 199.0, 200.0, 201.0, 202.0, 203.0, 204.0, 205.0, 206.0, 207.0, 208.0, 209.0, 210.0, 211.0, 212.0, 213.0, 214.0, 215.0, 216.0, 217.0, 218.0, 219.0, 220.0, 221.0, 222.0, 223.0, 224.0, 225.0, 226.0, 227.0, 228.0, 229.0, 230.0, 231.0, 232.0, 233.0, 234.0, 235.0, 236.0, 237.0, 238.0, 239.0, 240.0, 241.0, 242.0, 243.0, 244.0, 245.0, 246.0, 247.0, 248.0, 249.0, 250.0, 251.0, 252.0, 253.0, 254.0, 255.0, 256.0, 257.0, 258.0, 259.0, 260.0, 261.0, 262.0, 263.0, 264.0, 265.0, 266.0, 267.0, 268.0, 269.0, 270.0, 271.0, 272.0, 273.0, 274.0, 275.0, 276.0, 277.0, 278.0, 279.0, 280.0, 282.0, 283.0, 284.0, 285.0, 286.0, 288.0, 289.0, 290.0, 291.0, 292.0, 294.0, 296.0, 297.0, 298.0, 300.0};

float get_celsius(int Pin_thermistor_num){
  
  int Voltage = temp_V[0]; //電圧がtemp_V[0] → 超高温状態
  Voltage = analogRead(Pin_thermistor); //0~3.3Vを0~4095に分割した値
  //Serial.print("Vol: ");
  //Serial.print(Voltage);
  //Serial.print(", ");

  /*
  //二分探索法
  while (left <= right) {
    mid = (left + right) / 2;

    if (temp_V[mid] > Voltage) {
        left = mid + 1;
    } 
    else {
        right = mid - 1;
    }
  }
  
  under_temp_V = temp_V[left];
  over_temp_V = temp_V[left + 1];
  // left: Voltage以下となる最小インデックス
  // 候補は temp_V[left - 1]（大きい側）と temp_V[left]（小さい側）
  if (left == 0) {
    num_temp_V = 0; // すべての値よりVoltageが大きい（超高温）
  } else if (left >= 1351) {
    num_temp_V = 1350; // すべての値よりVoltageが小さい（超低温）
  } else {
    int diff_under = abs(temp_V[left] - Voltage);
    int diff_over = abs(temp_V[left + 1] - Voltage);
    num_temp_V = (diff_under < diff_over) ? left : (left + 1);
  }
  */
  
  //前から配列を検索してunder_temp,over_temp決定
  for(i = 0; i < 1495; i++){
    if(temp_V[i] <= Voltage){
      break;
    }
  }
  under_temp_V = temp_V[i--];
  over_temp_V = temp_V[i];
  //最近似値に相当する配列の番号を決定
  if((under_temp_V - Voltage) > (Voltage - over_temp_V)){
    num_temp_V = i;
  }
  else{
    num_temp_V = i--;
  }
  

  //平滑化していない温度をリターン
  
  return temp[num_temp_V];

}


//指数移動平均
float get_smoothed_celsius(float celsius, float smoothed_celsius){
  const float alpha = 0.3; // スムージング係数（0 < α < 1）
  //Serial.print("前平滑温度:");
  //Serial.print(smoothed_celsius);
  //Serial.print(", ");
  return (alpha * celsius + (1 - alpha) * smoothed_celsius);
}



/********************* ただの制御 ******************/
//void control_SSR(){





//}



//PWM制御
/*
float get_PWM_OUT_V(float smoothed_celsius){
  analogWriteFreq(25000);
  analogWriteRange(256);
  duty = map(smoothed_celsius, -15, 50, 0, 255);
  duty = constrain(duty, 0, 255);
  analogWrite(Pin_Peltier, duty); //dutyは0~255の値
  return (3.3*((float)duty / 255));
}
*/



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





void setup(){
  analogReadResolution(12);
  Serial.begin(115200);
  pinMode(Pin_thermistor, INPUT);
  pinMode(SSR, OUTPUT);
}


void loop(){
  Pin_thermistor_num = Pin_thermistor; //温度を読みたいサーミスタのピン番号を代入，サーミスターを何個か並列使用するときのためにPin_thermistor_numに代入するようにした
  celsius = get_celsius(Pin_thermistor_num);
  smoothed_celsius = get_smoothed_celsius(celsius, smoothed_celsius);
  




  //PWM_OUT_V = get_PWM_OUT_V(smoothed_celsius);

  //Serial.print("直温度:");
  //Serial.println(celsius);
  //Serial.print(", ");
  //Serial.print("平滑温度：");
  //Serial.print(smoothed_celsius); // 平滑化された温度の出力
  //Serial.print("℃, ");
  //Serial.print("PWM出力電圧:");
  //Serial.print(PWM_OUT_V);
  //Serial.println("V");


  //PID制御
  /*
  diff_temp_before = target_temp - celsius; //現在直温度と目標温度の差

  diff_temp = target_temp - smoothed_celsius;
  I_diff_temp = I_diff_temp + (diff_temp + diff_temp_before)*T/2;
  D_diff_temp = (diff_temp - diff_temp_before)/T;
  u = P*diff_temp + I*I_diff_temp + D*D_diff_temp;
  diff_temp_before = diff_temp;
  Serial.print(diff_temp);
  Serial.print(", ");
  Serial.print(diff_temp_before);
  Serial.print(", ");
  Serial.print("PID:");
  Serial.println(u);
  */





  //PIDからPWM
  /*
  u = constrain(u, -15, 50);
  if()  
  

  */

  /********************グラフプロット & SD保存用********************/
  if(check_reflow_start == true){
    time_now_loop = millis();
    time_start_to_now_loop = time_now_loop - time_start;
    data_time_loop = time_start_to_now_loop / 1000;
    data_time[num_loop] = data_time_loop;
    plot[num_loop] = smoothed_celsius;
    
  }

  num_loop++;
  //Serial.print(time_start_to_now_loop);
  //Serial.print(", ");
  //Serial.print(data_time_loop);
  delay(1000);
}




void setup1(){
  Serial.begin(115200); 
  pinMode(SSR, OUTPUT);

  //SPI0
  SPI.setTX(TFT_TOUCH_SD_MOSI);
  SPI.setSCK(TFT_TOUCH_SD_SCK);
  SPI.setRX(TOUCH_SD_MISO);

  //グラフィック設定
  tft.begin();
  tft.setRotation(3);                         //画面回転（0~3）
  canvas.fillScreen(ILI9341_BLACK);   //背景色リセット
  canvas1.fillScreen(ILI9341_BLACK);
  tft.setTextSize(1);                      //テキストサイズ

  //タッチパネル設定
  ts.begin();                   // タッチパネル初期化
  ts.setRotation(1); // タッチパネルの回転(画面回転が3ならここは1)

  // SDカードの初期化
  if (!SD.begin(SD_CS)) {
    Serial.println("SDカードの初期化に失敗しました");
    drawText(50, 50, "SD card not found!", &FreeSans12pt7b, ILI9341_RED);
    return;
  } else {
    Serial.println("SDカードが初期化されました");
  }


}

void loop1(){
  switch(page){
    case 1:
      check_reflow_start = false;
      SSR_ON = false;
      page_1();
      break;
    case 2:
      page_2(smoothed_celsius);
      break;
    case 3:
      delay(200);
      page_3(smoothed_celsius);
      break;
    case 4:
      delay(200);
      page_4();
      break;
    case 5:
      delay(200);
      page_5();
      break;
    case 6:
      delay(200);
      page_6(smoothed_celsius);
      break;
    case 7:
      for(l; l < 1; l++){
        tone(sound,3000,700);
        time_start = millis();
        num_loop = 0;
        check_reflow_start = true;
      }
      delay(200);
      page_7(smoothed_celsius);
      break;
    case 8:
      delay(200);
      page_2(smoothed_celsius);
      break;
    case 9:
      delay(200);
      
      page_2(smoothed_celsius);
      break;
    case 10:
      delay(200);
      page_2(smoothed_celsius);
      break;
    case 11:
      delay(200);
      page_11();
      break;
    case 12:
      delay(200);
      page_12();
      break;

    default:
      Serial.print("Page number error!!");
      page = 2;
      break;

  }

}
