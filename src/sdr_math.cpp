#include "sdr_math.h"


float ApproxAtan(float z)
{
    const float n1 = 0.97239411f;
    const float n2 = -0.19194795f;
    return (n1 + n2 * z * z) * z;
}

float ApproxAtan2(float y, float x)
{
    if (x != 0.0f)
    {
        if (fabsf(x) > fabsf(y))
        {
            const float z = y / x;
            if (x > 0.0f)
            {
                // atan2(y,x) = atan(y/x) if x > 0
                return ApproxAtan(z);
            }
            else if (y >= 0.0f)
            {
                // atan2(y,x) = atan(y/x) + PI if x < 0, y >= 0
                return ApproxAtan(z) + PI;
            }
            else
            {
                // atan2(y,x) = atan(y/x) - PI if x < 0, y < 0
                return ApproxAtan(z) - PI;
            }
        }
        else // Use property atan(y/x) = PI/2 - atan(x/y) if |y/x| > 1.
        {
            const float z = x / y;
            if (y > 0.0f)
            {
                // atan2(y,x) = PI/2 - atan(x/y) if |y/x| > 1, y > 0
                return -ApproxAtan(z) + PIH;
            }
            else
            {
                // atan2(y,x) = -PI/2 - atan(x/y) if |y/x| > 1, y < 0
                return -ApproxAtan(z) - PIH;
            }
        }
    }
    else
    {
        if (y > 0.0f) // x = 0, y > 0
        {
            return PIH;
        }
        else if (y < 0.0f) // x = 0, y < 0
        {
            return -PIH;
        }
    }
    return 0.0f; // x,y = 0. Could return NaN instead.
}

// *********************************************************************************************************

float log10f_fast(float X)
{
    float Y, F;
    int E;
    F = frexpf(fabsf(X), &E);
    Y = 1.23149591368684f;
    Y *= F;
    Y += -4.11852516267426f;
    Y *= F;
    Y += 6.02197014179219f;
    Y *= F;
    Y += -3.13396450166353f;
    Y += E;
    return (Y * 0.3010299956639812f);
}

float convertToF32(int16_t sample)
{

    float s = (float)sample;

    // s -= 32768.0;
    if (s > 0.0)
    {
        return s / 32767.0;
    }
    return s / 32768.0;
}

int16_t convertToInt16(float sample)
{
    if (sample > 0.0)
    {
        sample *= 32767.0;
    }
    else
    {
        sample *= 32768.0;
    }

    // sample += 32768.0;

    return (int16_t)sample;
}

// *********************************************************************************************************

float sign(float x)
{
    if (x < 0)
        return -1.0f;
    else if (x > 0)
        return 1.0f;
    else
        return 0.0f;
}