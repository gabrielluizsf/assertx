int xsum(int a, int b)
{
    return a + b;
}

int xdiv(int a, int b)
{
    if (b == 0)
    {
        return -1;
    }
    return a / b;
}

#define bool int
#define true 1
#define false 0

bool is_even(int number) {
    return number % 2 == 0;
}