#ifndef WEATHER_ICONS_H
#define WEATHER_ICONS_H

#include <lvgl.h>

// ==========================================
// 1. 宣告所有圖像資源 (LV_IMG_DECLARE)
// ==========================================

// 熱帶氣旋警告 (Tropical Cyclones)
LV_IMG_DECLARE(tc1);   // 1號戒備信號
LV_IMG_DECLARE(tc3);   // 3號強風信號
LV_IMG_DECLARE(tc8b);  // 8號東北烈風或暴風信號
LV_IMG_DECLARE(tc8c);  // 8號西北烈風或暴風信號
LV_IMG_DECLARE(tc8e);  // 8號東南烈風或暴風信號
LV_IMG_DECLARE(tc8d);  // 8號西南烈風或暴風信號
LV_IMG_DECLARE(tc9);   // 9號烈風或暴風風力增強信號
LV_IMG_DECLARE(tc10);  // 10號颶風信號

// 暴雨警告 (Rainstorm Warnings)
LV_IMG_DECLARE(raina); // 黃色暴雨警告
LV_IMG_DECLARE(rainr); // 紅色暴雨警告
LV_IMG_DECLARE(rainb); // 黑色暴雨警告

// 特別天氣警告 (Special Weather Warnings)
LV_IMG_DECLARE(vhot);  // 酷熱天氣警告
LV_IMG_DECLARE(ts);    // 雷暴警告


// ==========================================
// 2. 自動匹配工具 Function (Helper Functions)
// ==========================================

/**
 * 根據熱帶氣旋信號代碼取得圖片
 * @param signal 風球號碼 (1, 3, 8, 9, 10)
 * @param direction 8號風球方向 (例: "NE", "NW", "SE", "SW")，預設為 "NE"
 */
inline const lv_img_dsc_t* get_tc_icon(int signal, const char* direction = "NE") {
    switch (signal) {
        case 1:  return &tc1;
        case 3:  return &tc3;
        case 8:
            if (strcmp(direction, "NW") == 0) return &tc8c;
            if (strcmp(direction, "SE") == 0) return &tc8e;
            if (strcmp(direction, "SW") == 0) return &tc8d;
            return &tc8b; // 預設 NE 東北
        case 9:  return &tc9;
        case 10: return &tc10;
        default: return NULL;
    }
}

/**
 * 根據暴雨警告顏色取得圖片
 * @param color "AMBER" / "YELLOW", "RED", "BLACK"
 */
inline const lv_img_dsc_t* get_rainstorm_icon(const char* color) {
    if (strcasecmp(color, "YELLOW") == 0 || strcasecmp(color, "AMBER") == 0 || strcasecmp(color, "A") == 0) return &raina;
    if (strcasecmp(color, "RED") == 0 || strcasecmp(color, "R") == 0) return &rainr;
    if (strcasecmp(color, "BLACK") == 0 || strcasecmp(color, "B") == 0) return &rainb;
    return NULL;
}

#endif // WEATHER_ICONS_H