#ifndef ICHING_DATA_H
#define ICHING_DATA_H

typedef struct {
    const char *name;      /* 卦名 */
    const char *gua_ci;    /* 卦辞 */
    const char *xiang;     /* 《象》曰 */
    const char *daxiang;   /* 大象 */
    const char *yushi;     /* 运势 */
    const char *shiye;     /* 事业 */
    const char *jingshang; /* 经商 */
    const char *qiuming;   /* 求名 */
    const char *hunlian;   /* 婚恋 */
    const char *juece;     /* 决策 */
} iching_hexagram_t;

extern const iching_hexagram_t g_iching[64];

#endif
