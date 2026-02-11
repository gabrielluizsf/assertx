#include <stdio.h>
#include "../src/xmath.h"
#include "xassert.h"

void test_sum()
{
    assert_equal(xsum(5, 10), 15, "5 + 10 should be 15");
    return;
}

void test_div() 
{
    assert_equal(xdiv(10, 0), -1, "10/0 should be -1");
}
