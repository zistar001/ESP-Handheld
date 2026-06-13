#ifndef LIUREN_CORE_H
#define LIUREN_CORE_H

/* 小六壬六神位置 */
typedef enum {
    LIUREN_DA_AN = 0,    /* 大安 1 */
    LIUREN_LIU_LIAN,     /* 留连 2 */
    LIUREN_SU_XI,        /* 速喜 3 */
    LIUREN_CHI_KOU,      /* 赤口 4 */
    LIUREN_XIAO_JI,      /* 小吉 5 */
    LIUREN_KONG_WANG,    /* 空亡 6 */
    LIUREN_MAX
} liuren_pos_t;

typedef struct {
    int year;        /* 农历年 */
    int month;       /* 农历月 (1-12) */
    int day;         /* 农历日 (1-30) */
    int leap;        /* 是否闰月 */
} lunar_date_t;

typedef struct {
    liuren_pos_t position;
    const char *name;
    const char *meaning;
    const char *judgment;
} liuren_result_t;

/* 获取当天农历日期 */
int liuren_get_lunar_today(lunar_date_t *ld);

/* 获取当前时辰 (0=子时, 1=丑时, ... 11=亥时) */
int liuren_get_current_hour(void);

/* 执行小六壬推算 */
liuren_result_t liuren_calculate(int month, int day, int hour);

/* 获取时辰名称 */
const char *liuren_hour_name(int hour);

#endif
