#ifndef CEDA_TIME_H
#define CEDA_TIME_H

typedef long ms_time_t;
typedef long us_time_t;

/**
 * @brief Get current clock time in [ms].
 *
 * @return long Current clock time. [ms]
 */
ms_time_t time_now_ms(void);

/**
 * @brief Get current clock time in [us].
 *
 * @return long Current clock time. [us]
 */
us_time_t time_now_us(void);

#endif
