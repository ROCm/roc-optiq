#pragma once

template <typename T>
T
clamp(T value, T minVal, T maxVal)
{
    if(value < minVal)
    {
        return minVal;
    }
    else if(value > maxVal)
    {
        return maxVal;
    }
    return value;
}
