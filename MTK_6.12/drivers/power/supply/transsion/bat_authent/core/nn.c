


















#include "fl_type.h"
#include "nn.h"

static void nn_assign(uint32_t *a, uint32_t *b, uint32_t digits)
{
	if (digits) {
		do
			*a++ = *b++;
		while (--digits);
	}
}

static void nn_assignzero(uint32_t *a, uint32_t digits)
{
	if (digits) {
		do
			*a++ = 0;
		while (--digits);
	}
}

static void dmult(uint32_t a, uint32_t b, uint32_t *high, uint32_t *low)
{
	uint16_t aa[4];
	uint32_t m1;
	uint32_t m2;
	uint32_t m;
	uint32_t ml;
	uint32_t mh;
	uint32_t carry;

	carry = 0;
	aa[0] = (uint16_t)(a & MAX_NN_HALF_DIGIT);
	aa[1] = (uint16_t)((a >> NN_HALF_DIGIT_BITS) & MAX_NN_HALF_DIGIT);
	aa[2] = (uint16_t)(b & MAX_NN_HALF_DIGIT);
	aa[3] = (uint16_t)((b >> NN_HALF_DIGIT_BITS) & MAX_NN_HALF_DIGIT);
	*low = (uint32_t)aa[0] * aa[2];
	*high = (uint32_t)aa[1] * aa[3];
	m1 = (uint32_t)aa[0] * aa[3];
	m2 = (uint32_t)aa[1] * aa[2];
	m = m1 + m2;
	if (m < m1)
		carry = 1L << (NN_DIGIT_BITS / 2);
	ml = (m & MAX_NN_HALF_DIGIT) << (NN_DIGIT_BITS / 2);
	mh = m >> (NN_DIGIT_BITS / 2);
	*low += ml;
	if (*low < ml)
		carry++;
	*high += carry + mh;
}

static uint32_t nn_add_generate_carry(uint32_t t1, uint32_t c)
{
	if (t1 < c)
		return 1;
	else
		return 0;
}

/* Computes a = b + c. Returns carry.
 *
 *       Lengths: a[digits], b[digits], c[digits].
 */
uint32_t nn_add(uint32_t *a, uint32_t *b, uint32_t *c, uint32_t digits)
{
	uint32_t temp;
	uint32_t carry;

	carry = 0;
	if (digits) {
		do {
			temp = (*b++) + carry;
			if (temp < carry) {
				temp = *c;
			} else {
				temp += *c;
				carry = nn_add_generate_carry(temp, *c);
			}
			*a++ = temp;
			c++;
		} while (--digits);
	}
	return carry;
}

static uint32_t nn_sub_generate_carry(uint32_t t1, uint32_t t2)
{
	if (t1 > t2)
		return 1;
	else
		return 0;
}
/* Computes a = b - c. Returns borrow.
 *
 *       Lengths: a[digits], b[digits], c[digits].
 */
uint32_t nn_sub(uint32_t *a, uint32_t *b, uint32_t *c, uint32_t digits)
{
	uint32_t temp;
	uint32_t temp2;
	uint32_t borrow;

	borrow = 0;
	if (digits) {
		do {
			temp = (*b++) - borrow;
			if (temp == MAX_NN_DIGIT) {
				temp = MAX_NN_DIGIT - *c++;
			} else {
				temp  = temp - *c;
				temp2 = MAX_NN_DIGIT - *c;
				c++;
				borrow = nn_sub_generate_carry(temp, temp2);
			}
			*a++ = temp;
		} while (--digits);
	}

	return borrow;
}

static uint32_t nn_digits(uint32_t *a, uint32_t digits)
{
	if (digits) {
		digits--;
		do {
			if (*(a + digits))
				break;
		} while (digits--);
		return (digits + 1);
	}
	return digits;
}

static uint32_t nn_mult_c_loop(uint32_t *t, uint32_t *b, uint32_t *c,
		uint32_t i, uint32_t c_digits)
{
	uint32_t dhigh;
	uint32_t dlow;
	uint32_t carry;
	uint32_t j;

	carry = 0;
	for (j = 0; j < c_digits; j++) {
		dmult(*(b + i), *(c + j), &dhigh, &dlow);
		*(t + (i + j)) = *(t + (i + j)) + carry;
		if (*(t + (i + j)) < carry)
			carry = 1;
		else
			carry = 0;
		*(t + (i + j)) += dlow;
		if (*(t + (i + j)) < dlow)
			carry++;
		carry += dhigh;
	}
	return carry;
}

/* Computes a = b * c.
 *       Lengths: a[2*digits], b[digits], c[digits].
 *       Assumes digits < MAX_NN_DIGITS.
 */
static void nn_mult(uint32_t *a, uint32_t *b, uint32_t *c, uint32_t digits)
{
	uint32_t t[2 * MAX_NN_DIGITS];
	uint32_t carry;
	uint32_t b_digits;
	uint32_t c_digits;
	uint32_t i;

	nn_assignzero(t, 2 * digits);
	b_digits = nn_digits(b, digits);
	c_digits = nn_digits(c, digits);
	for (i = 0; i < b_digits; i++) {
		if (*(b + i) != 0)
			carry = nn_mult_c_loop(t, b, c, i, c_digits);
		else
			carry = 0;

		*(t + (i + c_digits)) += carry;
	}
	nn_assign(a, t, 2 * digits);
}

/* Computes a = b * c * R^-1mod d.
 *
 * Lengths: a[digits], b[digits], c[digits], d[digits].
 * Assumes d > 0, digits < MAX_NN_DIGITS.
 */
uint32_t nn_montgomery_modmult(uint32_t *a, uint32_t *b, uint32_t *c,
		uint32_t *n, uint32_t *modm, uint32_t digits)
{
	uint32_t i;
	uint32_t j;
	uint32_t t[2 * MAX_NN_DIGITS];
	uint32_t temp[2];
	uint32_t temp32[MAX_NN_DIGITS];
	uint32_t temp64[2 * MAX_NN_DIGITS];

	if (digits == 0)
		return 0;
	for (i = 0; i < digits; i++)
		temp32[digits - 1 - i] = 0;
	for (i = 0; i < 2 * digits; i++) {
		t[2 * digits - 1 - i] = 0;
		temp64[2 * digits - 1 - i] = 0;
	}
	for (i = 0; i < digits; i++) {
		nn_mult(temp, &b[i], &c[0], 1);
		nn_add(&temp[0], &temp[0], &t[0], 1);
		nn_mult(temp, &temp[0], modm, 1);

		temp32[0] = temp[0];
		nn_mult(temp64, temp32, n, digits);

		nn_add(t, t, temp64, 2 * digits);
		temp32[0] = b[i];
		nn_mult(temp64, temp32, c, digits);
		nn_add(t, t, temp64, 2 * digits);

		for (j = 0; j < 2 * digits - 1; j++)
			t[j] = t[j + 1];
	}

	if (t[digits] > 0) {
		nn_assignzero(temp64, 2 * digits);
		nn_assign(temp64, n, digits);
		nn_sub(t, t, temp64, 2 * digits);
		for (i = 0; i < digits; i++)
			a[i] = t[i];
		return 1;
	}
	for (i = 0; i < digits; i++)
		a[i] = t[i];
	return 0;
}

