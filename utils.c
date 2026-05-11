#include "utils.h"

int myAbs(int value) {
    return value < 0 ? -value : value; 
}

int saturation(int value, int min, int max)
{
    if (value < min)
        return min;
    else if (value > max)
        return max;
    else
        return value;
}

float lowPass(float prev, float current, float alpha) {
    return alpha * current + (1 - alpha) * prev;
}
