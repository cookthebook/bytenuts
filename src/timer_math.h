#ifndef _TIMER_MATH_H_
#define _TIMER_MATH_H_

#include <stdint.h>
#include <time.h>

/* NOTE
 * All math here is assuming that the second and nanosecond values of the
 * timespecs are all non-negative, and nanoseconds is in the range [0, 999999999] */

/* Compare the two timespecs
 * Return -1 if a < b, 0 if a = b, and 1 if a > b */
int timer_cmp(const struct timespec *a, const struct timespec *b);

/* Add the timespec value of b to a */
void timer_add(struct timespec *a, const struct timespec *b);

/* Add ms milliseconds to the timespec*/
void timer_add_ms(struct timespec *ts, uint32_t ms);

/* Subtract time b from a. If b > a, then a is set to 0 */
void timer_sub(struct timespec *a, const struct timespec *b);

/* Subtract ms milliseconds from ts. If ts would become negative, it is set to 0 */
void timer_sub_ms(struct timespec *ts, uint32_t ms);

#endif /* _TIMER_MATH_H_ */
