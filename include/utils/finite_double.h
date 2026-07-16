/**
 * @file finite_double.h
 * @brief IEEE-754 classification that remains valid under -ffast-math
 */

#ifndef UTILS_FINITE_DOUBLE_H
#define UTILS_FINITE_DOUBLE_H

#include <stdint.h>
#include <string.h>

static inline uint64_t DoubleBitsStrict(double value) {
    uint64_t bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static inline int DoubleIsFiniteStrict(double value) {
    return (DoubleBitsStrict(value) & UINT64_C(0x7ff0000000000000)) !=
           UINT64_C(0x7ff0000000000000);
}

static inline int DoubleIsNaNStrict(double value) {
    uint64_t bits = DoubleBitsStrict(value);
    return (bits & UINT64_C(0x7ff0000000000000)) ==
               UINT64_C(0x7ff0000000000000) &&
           (bits & UINT64_C(0x000fffffffffffff)) != 0;
}

#endif /* UTILS_FINITE_DOUBLE_H */
