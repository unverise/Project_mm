#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/log2.h>
#include <linux/uaccess.h>

#include "tran_kfifo.h"

/*
 * internal helper to calculate the unused elements in a fifo
 */
static inline unsigned int tran_kfifo_unused(struct __tran_kfifo *fifo)
{
	return (fifo->mask + 1) - (fifo->in - fifo->out);
}

int __tran_kfifo_alloc(struct __tran_kfifo *fifo, unsigned int size,
		size_t esize, gfp_t gfp_mask)
{
	/*
	 * round up to the next power of 2, since our 'let the indices
	 * wrap' technique works only in this case.
	 */
	size = roundup_pow_of_two(size);

	fifo->in = 0;
	fifo->out = 0;
	fifo->esize = esize;

	if (size < 2) {
		fifo->data = NULL;
		fifo->mask = 0;
		return -EINVAL;
	}

	fifo->data = kmalloc_array(esize, size, gfp_mask);

	if (!fifo->data) {
		fifo->mask = 0;
		return -ENOMEM;
	}
	fifo->mask = size - 1;

	return 0;
}

void __tran_kfifo_free(struct __tran_kfifo *fifo)
{
	kfree(fifo->data);
	fifo->in = 0;
	fifo->out = 0;
	fifo->esize = 0;
	fifo->data = NULL;
	fifo->mask = 0;
}

int __tran_kfifo_init(struct __tran_kfifo *fifo, void *buffer,
		unsigned int size, size_t esize)
{
	size /= esize;

	if (!is_power_of_2(size))
		size = rounddown_pow_of_two(size);

	fifo->in = 0;
	fifo->out = 0;
	fifo->esize = esize;
	fifo->data = buffer;

	if (size < 2) {
		fifo->mask = 0;
		return -EINVAL;
	}
	fifo->mask = size - 1;

	return 0;
}

static void tran_kfifo_copy_in(struct __tran_kfifo *fifo, const void *src,
		unsigned int len, unsigned int off)
{
	unsigned int size = fifo->mask + 1;
	unsigned int esize = fifo->esize;
	unsigned int l;

	off &= fifo->mask;
	if (esize != 1) {
		off *= esize;
		size *= esize;
		len *= esize;
	}
	l = min(len, size - off);

	memcpy(fifo->data + off, src, l);
	memcpy(fifo->data, src + l, len - l);
	/*
	 * make sure that the data in the fifo is up to date before
	 * incrementing the fifo->in index counter
	 */
	smp_wmb();
}

unsigned int __tran_kfifo_in(struct __tran_kfifo *fifo,
		const void *buf, unsigned int len)
{
	unsigned int l;

	l = tran_kfifo_unused(fifo);
	if (len > l)
		len = l;

	tran_kfifo_copy_in(fifo, buf, len, fifo->in);
	fifo->in += len;
	return len;
}

static void tran_kfifo_copy_out(struct __tran_kfifo *fifo, void *dst,
		unsigned int len, unsigned int off)
{
	unsigned int size = fifo->mask + 1;
	unsigned int esize = fifo->esize;
	unsigned int l;

	off &= fifo->mask;
	if (esize != 1) {
		off *= esize;
		size *= esize;
		len *= esize;
	}
	l = min(len, size - off);

	memcpy(dst, fifo->data + off, l);
	memcpy(dst + l, fifo->data, len - l);
	/*
	 * make sure that the data is copied before
	 * incrementing the fifo->out index counter
	 */
	smp_wmb();
}

unsigned int __tran_kfifo_out_peek(struct __tran_kfifo *fifo,
		void *buf, unsigned int len)
{
	unsigned int l;

	l = fifo->in - fifo->out;
	if (len > l)
		len = l;

	tran_kfifo_copy_out(fifo, buf, len, fifo->out);
	return len;
}

unsigned int __tran_kfifo_out(struct __tran_kfifo *fifo,
		void *buf, unsigned int len)
{
	len = __tran_kfifo_out_peek(fifo, buf, len);
	fifo->out += len;
	return len;
}

#define	__TRAN_KFIFO_POKE(data, in, mask, val) \
	( \
	(data)[(in) & (mask)] = (unsigned char)(val) \
	)

/*
 * __tran_kfifo_poke_n internal helper function for storeing the length of
 * the record into the fifo
 */
static void __tran_kfifo_poke_n(struct __tran_kfifo *fifo, unsigned int n, size_t recsize)
{
	unsigned int mask = fifo->mask;
	unsigned char *data = fifo->data;

	__TRAN_KFIFO_POKE(data, fifo->in, mask, n);

	if (recsize > 1)
		__TRAN_KFIFO_POKE(data, fifo->in + 1, mask, n >> 8);
}

unsigned int __tran_kfifo_in_r(struct __tran_kfifo *fifo, const void *buf,
		unsigned int len, size_t recsize)
{
	if (len + recsize > tran_kfifo_unused(fifo))
		return 0;

	__tran_kfifo_poke_n(fifo, len, recsize);

	tran_kfifo_copy_in(fifo, buf, len, fifo->in + recsize);
	fifo->in += len + recsize;
	return len;
}

#define	__TRAN_KFIFO_PEEK(data, out, mask) \
	((data)[(out) & (mask)])
/*
 * __tran_kfifo_peek_n internal helper function for determinate the length of
 * the next record in the fifo
 */
static unsigned int __tran_kfifo_peek_n(struct __tran_kfifo *fifo, size_t recsize)
{
	unsigned int l;
	unsigned int mask = fifo->mask;
	unsigned char *data = fifo->data;

	l = __TRAN_KFIFO_PEEK(data, fifo->out, mask);

	if (--recsize)
		l |= __TRAN_KFIFO_PEEK(data, fifo->out + 1, mask) << 8;

	return l;
}

static unsigned int tran_kfifo_out_copy_r(struct __tran_kfifo *fifo,
	void *buf, unsigned int len, size_t recsize, unsigned int *n)
{
	*n = __tran_kfifo_peek_n(fifo, recsize);

	if (len > *n)
		len = *n;

	tran_kfifo_copy_out(fifo, buf, len, fifo->out + recsize);
	return len;
}

unsigned int __tran_kfifo_out_r(struct __tran_kfifo *fifo, void *buf,
		unsigned int len, size_t recsize)
{
	unsigned int n;

	if (fifo->in == fifo->out)
		return 0;

	len = tran_kfifo_out_copy_r(fifo, buf, len, recsize, &n);
	fifo->out += n + recsize;
	return len;
}
