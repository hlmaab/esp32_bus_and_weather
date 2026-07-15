#define LV_LVGL_H_INCLUDE_SIMPLE 1

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h> 
#include "time.h"
#include <esp_display_panel.hpp>
#include <lvgl.h>
#include "secrets.h"

// 宣告放進資料夾的自訂中文控制字型
LV_FONT_DECLARE(my_font_chinese_26);
LV_FONT_DECLARE(my_font_chinese_48);
LV_FONT_DECLARE(my_font_chinese_16);
LV_FONT_DECLARE(noto_emoji_18);

extern "C" {
#include "lvgl_v8_port.h"
}

using namespace esp_panel::drivers;
using namespace esp_panel::board;

// ==================== Wi-Fi 設定 ====================
const char* ssid     = SECRET_SSID;     // 👈 替換成 secrets.h 裡的變數
const char* password = SECRET_PASSWORD; // 👈 替換成 secrets.h 裡的變數

const char* ntpServer = "stdtime.gov.hk";
const long  gmtOffset_sec = 28800; // GMT+8
const int   daylightOffset_sec = 0;
// ==================== 🚗 隧道行車時間設定 ====================
const char* TUNNEL_START_ID = "N09";   // 👈 起點 ID (N09 = 將軍澳收費廣場)
const char* TUNNEL_DEST_ID  = "TKOT";  // 👈 終點 ID (TKOT = 啟田道迴旋處/觀塘)
// ==========================================================

struct BusConfig {
    const char* route;
    const char* dest;
    const char* stop_id;
    int seq;
    int mins[3];          // 儲存第 1, 2, 3 班車的分鐘數
    lv_obj_t* lbl_mins;   // 第 1 班車
    lv_obj_t* lbl_next2;  // 第 2 班車
    lv_obj_t* lbl_next3;  // 第 3 班車
};
// ==================== 🚗 巴士設定 ====================
BusConfig buses[3] = {
  { "296A", "往牛頭角站(循環線)", "403881982F9E7209", 1, {-1, -1, -1}, nullptr, nullptr, nullptr },
  { "296C", "往長沙灣(海盈邨)", "5527FF8CC85CF139", 1, {-1, -1, -1}, nullptr, nullptr, nullptr },
  { "296D", "往九龍站",         "21E3E95EAEB2048C", 1, {-1, -1, -1}, nullptr, nullptr, nullptr }
};
// ==========================================================
struct ForecastUI {
    lv_obj_t *lbl_date;    // 日期 (如 07/12)
    lv_obj_t *lbl_icon;    // 天氣大圖示 (如 \u2600/🌧️)
    lv_obj_t *lbl_temp;    // 溫度 (如 29-34°C)
    lv_obj_t *lbl_psr;     // PSR 降雨概率 (如 ☔低)
};

ForecastUI forecast_cols[3];

lv_obj_t *lbl_time;
lv_obj_t *lbl_date;

// ===== ✨ 新增今日即時天氣正方形框 UI 物件 =====
lv_obj_t *today_weather_box;
lv_obj_t *lbl_today_icon;
lv_obj_t *lbl_today_temp;
lv_obj_t *lbl_today_psr;

// ===== 將軍澳隧道行車時間 UI 物件 =====
lv_obj_t *lbl_tunnel_title;
lv_obj_t *lbl_tunnel_time;
lv_obj_t *obj_tunnel_status; // 用作顏色指示燈

// 獲取指定九巴路線與站點的下班車剩餘分鐘數
// 獲取指定九巴路線與站點的下班車剩餘分鐘數（專屬過濾：只顯示尚德出發去程，不含回程）
bool get_kmb_all_eta(const char* route, const char* stop_id, int target_seq, int* mins_out) {
    if (WiFi.status() != WL_CONNECTED) return false; 

    // 初始化快顯陣列為 -999 (代表尚未有資料)
    mins_out[0] = -999; mins_out[1] = -999; mins_out[2] = -999;

    NetworkClientSecure client;
    client.setInsecure(); 
    
    HTTPClient http;
    // 注意：結尾的 /1 是去程(Outbound)方向，但保險起見我們在 JSON 內層還會再做一次 dir 與 seq 的雙重驗證
    String url = "https://data.etabus.gov.hk/v1/transport/kmb/eta/" + String(stop_id) + "/" + String(route) + "/1";
    
    http.begin(client, url);
    http.setTimeout(4000); 
    
    int httpCode = http.GET();
    bool success = false;

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc; 
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error && doc["data"].is<JsonArray>()) {
            JsonArray dataArr = doc["data"].as<JsonArray>();
            success = true;
            
            int eta_count = 0; // 🚀 用來記錄成功抓到幾班「尚德出發」的去程車
            
            for (JsonObject bus_data : dataArr) {
                const char* current_dir = bus_data["dir"];
                int current_seq = bus_data["seq"];
                
                // 🌟 核心過濾 1：必須是去程 (O) 且必須是第 target_seq 站 (e.g.尚德總站出發)
                if (current_dir != nullptr && strcmp (current_dir, "O" ) == 0 && current_seq == target_seq) {                    
                    // 🌟 核心過濾 2：如果該班次沒有 eta 時間（例如最後班次已過，eta 為 null），直接跳過
                    if (bus_data["eta"].isNull() || !bus_data.containsKey("eta")) {
                        continue; 
                    }

                    const char* eta_time_str = bus_data["eta"]; 
                    if (eta_time_str == nullptr || strlen(eta_time_str) < 19) {
                        continue; 
                    }

                    struct tm eta_tm = {0}; 
                    int year, month, day, hour, minute, second;
                    if (sscanf(eta_time_str, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) == 6) {
                        eta_tm.tm_year = year - 1900;
                        eta_tm.tm_mon = month - 1;
                        eta_tm.tm_mday = day;
                        eta_tm.tm_hour = hour;
                        eta_tm.tm_min = minute;
                        eta_tm.tm_sec = second;
                        eta_tm.tm_isdst = -1; 

                        time_t eta_epoch = mktime(&eta_tm);
                        time_t now_epoch;
                        
                        if (time(&now_epoch) != -1 && now_epoch > 1000000) {
                            if (eta_epoch != -1) {
                                double diff_secs = difftime(eta_epoch, now_epoch);
                                int remaining = diff_secs / 60;
                                
                                // 將計算好的分鐘數塞入 mins_out 的對應位置
                                mins_out[eta_count] = (remaining < 0) ? 0 : remaining;
                            }
                        } else {
                            mins_out[eta_count] = -2; // 時間同步錯誤
                        }
                        
                        eta_count++;
                        // 🚀 抓滿尚德出發的前 3 班車，就可以直接跳出迴圈了
                        if (eta_count >= 3) {
                            break;
                        }
                    }
                }
            }
        }
    }
    http.end();
    client.stop(); 
    return success;
}
// ===== 精準解析將軍澳隧道行車時間 =====
int get_tunnel_time(const char* start_id, const char* dest_id) {
    if (WiFi.status() != WL_CONNECTED) return -1;

    NetworkClientSecure client;
    client.setInsecure(); 
    
    HTTPClient http;
    String url = "https://resource.data.one.gov.hk/td/jss/Journeytimev2.xml"; 
    
    http.begin(client, url);
    http.setTimeout(5000); // 檔案較大，超時稍微放寬到 5 秒
    
    int httpCode = http.GET();
    int tko_time = -999;

    if (httpCode == HTTP_CODE_OK) {
        // 改用 Stream 串流讀取，避免 getString() 截斷 27KB 的大檔案
        Stream* stream = http.getStreamPtr();

        String start_tag = "<LOCATION_ID>" + String(start_id) + "</LOCATION_ID>";
        String dest_tag = "<DESTINATION_ID>" + String(dest_id) + "</DESTINATION_ID>";

        // 尋找將軍澳隧道專屬的關鍵特徵字串
        // 核心邏輯：先找到起點 N09，再確認終點 TKOT，隨後抓取 JOURNEY_DATA
        if (stream->find((char*)start_tag.c_str())) {
            // 在 N09 後面繼續找終點
            if (stream->find((char*)dest_tag.c_str())) {
                // 找到了該區塊，接著撈時間
                if (stream->find("<JOURNEY_DATA>")) {
                    // 讀取接下來的數值，直到遇到 </
                    String timeStr = stream->readStringUntil('<');
                    timeStr.trim();
                    if (timeStr.length() > 0) {
                        tko_time = timeStr.toInt();
                        Serial.print("【成功動態解析】將軍澳隧道時間: ");
                        Serial.print(tko_time);
                        Serial.println(" 分鐘");
                    }
                }
            }
        }
    } else {
        Serial.print("【錯誤】隧道 API 連線失敗，代碼: ");
        Serial.println(httpCode);
    }
    
    http.end();
    client.stop(); 
    
    // 如果解析失敗，啟動防呆
    if (tko_time == -999 || tko_time <= 0) {
        Serial.println("【警告】隧道時間解析失敗或被截斷，啟動防呆回傳 0");
        tko_time = 0; 
    }
    return tko_time;
}

void create_bus_ui()
{
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_make(15, 24, 42), 0);

    lv_obj_t *main_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(main_cont, 800, 480); 
    lv_obj_clear_flag(main_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(main_cont, 0, 0);
    lv_obj_set_style_border_width(main_cont, 0, 0);
    lv_obj_set_style_pad_all(main_cont, 0, 0);

    lv_obj_t *left_side = lv_obj_create(main_cont);
    lv_obj_set_size(left_side, 360, 480); 
    lv_obj_set_style_radius(left_side, 0, 0);
    lv_obj_set_style_border_width(left_side, 0, 0);
    lv_obj_set_style_pad_all(left_side, 20, 0);
    lv_obj_set_style_bg_color(left_side, lv_color_make(15, 24, 42), 0); 
    lv_obj_set_style_bg_opa(left_side, LV_OPA_COVER, 0);
    lv_obj_clear_flag(left_side, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(left_side, 0, 0); 

    // 時間面板容器
    lv_obj_t *time_panel = lv_obj_create(left_side);
    lv_obj_set_size(time_panel, 320, 285); 
    lv_obj_set_style_bg_color(time_panel, lv_color_make(30, 41, 59), 0); 
    lv_obj_set_style_bg_opa(time_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(time_panel, 12, 0); 
    lv_obj_set_style_border_width(time_panel, 1, 0);
    lv_obj_set_style_border_color(time_panel, lv_color_make(71, 85, 105), 0); 
    lv_obj_align(time_panel, LV_ALIGN_TOP_MID, 0, 15); 
    lv_obj_clear_flag(time_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(time_panel, 12, 0); // 加上內邊距方便排版

    // ✨ 第一行：日期放左上
    lbl_date = lv_label_create(time_panel);
    lv_label_set_text(lbl_date, "----.--.-- (---)");
    lv_obj_set_style_text_font(lbl_date, &my_font_chinese_16, 0); // 使用16號中文字型顯得精緻
    lv_obj_set_style_text_color(lbl_date, lv_color_make(148, 163, 184), 0); 
    lv_obj_align(lbl_date, LV_ALIGN_TOP_LEFT, 5, 2); 

    // ✨ 第二行分左右 - 左邊顯示時間
    lbl_time = lv_label_create(time_panel);
    lv_label_set_text(lbl_time, "--:--");
    lv_obj_set_style_text_font(lbl_time, &my_font_chinese_48, 0); // 時間放大增加氣勢
    lv_obj_set_style_text_color(lbl_time, lv_color_white(), 0);
    lv_obj_align(lbl_time, LV_ALIGN_TOP_LEFT, 0, 28); 

    // ✨ 第二行分左右 - 右邊建立今日天氣的正方形框
    today_weather_box = lv_obj_create(time_panel);
    lv_obj_set_size(today_weather_box, 135, 72); // 正方形/微長方框貼合右側
    lv_obj_set_style_bg_color(today_weather_box, lv_color_make(38, 50, 68), 0);
    lv_obj_set_style_bg_opa(today_weather_box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(today_weather_box, 8, 0);
    lv_obj_set_style_border_width(today_weather_box, 1, 0);
    lv_obj_set_style_border_color(today_weather_box, lv_color_make(71, 85, 105), 0);
    lv_obj_align(today_weather_box, LV_ALIGN_TOP_RIGHT, -2, 18);
    lv_obj_clear_flag(today_weather_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(today_weather_box, 4, 0);

    // 正方形框內部分為左右：左邊放天氣 Icon
    lbl_today_icon = lv_label_create(today_weather_box);
    lv_label_set_text(lbl_today_icon, "☁️");
    lv_obj_set_style_text_font(lbl_today_icon, &noto_emoji_18, 0); // 大圖示
    lv_obj_set_style_text_color(lbl_today_icon, lv_color_white(), 0);
    lv_obj_align(lbl_today_icon, LV_ALIGN_LEFT_MID, 4, 0);

    // 正方形框內右邊的「上面」放溫度範圍 %d-%d°C
    lbl_today_temp = lv_label_create(today_weather_box);
    lv_label_set_text(lbl_today_temp, "---°C");
    lv_obj_set_style_text_font(lbl_today_temp, &my_font_chinese_16, 0);
    lv_obj_set_style_text_color(lbl_today_temp, lv_color_white(), 0);
    lv_obj_align(lbl_today_temp, LV_ALIGN_TOP_RIGHT, -4, 6);

    // 正方形框內右邊的「下面」放降雨率 psr value ☔%s
    lbl_today_psr = lv_label_create(today_weather_box);
    lv_label_set_text(lbl_today_psr, "☔--");
    lv_obj_set_style_text_font(lbl_today_psr, &noto_emoji_18, 0);
    lv_obj_set_style_text_color(lbl_today_psr, lv_color_make(148, 163, 184), 0);
    lv_obj_align(lbl_today_psr, LV_ALIGN_BOTTOM_RIGHT, -4, -6);

    // 分隔線
    lv_obj_t * line = lv_obj_create(time_panel);
    lv_obj_set_size(line, 290, 1);
    lv_obj_set_style_bg_color(line, lv_color_make(71, 85, 105), 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 100);

    // 下方預報行容器（改為未來的 3 天預報）
    lv_obj_t *weather_row_cont = lv_obj_create(time_panel);
    lv_obj_set_size(weather_row_cont, 295, 145); 
    lv_obj_set_style_bg_opa(weather_row_cont, LV_OPA_TRANSP, 0); 
    lv_obj_set_style_border_width(weather_row_cont, 0, 0);       
    lv_obj_set_style_pad_all(weather_row_cont, 0, 0);            
    lv_obj_clear_flag(weather_row_cont, LV_OBJ_FLAG_SCROLLABLE);  
    lv_obj_align(weather_row_cont, LV_ALIGN_TOP_MID, 0, 112);    

    lv_obj_set_layout(weather_row_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(weather_row_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(weather_row_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    int col_width = 86; 
    for(int i = 0; i < 3; i++) {
        lv_obj_t *col_cont = lv_obj_create(weather_row_cont); 
        lv_obj_set_size(col_cont, col_width, 135); 
        
        lv_obj_set_style_bg_color(col_cont, lv_color_make(38, 50, 68), 0);
        lv_obj_set_style_bg_opa(col_cont, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(col_cont, 8, 0);
        lv_obj_set_style_border_width(col_cont, 1, 0);
        lv_obj_set_style_border_color(col_cont, lv_color_make(71, 85, 105), 0);
        
        lv_obj_set_style_pad_top(col_cont, 6, 0);
        lv_obj_set_style_pad_bottom(col_cont, 6, 0);
        lv_obj_set_style_pad_left(col_cont, 0, 0);
        lv_obj_set_style_pad_right(col_cont, 0, 0);
        lv_obj_clear_flag(col_cont, LV_OBJ_FLAG_SCROLLABLE);
        
        lv_obj_set_layout(col_cont, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(col_cont, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(col_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        forecast_cols[i].lbl_date = lv_label_create(col_cont);
        lv_label_set_text(forecast_cols[i].lbl_date, "--/--");
        lv_obj_set_style_text_font(forecast_cols[i].lbl_date, &my_font_chinese_16, 0);
        lv_obj_set_style_text_color(forecast_cols[i].lbl_date, lv_color_make(148, 163, 184), 0);

        forecast_cols[i].lbl_icon = lv_label_create(col_cont);
        lv_label_set_text(forecast_cols[i].lbl_icon, "☁️");
        lv_obj_set_style_text_font(forecast_cols[i].lbl_icon, &noto_emoji_18, 0);
        lv_obj_set_style_text_color(forecast_cols[i].lbl_icon, lv_color_white(), 0);

        forecast_cols[i].lbl_temp = lv_label_create(col_cont);
        lv_label_set_text(forecast_cols[i].lbl_temp, "--°C");
        lv_obj_set_style_text_font(forecast_cols[i].lbl_temp, &my_font_chinese_16, 0);
        lv_obj_set_style_text_color(forecast_cols[i].lbl_temp, lv_color_white(), 0);

        forecast_cols[i].lbl_psr = lv_label_create(col_cont);
        lv_label_set_text(forecast_cols[i].lbl_psr, "☔-");
        lv_obj_set_style_text_font(forecast_cols[i].lbl_psr, &noto_emoji_18, 0);
    }
    
    // 隧道面板
    lv_obj_t *tunnel_panel = lv_obj_create(left_side);
    lv_obj_set_size(tunnel_panel, 320, 110); 
    lv_obj_set_style_bg_color(tunnel_panel, lv_color_make(30, 41, 59), 0);
    lv_obj_set_style_bg_opa(tunnel_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tunnel_panel, 12, 0);
    lv_obj_set_style_border_width(tunnel_panel, 1, 0);
    lv_obj_set_style_border_color(tunnel_panel, lv_color_make(71, 85, 105), 0);
    lv_obj_align(tunnel_panel, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_clear_flag(tunnel_panel, LV_OBJ_FLAG_SCROLLABLE);

    obj_tunnel_status = lv_obj_create(tunnel_panel);
    lv_obj_set_size(obj_tunnel_status, 14, 14);
    lv_obj_set_style_radius(obj_tunnel_status, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(obj_tunnel_status, lv_color_make(34, 197, 94), 0); 
    lv_obj_set_style_bg_opa(obj_tunnel_status, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj_tunnel_status, 0, 0);
    lv_obj_align(obj_tunnel_status, LV_ALIGN_LEFT_MID, 12, -18);

    lbl_tunnel_title = lv_label_create(tunnel_panel);
    lv_label_set_text(lbl_tunnel_title, "將軍澳隧道 (往觀塘)");
    lv_obj_set_style_text_font(lbl_tunnel_title, &my_font_chinese_26, 0);
    lv_obj_set_style_text_color(lbl_tunnel_title, lv_color_white(), 0);
    lv_obj_align(lbl_tunnel_title, LV_ALIGN_LEFT_MID, 35, -18);

    lbl_tunnel_time = lv_label_create(tunnel_panel);
    lv_label_set_text(lbl_tunnel_time, "-- 分鐘");
    lv_obj_set_style_text_font(lbl_tunnel_time, &my_font_chinese_26, 0);
    lv_obj_set_style_text_color(lbl_tunnel_time, lv_color_make(250, 204, 21), 0); 
    lv_obj_align(lbl_tunnel_time, LV_ALIGN_LEFT_MID, 35, 18);

    // 右側巴士列表部分不變
    lv_obj_t *right_side = lv_obj_create(main_cont);
    lv_obj_set_size(right_side, 440, 480); 
    lv_obj_set_style_radius(right_side, 0, 0);
    lv_obj_set_style_border_width(right_side, 0, 0);
    lv_obj_set_style_pad_all(right_side, 0, 0); 
    lv_obj_set_style_bg_color(right_side, lv_color_make(45, 50, 55), 0); 
    lv_obj_set_style_bg_opa(right_side, LV_OPA_COVER, 0);
    lv_obj_clear_flag(right_side, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(right_side, 360, 0); 

    lv_obj_t *header = lv_obj_create(right_side);
    lv_obj_set_size(header, 440, 45); 
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_make(220, 38, 38), 0); 
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(header, 0, 0); 
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *lbl_header_next = lv_label_create(header);
    lv_label_set_text(lbl_header_next, "下班"); 
    lv_obj_set_style_text_font(lbl_header_next, &my_font_chinese_26, 0);
    lv_obj_set_style_text_color(lbl_header_next, lv_color_white(), 0);
    lv_obj_align(lbl_header_next, LV_ALIGN_LEFT_MID, 160, 3); 

    lv_obj_t *lbl_header = lv_label_create(header);
    lv_label_set_text(lbl_header, "即將到達");
    lv_obj_set_style_text_font(lbl_header, &my_font_chinese_26, 0);
    lv_obj_set_style_text_color(lbl_header, lv_color_white(), 0);
    lv_obj_align(lbl_header, LV_ALIGN_RIGHT_MID, -20, 3);

    int start_y = 55; 
    for(int i = 0; i < 3; i++) {
        lv_obj_t *row = lv_obj_create(right_side);
        lv_obj_set_size(row, 440, 130); 
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(row, lv_color_make(30, 35, 40), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0); 
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, start_y + (i * 135)); 
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(row, 0, 0); 

        lv_obj_t *lbl_route = lv_label_create(row);
        lv_label_set_text(lbl_route, buses[i].route);
        lv_obj_set_style_text_font(lbl_route, &my_font_chinese_26, 0); 
        lv_obj_set_style_text_color(lbl_route, lv_color_white(), 0); 
        lv_obj_align(lbl_route, LV_ALIGN_TOP_LEFT, 15, 15);

        lv_obj_t *lbl_dest = lv_label_create(row);
        lv_label_set_text(lbl_dest, buses[i].dest);
        lv_obj_set_style_text_font(lbl_dest, &my_font_chinese_26, 0); 
        lv_obj_set_style_text_color(lbl_dest, lv_color_make(180, 185, 190), 0); 
        lv_obj_align(lbl_dest, LV_ALIGN_TOP_LEFT, 15, 80); 

        buses[i].lbl_next2 = lv_label_create(row);
        lv_label_set_text(buses[i].lbl_next2, "-- Min.");
        lv_obj_set_style_text_font(buses[i].lbl_next2, &my_font_chinese_26, 0);
        lv_obj_set_style_text_color(buses[i].lbl_next2, lv_color_make(220, 225, 230), 0);
        lv_obj_align(buses[i].lbl_next2, LV_ALIGN_TOP_LEFT, 160, 15);

        buses[i].lbl_next3 = lv_label_create(row);
        lv_label_set_text(buses[i].lbl_next3, "-- Min.");
        lv_obj_set_style_text_font(buses[i].lbl_next3, &my_font_chinese_26, 0);
        lv_obj_set_style_text_color(buses[i].lbl_next3, lv_color_make(160, 165, 170), 0);
        lv_obj_align(buses[i].lbl_next3, LV_ALIGN_TOP_LEFT, 160, 48);

        buses[i].lbl_mins = lv_label_create(row);
        lv_label_set_text(buses[i].lbl_mins, "--");
        lv_obj_set_style_text_font(buses[i].lbl_mins, &my_font_chinese_26, 0);
        lv_obj_set_style_text_color(buses[i].lbl_mins, lv_color_make(250, 204, 21), 0);
        lv_obj_align(buses[i].lbl_mins, LV_ALIGN_RIGHT_MID, -50, 0);
    }
}

void update_bus_and_tunnel_data()
{
    Serial.println("正在更新巴士 ETA 與將軍澳隧道行車時間...");
    
    for(int i = 0; i < 3; i++) {
        int current_mins[3] = {-999, -999, -999};
        
        // 核心改變：一次聯網直接拿回該路線的 3 班車資料！
        bool ok = get_kmb_all_eta (buses[i].route, buses[i].stop_id, buses[i].seq, current_mins);

        // 把撈到的時間賦值回結構體與本地變數
        buses[i].mins[0] = current_mins[0];
        buses[i].mins[1] = current_mins[1];
        buses[i].mins[2] = current_mins[2];

        int m1 = current_mins[0];
        int m2 = current_mins[1];
        int m3 = current_mins[2];

        lvgl_port_lock(-1);
        
        // 顯示第 1 班車的 UI 邏輯
        if(!ok || m1 == -1 || m1 == -2) {
            lv_obj_set_style_text_font(buses[i].lbl_mins, &my_font_chinese_26, 0); 
            lv_label_set_text(buses[i].lbl_mins, "err");
            lv_obj_set_style_text_color(buses[i].lbl_mins, lv_color_make(156, 163, 175), 0);
            lv_obj_align(buses[i].lbl_mins, LV_ALIGN_RIGHT_MID, -50, 0);
        } else if(m1 == -999) {
            lv_obj_set_style_text_font(buses[i].lbl_mins, &my_font_chinese_26, 0);
            lv_label_set_text(buses[i].lbl_mins, "--"); 
            lv_obj_set_style_text_color(buses[i].lbl_mins, lv_color_make(156, 163, 175), 0);
            lv_obj_align(buses[i].lbl_mins, LV_ALIGN_RIGHT_MID, -50, 0);
        } else if(m1 == 0) {
            lv_obj_set_style_text_font(buses[i].lbl_mins, &my_font_chinese_26, 0); 
            lv_label_set_text(buses[i].lbl_mins, "已到站");
            lv_obj_set_style_text_color(buses[i].lbl_mins, lv_color_make(220, 38, 38), 0); 
            lv_obj_align(buses[i].lbl_mins, LV_ALIGN_RIGHT_MID, -50, 0); 
        } else if(m1 > 0 && m1 < 120) { 
            lv_obj_set_style_text_font(buses[i].lbl_mins, &my_font_chinese_48, 0);
            lv_label_set_text_fmt(buses[i].lbl_mins, "%d", m1);
            lv_obj_align(buses[i].lbl_mins, LV_ALIGN_RIGHT_MID, -50, 0);
            if(m1 < 10) {
                lv_obj_set_style_text_color(buses[i].lbl_mins, lv_color_make(220, 38, 38), 0); 
            } else {
                lv_obj_set_style_text_color(buses[i].lbl_mins, lv_color_make(250, 204, 21), 0); 
            }
        }

        // 顯示第 2 班車的 UI 邏輯
        if(m2 >= 0 && m2 < 120) { 
            lv_label_set_text_fmt(buses[i].lbl_next2, "%d Min.", m2);
        } else {
            lv_label_set_text(buses[i].lbl_next2, "-- Min."); 
        }

        // 顯示第 3 班車的 UI 邏輯
        if(m3 >= 0 && m3 < 120) {
            lv_label_set_text_fmt(buses[i].lbl_next3, "%d Min.", m3);
        } else {
            lv_label_set_text(buses[i].lbl_next3, "-- Min.");
        }
        
        lvgl_port_unlock();
        
    }

    // 更新隧道時間
    int tunnel_mins = get_tunnel_time(TUNNEL_START_ID, TUNNEL_DEST_ID);
    lvgl_port_lock(-1);
    if (tunnel_mins > 0) {
        char tunnel_buf[32];
        sprintf(tunnel_buf, "%d 分鐘", tunnel_mins);
        lv_label_set_text(lbl_tunnel_time, tunnel_buf);

        if (tunnel_mins <= 7) {
            lv_obj_set_style_bg_color(obj_tunnel_status, lv_color_make(34, 197, 94), 0); 
        } else if (tunnel_mins > 7 && tunnel_mins <= 11) {
            lv_obj_set_style_bg_color(obj_tunnel_status, lv_color_make(234, 179, 8), 0);  
        } else {
            lv_obj_set_style_bg_color(obj_tunnel_status, lv_color_make(239, 68, 68), 0);  
        }
    } else {
        lv_label_set_text(lbl_tunnel_time, "-- 分鐘");
        lv_obj_set_style_bg_color(obj_tunnel_status, lv_color_make(156, 163, 175), 0); 
    }
    lvgl_port_unlock();
}

// 🛠️ 升級天文台解析：直接把 API 預報的第 0 天當作「今日天氣正方形框」的資料來源！
// 🛠️ 修正天文台解析：補上 NetworkClientSecure 避免 protocol 警告與緩衝殘留
void update_weather_data() {
    if (WiFi.status() != WL_CONNECTED) return;

    Serial.println("正在向天文台抓取天氣預報...");
    
    NetworkClientSecure client;
    client.setInsecure(); // 天文台使用 HTTPS，必須搭配 Secure Client
    
    HTTPClient http;
    String url = "https://data.weather.gov.hk/weatherAPI/opendata/weather.php?dataType=fnd&lang=tc";
    
    http.begin(client, url); // 👈 修正：傳入 client
    http.setTimeout(4000); 
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc; 
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            lvgl_port_lock(-1);

            // ------------------ ✨ 1. 解析今日天氣（固定用 index 0） ------------------
            JsonObject today = doc["weatherForecast"][0];
            int today_min = today["forecastMintemp"]["value"];
            int today_max = today["forecastMaxtemp"]["value"];
            const char* today_psr_val = today["PSR"];
            const char* today_desc = today["forecastWeather"];
            const char* today_date_raw = today["forecastDate"]; 

            // 今日圖示
            const char* today_icon = "☁️";
            if (today_desc != nullptr) {
                if (strstr(today_desc, "晴") != NULL) {
                    today_icon = "\u2600"; 
                } else if (strstr(today_desc, "雨") != NULL || strstr(today_desc, "驟雨") != NULL) {
                    today_icon = "🌧️";
                } else if (strstr(today_desc, "雷") != NULL || strstr(today_desc, "雷暴") != NULL) {
                    today_icon = "⛈️"; 
                }
            }
            lv_label_set_text(lbl_today_icon, today_icon);

            // 今日溫度
            char today_temp_buf[16];
            snprintf(today_temp_buf, sizeof(today_temp_buf), "%d-%d°C", today_min, today_max);
            lv_label_set_text(lbl_today_temp, today_temp_buf);

            // 今日降雨率
            char today_psr_buf[16];
            snprintf(today_psr_buf, sizeof(today_psr_buf), "☔%s", (today_psr_val != nullptr) ? today_psr_val : "--");
            lv_label_set_text(lbl_today_psr, today_psr_buf);
            if (today_psr_val != nullptr && (strcmp(today_psr_val, "高") == 0 || strcmp(today_psr_val, "中高") == 0)) {
                lv_obj_set_style_text_color(lbl_today_psr, lv_color_make(239, 68, 68), 0); 
            } else {
                lv_obj_set_style_text_color(lbl_today_psr, lv_color_make(34, 211, 238), 0); 
            }

            // ------------------ 🔄 2. 解析未來 3 天預報 ------------------
            int start_index = 1;
            JsonObject next_day = doc["weatherForecast"][1];
            const char* next_day_date = next_day["forecastDate"];
            
            if (today_date_raw != nullptr && next_day_date != nullptr && strcmp(today_date_raw, next_day_date) == 0) {
                start_index = 2;
            }

            for(int i = 0; i < 3; i++) {
                JsonObject day = doc["weatherForecast"][start_index + i]; 
                
                const char* forecastDate = day["forecastDate"]; 
                int temp_min = day["forecastMintemp"]["value"];
                int temp_max = day["forecastMaxtemp"]["value"];
                const char* psr = day["PSR"]; 
                const char* weather_desc = day["forecastWeather"];

                char date_buf[16];
                if(forecastDate != nullptr && strlen(forecastDate) == 8) {
                    snprintf(date_buf, sizeof(date_buf), "%.2s/%.2s", forecastDate + 4, forecastDate + 6);
                } else {
                    snprintf(date_buf, sizeof(date_buf), "--/--");
                }
                lv_label_set_text(forecast_cols[i].lbl_date, date_buf);

                const char* weather_icon = "☁️"; 
                if (weather_desc != nullptr) {
                    if (strstr(weather_desc, "晴") != NULL) {
                        weather_icon = "\u2600"; 
                    } else if (strstr(weather_desc, "雨") != NULL || strstr(weather_desc, "驟雨") != NULL) {
                        weather_icon = "🌧️";
                    } else if (strstr(weather_desc, "雷") != NULL || strstr(weather_desc, "雷暴") != NULL) {
                        weather_icon = "⛈️"; 
                    }
                }
                lv_label_set_text(forecast_cols[i].lbl_icon, weather_icon);

                char temp_buf[16];
                snprintf(temp_buf, sizeof(temp_buf), "%d-%d°C", temp_min, temp_max);
                lv_label_set_text(forecast_cols[i].lbl_temp, temp_buf);

                char psr_buf[32];
                if (psr != nullptr) {
                    snprintf(psr_buf, sizeof(psr_buf), "☔%s", psr);
                    if (strcmp(psr, "高") == 0 || strcmp(psr, "中高") == 0) {
                        lv_obj_set_style_text_color(forecast_cols[i].lbl_psr, lv_color_make(239, 68, 68), 0); 
                    } else if (strcmp(psr, "中") == 0 || strcmp(psr, "中低") == 0) {
                        lv_obj_set_style_text_color(forecast_cols[i].lbl_psr, lv_color_make(234, 179, 8), 0);  
                    } else {
                        lv_obj_set_style_text_color(forecast_cols[i].lbl_psr, lv_color_make(34, 197, 94), 0);  
                    }
                } else {
                    snprintf(psr_buf, sizeof(psr_buf), "☔--");
                    lv_obj_set_style_text_color(forecast_cols[i].lbl_psr, lv_color_make(156, 163, 175), 0);
                }
                lv_label_set_text(forecast_cols[i].lbl_psr, psr_buf);
            }
            
            lvgl_port_unlock();
        } else {
            Serial.print("JSON 解析錯誤: ");
            Serial.println(error.c_str());
        }
    } else {
        Serial.print("天文台 API 連線失敗，代碼: ");
        Serial.println(httpCode);
    }
    http.end();
    client.stop(); // 👈 修正：釋放 Secure 連線資源
}

void update_real_time()
{
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)) return;

    char timeStr[6];
    sprintf(timeStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

    const char* wd[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    char dateStr[30];
    sprintf(dateStr, "%04d-%02d-%02d (%s)", 
            timeinfo.tm_year + 1900, 
            timeinfo.tm_mon + 1, 
            timeinfo.tm_mday, 
            wd[timeinfo.tm_wday]);

    lvgl_port_lock(-1);
    lv_label_set_text(lbl_time, timeStr);
    lv_label_set_text(lbl_date, dateStr);
    lvgl_port_unlock();
}

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("=== 智能九巴與隧道看板啟動 ===");

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi 已成功連線！");

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    Board *board = new Board();
    board->init();
    auto lcd = board->getLCD();
    
    lcd->configFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);

#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
    auto lcd_bus = lcd->getBus();
    if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
        static_cast<BusRGB *>(lcd_bus)->configRGB_BounceBufferSize(lcd->getFrameWidth() * 20);
    }
#endif

    assert(board->begin());
    lvgl_port_init(board->getLCD(), board->getTouch());

    lvgl_port_lock(-1);
    create_bus_ui();
    lvgl_port_unlock();

    update_real_time();
    update_bus_and_tunnel_data(); 
    update_weather_data(); 
}

void loop()
{
    unsigned long current_millis = millis();
    static unsigned long last_clock_update = 0;
    static unsigned long last_bus_update = 0;
    static unsigned long last_weather_update = 1; 

    if (current_millis - last_clock_update >= 1000) {
        last_clock_update = current_millis;
        update_real_time();
    }
    
    if (current_millis - last_bus_update >= 30000) {
        last_bus_update = current_millis;
        update_bus_and_tunnel_data();
    }
    
    if (current_millis - last_weather_update >= 900000) {
        last_weather_update = current_millis;
        update_weather_data();
    }

    delay(5);
}
