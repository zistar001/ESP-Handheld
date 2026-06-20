#include "liuren_core.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "modules/time_sync/time_sync.h"

/* === 小六壬六神数据 === */
static const char *pos_names[LIUREN_MAX] = {
    "大安", "留连", "速喜", "赤口", "小吉", "空亡"
};

static const char *pos_meanings[LIUREN_MAX] = {
    "大吉", "拖延", "喜事", "争执", "好结果", "大利"
};

static const char *pos_judgments[LIUREN_MAX] = {
    "身未动时，属木青龙，谋事一五七，贵人西南，冲犯东方，小孩婆姐六畜惊，大人青面阴神。断曰：大安事事昌，求财在坤方，失物去不远，宅舍保安康，行人身未动，病者主无妨，将军回田野，仔细更推详。",
    "卒未归时，属水玄武，谋事二八十，贵人南方，冲犯北方，小孩游路亡魂，大人乌面夫人。断曰：留连事难成，求谋日未明，官事宜速决，失者去不远，行者早回心，三分靠运气，七分靠打拼，凡事莫强求，安稳才是真。",
    "人便至时，属火朱雀，谋事三六九，贵人西南，冲犯南方，小孩婆姐动勿惊，大人火箭将军。断曰：速喜喜来临，求财向南行，失物申未午，逢人路上寻，官事有福德，病者无祸侵，田宅六畜吉，行人有信音。",
    "官事凶时，属金白虎，谋事四七十，贵人东方，冲犯西方，小孩迷魂童子，大人金神七煞。断曰：赤口主口舌，官非切要防，失物急去寻，行人有惊慌，鸡犬多作怪，病者出西方，更须防咀咒，恐怕染瘟殃。",
    "人来喜时，属木六合，谋事一五七，贵人西南，冲犯东方，小孩婆姐六畜惊，大人无主家神。断曰：小吉最吉昌，路上好商量，阳人来报喜，失物在坤方，行人立便至，交关真是强，凡事皆和合，病者守无妨。",
    "音信稀时，属土勾陈，谋事三六九，贵人北方，冲犯厝地，小孩土瘟神煞，大人土压夫人。断曰：空亡事不长，阴人多乖张，求财无利益，行人有灾殃，失物寻不见，官事主刑伤，病人逢暗鬼，解禳保安康。"
};

static const char *hour_names[12] = {
    "子时(23-01)", "丑时(01-03)", "寅时(03-05)", "卯时(05-07)",
    "辰时(07-09)", "巳时(09-11)", "午时(11-13)", "未时(13-15)",
    "申时(15-17)", "酉时(17-19)", "戌时(19-21)", "亥时(21-23)"
};

/* 时辰地支序号: 子=0, 丑=1, 寅=2, ... */
static const int hour_index_map[24] = {
    0,0, 1,1, 2,2, 3,3, 4,4, 5,5, 6,6, 7,7, 8,8, 9,9, 10,10, 11,11
};

/* === 公历转农历 === */
/* 农历数据表 2024-2034 (每个int编码一年信息) */
/* 编码: bit0-3=闰月(0=无闰), bit4-15=每月大小月(1=30天,0=29天), bit16+=年偏移 */
static const unsigned int lunar_info[] = {
    0x04bd8,  /* 2024 */
    0x04ae0,  /* 2025 */
    0x0a570,  /* 2026 */
    0x054d5,  /* 2027 */
    0x0d260,  /* 2028 */
    0x0d950,  /* 2029 */
    0x16554,  /* 2030 */
    0x056a0,  /* 2031 */
    0x09ad0,  /* 2032 */
    0x055d2,  /* 2033 */
    0x04ae0,  /* 2034 */
};

#define LUNAR_START_YEAR 2024

/* 获取农历年每月天数 */
static int lunar_month_days(unsigned int info, int month) {
    return (info & (1 << (16 - month))) ? 30 : 29;
}

/* 获取闰月 (0=无) */
static int lunar_leap_month(unsigned int info) {
    return info & 0xf;
}

/* 公历转农历 */
static int solar_to_lunar(int syear, int smonth, int sday, lunar_date_t *ld) {
    /* 简化实现: 使用查表法 */
    /* 2024年春节: 2024-02-10 */
    /* 2025年春节: 2025-01-29 */
    /* 2026年春节: 2026-02-17 */
    static const int spring_festival[][3] = {
        {2023, 1, 22},  /* ← 向前扩展一年，保证春节前日期不乱码 */
        {2024, 2, 10}, {2025, 1, 29}, {2026, 2, 17},
        {2027, 2, 6},  {2028, 1, 26}, {2029, 2, 13},
        {2030, 2, 3},  {2031, 1, 23}, {2032, 2, 11},
        {2033, 1, 31}, {2034, 2, 19}, {0, 0, 0}
    };

    int idx = syear - (LUNAR_START_YEAR - 1);  /* offset now starts at 2023 */
    if (idx < 0 || idx > 11) return -1;

    /* 计算从春节到目标日期的天数差 */
    int sf_month = spring_festival[idx][1];
    int sf_day = spring_festival[idx][2];

    /* 月份天数数组 — 考虑闰年 */
    int year_eff = syear;
    if (smonth < 3) year_eff--;  /* Jan/Feb use previous year for leap check */
    int is_leap = (year_eff % 4 == 0 && year_eff % 100 != 0) || (year_eff % 400 == 0);
    int month_days[] = {31, is_leap ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    /* 计算天数差 */
    int diff = 0;
    if (smonth > sf_month || (smonth == sf_month && sday >= sf_day)) {
        /* 在春节之后(同农历年) */
        for (int m = sf_month; m < smonth; m++) diff += month_days[m - 1];
        diff += sday - sf_day;
        ld->year = syear;
    } else {
        /* 在春节之前(前一个农历年) */
        for (int m = sf_month; m <= 12; m++) diff += month_days[m - 1];
        for (int m = 1; m < smonth; m++) diff += month_days[m - 1];
        diff += sday - sf_day;
        ld->year = syear - 1;
        idx = ld->year - LUNAR_START_YEAR;
        if (idx < 0) return -1;
    }

    /* 根据天数差查找农历月日 */
    unsigned int info = lunar_info[idx];
    ld->leap = 0;
    int d = diff;
    int leap = lunar_leap_month(info);

    for (int m = 1; m <= 12 && d >= 0; m++) {
        int md = lunar_month_days(info, m);
        if (d < md) {
            ld->month = m;
            ld->day = d + 1;
            return 0;
        }
        d -= md;

        /* 闰月 */
        if (m == leap && d >= 0) {
            int leap_days = lunar_month_days(info, 13); /* 闰月天数 */
            if (d < leap_days) {
                ld->month = m;
                ld->day = d + 1;
                ld->leap = 1;
                return 0;
            }
            d -= leap_days;
        }
    }

    ld->month = 12;
    ld->day = d + 1;
    return 0;
}

int liuren_get_lunar_today(lunar_date_t *ld) {
    const char *date_str = time_sync_get_date_string();
    if (!date_str || strlen(date_str) < 10) return -1;

    int y, m, d;
    if (sscanf(date_str, "%d-%d-%d", &y, &m, &d) != 3) return -1;

    return solar_to_lunar(y, m, d, ld);
}

int liuren_get_current_hour(void) {
    time_t now;
    struct tm *t;
    time(&now);
    t = localtime(&now);
    int h = t->tm_hour;
    if (h < 0 || h > 23) return 0;
    return hour_index_map[h];
}

const char *liuren_hour_name(int hour) {
    if (hour < 0 || hour > 11) return "未知";
    return hour_names[hour];
}

liuren_result_t liuren_calculate(int month, int day, int hour) {
    liuren_result_t res;

    /* 从大安(0)开始, month从正月(1)起 */
    int pos = (0 + (month - 1)) % 6;

    /* 从月位置起, 数到日 */
    pos = (pos + (day - 1)) % 6;

    /* 从日位置起, 数到时 */
    pos = (pos + hour) % 6;

    res.position = pos;
    res.name = pos_names[pos];
    res.meaning = pos_meanings[pos];
    res.judgment = pos_judgments[pos];
    return res;
}
