/* ROM-free checks of the comparison build's six functions: tanf, atanf and
 * cosf (LP64-006 Task 4, port/src/port_compare_math.c) and sinf, expf and
 * powf (MAC-001 Task 8, port/src/port_compare_math_arm.c and
 * port_compare_math_arm_sinf.c); test_lp64_compare.sh links this with them. Each is within two units in the
 * last place of the C library's double function, rounded to float, over
 * 100,000 inputs; tanf(+-0) is +-0; atanf is odd.
 * The system <math.h> is shadowed by the tree's include/math.h, so the
 * functions are declared here. Compiled with -fno-builtin, so no call is
 * folded at compile time. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

float tanf(float);
float atanf(float);
float cosf(float);
float sinf(float);
float expf(float);
float powf(float, float);
double tan(double);
double atan(double);
double cos(double);
double sin(double);
double exp(double);
double pow(double, double);

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

static void sinf_is_within_two_ulp_of_double_sin(void)
{
    // Arrange, Act
    int64_t worst = worstUlp("sinf", sinf, sin);

    // Assert
    expect(worst <= 2, "sinf is within two ulp of double sin");
}

static void expf_is_within_two_ulp_of_double_exp(void)
{
    // Arrange, Act
    int64_t worst = worstUlp("expf", expf, exp);

    // Assert
    expect(worst <= 2, "expf is within two ulp of double exp");
}

/* powf over a base of any magnitude from zero up (every second one below 16)
 * and an exponent in [-16, 16): no negative base, whose result is a NaN for
 * most exponents, and a NaN's sign is the host's. */
static void powf_is_within_two_ulp_of_double_pow(void)
{
    // Arrange
    int64_t worst = 0;
    float worstBase = 0.0f;
    float worstExponent = 0.0f;
    uint32_t k;

    // Act
    for (k = 0; k < TEST_INPUTS; k++)
    {
        float base = bitsFloat(floatBits(testInput(k)) & 0x7fffffffu);
        float exponent = testInput(2u * k + 1u);
        int64_t d = ulpDistance(powf(base, exponent), (float) pow((double) base, (double) exponent));

        if (d > worst)
        {
            worst = d;
            worstBase = base;
            worstExponent = exponent;
        }
    }
    if (worst > 2)
    {
        printf("test_compare_math: powf(%a, %a) is %lld ulp from the double function\n", worstBase,
               worstExponent, (long long) worst);
    }

    // Assert
    expect(worst <= 2, "powf is within two ulp of double pow");
}

/* The cases the sweep and the ulp checks never reach: the branches of
 * powf's special cases, expf past its range, sinf of a zero. Each answer is
 * exact, so it is compared by its bits; a NaN only as being one. */
static void sinf_expf_and_powf_special_cases_are_exact(void)
{
    // Arrange
    static const struct
    {
        float base, exponent;
        uint32_t bits;
    } powers[] = {
        {-8.0f, 3.0f, 0xc4000000u},        /* a negative base, odd power: -512 */
        {-2.0f, 2.0f, 0x40800000u},        /* even power: 4 */
        {4.0f, 0.5f, 0x40000000u},         /* 2 */
        {0.0f, -1.0f, 0x7f800000u},        /* +0 to a negative power: +inf */
        {-0.0f, -3.0f, 0xff800000u},       /* -0 to a negative odd power: -inf */
        {-0.0f, 3.0f, 0x80000000u},        /* -0 */
        {-0.0f, 2.0f, 0x00000000u},        /* +0 */
        {123.0f, 0.0f, 0x3f800000u},       /* anything to 0: 1 */
        {1.0f, -77.5f, 0x3f800000u},       /* 1 to anything: 1 */
        {2.0f, 1.0f / 0.0f, 0x7f800000u},  /* above 1 to +inf: +inf */
        {0.5f, 1.0f / 0.0f, 0x00000000u},  /* below 1 to +inf: 0 */
        {2.0f, -1.0f / 0.0f, 0x00000000u}, /* above 1 to -inf: 0 */
        {-1.0f, 1.0f / 0.0f, 0x3f800000u}, /* -1 to an infinity: 1 */
        {2.0f, 200.0f, 0x7f800000u},       /* overflow */
        {2.0f, -200.0f, 0x00000000u},      /* underflow */
        {-2.0f, 201.0f, 0xff800000u},      /* overflow, negative */
        {0x1p-140f, 0.5f, 0x1c800000u},    /* a subnormal base: 2^-70 */
    };
    volatile float zero = 0.0f;
    volatile float negativeZero = -0.0f;
    volatile float infinity = 1.0f / 0.0f;
    volatile float negativeBase = -8.0f;
    volatile float half = 0.5f;
    int exact = 1;
    uint32_t k;

    // Act
    for (k = 0; k < sizeof(powers) / sizeof(powers[0]); k++)
    {
        volatile float base = powers[k].base;
        volatile float exponent = powers[k].exponent;

        if (floatBits(powf(base, exponent)) != powers[k].bits)
        {
            printf("test_compare_math: powf(%a, %a) is %08x, expected %08x\n", (double) base, (double) exponent,
                   (unsigned) floatBits(powf(base, exponent)), (unsigned) powers[k].bits);
            exact = 0;
        }
    }
    float rootOfNegative = powf(negativeBase, half);
    float sineOfInfinity = sinf(infinity);

    // Assert
    expect(exact, "powf's special cases are exact");
    expect(rootOfNegative != rootOfNegative, "powf of a negative base and a fraction is a NaN");
    expect(sineOfInfinity != sineOfInfinity, "sinf of an infinity is a NaN");
    expect(floatBits(sinf(zero)) == 0x00000000u, "sinf(0) is +0");
    expect(floatBits(sinf(negativeZero)) == 0x80000000u, "sinf(-0) is -0");
    expect(floatBits(expf(zero)) == 0x3f800000u, "expf(0) is 1");
    expect(floatBits(expf(-infinity)) == 0x00000000u, "expf(-inf) is +0");
    expect(floatBits(expf(infinity)) == 0x7f800000u, "expf(+inf) is +inf");
    expect(floatBits(expf(negativeBase * -12.5f)) == 0x7f800000u, "expf(100) overflows to +inf");
    expect(floatBits(expf(negativeBase * 25.0f)) == 0x00000000u, "expf(-200) underflows to +0");
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
    sinf_is_within_two_ulp_of_double_sin();
    expf_is_within_two_ulp_of_double_exp();
    powf_is_within_two_ulp_of_double_pow();
    sinf_expf_and_powf_special_cases_are_exact();
    tanf_of_zero_is_zero();
    atanf_is_odd();
    if (failures != 0)
    {
        return 1;
    }
    printf("test_compare_math: PASS (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
