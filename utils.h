#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

/* Математические утилиты */
int myAbs(int value);
int saturation(int value, int min, int max);
float lowPass(float prev, float current, float alpha);

#endif // UTILS_H
