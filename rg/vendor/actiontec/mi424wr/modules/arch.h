#ifndef _MI424WR_ARCH_H_
#define _MI424WR_ARCH_H_

extern int mi424wr_en2010_count;
extern int mi424wr_en2210_count;

typedef enum {
    MI424WR_UNKNOWN = 0,
    MI424WR_REVC = 1,
    MI424WR_REVD = 2,
} mi424wr_rev_t;

mi424wr_rev_t mi424wr_rev_get(void);

#define MI424WR_IS_REVC() (mi424wr_rev_get() == MI424WR_REVC)
#define MI424WR_IS_REVD() (mi424wr_rev_get() == MI424WR_REVD)

#endif
