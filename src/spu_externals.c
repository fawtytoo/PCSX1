// 15-bit value + 1-sign
int CLAMP16(int x)
{
    if (x > 32767)
        return 32767;

    if (x < -32768)
        return -32768;

    return x;
}
