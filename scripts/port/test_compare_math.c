/* ROM-free checks of the comparison build's tanf, atanf and cosf (LP64-006
 * Task 4, test_lp64_compare.sh links this with port/src/port_compare_math.c):
 * each is within two units in the last place of the C library's double tan,
 * atan or cos, rounded to float, over 100,000 inputs; tanf(+-0) is +-0; atanf
 * is odd.
 * The system <math.h> is shadowed by the tree's include/math.h, so the
 * functions are declared here. Compiled with -fno-builtin, so no call is
 * folded at compile time. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

float tanf(float);
float atanf(float);
float cosf(float);
double tan(double);
double atan(double);
double cos(double);

#define TEST_INPUTS 100000

static int failures;

static void expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_compare_math: FAIL (%d-bit): %s\n", (int) (sizeof(void *) * 8), what);
        failures++;
    }
}

static uint32_t floatBits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float bitsFloat(uint32_t bits)
{
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* Floats ordered as integers: adjacent floats differ by one, across zero too. */
static int64_t orderedBits(float value)
{
    uint32_t bits = floatBits(value);
    return (bits & 0x80000000u) ? -(int64_t) (bits & 0x7fffffffu) : (int64_t) bits;
}

static int64_t ulpDistance(float a, float b)
{
    int64_t d = orderedBits(a) - orderedBits(b);
    return d < 0 ? -d : d;
}

/* The k-th test input: a finite float of any sign and magnitude, from a fixed
 * linear congruential sequence, so every host tests the same inputs. Every
 * second input is drawn from [-16, 16), where the game's angles lie. */
static float testInput(uint32_t k)
{
    uint32_t state = k * 1664525u + 1013904223u;
    state ^= state >> 13;
    state *= 2654435761u;
    if (k & 1u)
    {
        return (float) ((int32_t) (state >> 8) - (1 << 23)) / (float) (1 << 19);
    }
    if ((state & 0x7f800000u) == 0x7f800000u)
    {
        state &= ~0x00800000u; /* infinity or NaN: halve the exponent field's top */
    }
    return bitsFloat(state);
}

/* The largest distance, in ulp, of `single` from `reference` rounded to
 * float over the test inputs; reports the worst input past two ulp. */
static int64_t worstUlp(const char *name, float (*single)(float), double (*reference)(double))
{
    int64_t worst = 0;
    float worstInput = 0.0f;
    uint32_t k;

    for (k = 0; k < TEST_INPUTS; k++)
    {
        float x = testInput(k);
        int64_t d = ulpDistance(single(x), (float) reference((double) x));

        if (d > worst)
        {
            worst = d;
            worstInput = x;
        }
    }
    if (worst > 2)
    {
        printf("test_compare_math: %s(%a) is %lld ulp from the double function\n", name, worstInput,
               (long long) worst);
    }
    return worst;
}

static void tanf_is_within_two_ulp_of_double_tan(void)
{
    // Arrange, Act
    int64_t worst = worstUlp("tanf", tanf, tan);

    // Assert
    expect(worst <= 2, "tanf is within two ulp of double tan");
}

static void atanf_is_within_two_ulp_of_double_atan(void)
{
    // Arrange, Act
    int64_t worst = worstUlp("atanf", atanf, atan);

    // Assert
    expect(worst <= 2, "atanf is within two ulp of double atan");
}

static void cosf_is_within_two_ulp_of_double_cos(void)
{
    // Arrange, Act
    int64_t worst = worstUlp("cosf", cosf, cos);

    // Assert
    expect(worst <= 2, "cosf is within two ulp of double cos");
}

static void tanf_of_zero_is_zero(void)
{
    // Arrange
    volatile float zero = 0.0f;
    volatile float negativeZero = -0.0f;

    // Act
    float positive = tanf(zero);
    float negative = tanf(negativeZero);

    // Assert
    expect(floatBits(positive) == 0x00000000u, "tanf(0) is +0");
    expect(floatBits(negative) == 0x80000000u, "tanf(-0) is -0");
}

static void atanf_is_odd(void)
{
    // Arrange
    int odd = 1;
    uint32_t k;

    for (k = 0; k < TEST_INPUTS; k++)
    {
        float x = testInput(k);

        // Act
        float positive = atanf(x);
        float negative = atanf(-x);

        // Assert (gathered over all inputs)
        if (floatBits(negative) != (floatBits(positive) ^ 0x80000000u))
        {
            odd = 0;
        }
    }
    expect(odd, "atanf(-x) is -atanf(x), bit for bit");
}

int main(void)
{
    tanf_is_within_two_ulp_of_double_tan();
    atanf_is_within_two_ulp_of_double_atan();
    cosf_is_within_two_ulp_of_double_cos();
    tanf_of_zero_is_zero();
    atanf_is_odd();
    if (failures != 0)
    {
        return 1;
    }
    printf("test_compare_math: PASS (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
