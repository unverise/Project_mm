#ifndef _TRAN_KFIFO_H
#define _TRAN_KFIFO_H

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/stddef.h>
#include <linux/scatterlist.h>

struct __tran_kfifo {
    unsigned int    in;
    unsigned int    out;
    unsigned int    mask;
    unsigned int    esize;
    void        *data;
};

#define __STRUCT_TRAN_KFIFO_COMMON(datatype, recsize, ptrtype) \
    union { \
        struct __tran_kfifo    kfifo; \
        datatype    *type; \
        const datatype    *const_type; \
        char        (*rectype)[recsize]; \
        ptrtype        *ptr; \
        ptrtype const    *ptr_const; \
    }

#define __STRUCT_TRAN_KFIFO_PTR(type, recsize, ptrtype) \
{ \
    __STRUCT_TRAN_KFIFO_COMMON(type, recsize, ptrtype); \
    type        buf[0]; \
}

#define STRUCT_TRAN_KFIFO_PTR(type) \
    struct __STRUCT_TRAN_KFIFO_PTR(type, 0, type)

struct tran_kfifo_rec_ptr_2 __STRUCT_TRAN_KFIFO_PTR(unsigned char, 2, void);

/*
 * helper macro to distinguish between real in place fifo where the fifo
 * array is a part of the structure and the fifo type where the array is
 * outside of the fifo structure.
 */
#define    __tran_is_kfifo_ptr(fifo) \
    (sizeof(*fifo) == sizeof(STRUCT_TRAN_KFIFO_PTR(typeof(*(fifo)->type))))

static inline unsigned int __must_check
__tran_kfifo_uint_must_check_helper(unsigned int val)
{
    return val;
}

static inline int __must_check
__tran_kfifo_int_must_check_helper(int val)
{
    return val;
}

/**
 * kfifo_len - returns the number of used elements in the fifo
 * @fifo: address of the fifo to be used
 */
#define tran_kfifo_len(fifo) \
({ \
    typeof((fifo) + 1) __tmpl = (fifo); \
    __tmpl->kfifo.in - __tmpl->kfifo.out; \
})

/**
 * kfifo_is_empty - returns true if the fifo is empty
 * @fifo: address of the fifo to be used
 */
#define tran_kfifo_is_empty(fifo) \
({ \
    typeof((fifo) + 1) __tmpq = (fifo); \
    __tmpq->kfifo.in == __tmpq->kfifo.out; \
})

/**
 * kfifo_is_full - returns true if the fifo is full
 * @fifo: address of the fifo to be used
 */
#define tran_kfifo_is_full(fifo) \
({ \
    typeof((fifo) + 1) __tmpq = (fifo); \
    tran_kfifo_len(__tmpq) > __tmpq->kfifo.mask; \
})

/**
 * kfifo_alloc - dynamically allocates a new fifo buffer
 * @fifo: pointer to the fifo
 * @size: the number of elements in the fifo, this must be a power of 2
 * @gfp_mask: get_free_pages mask, passed to kmalloc()
 *
 * This macro dynamically allocates a new fifo buffer.
 *
 * The number of elements will be rounded-up to a power of 2.
 * The fifo will be release with kfifo_free().
 * Return 0 if no error, otherwise an error code.
 */
#define tran_kfifo_alloc(fifo, size, gfp_mask) \
__tran_kfifo_int_must_check_helper( \
({ \
    typeof((fifo) + 1) __tmp = (fifo); \
    struct __tran_kfifo *__tran_kfifo = &__tmp->kfifo; \
    __tran_is_kfifo_ptr(__tmp) ? \
    __tran_kfifo_alloc(__tran_kfifo, size, sizeof(*__tmp->type), gfp_mask) : \
    -EINVAL; \
}) \
)

/**
 * kfifo_free - frees the fifo
 * @fifo: the fifo to be freed
 */
#define tran_kfifo_free(fifo) \
({ \
    typeof((fifo) + 1) __tmp = (fifo); \
    struct __tran_kfifo *__tran_kfifo = &__tmp->kfifo; \
    if (__tran_is_kfifo_ptr(__tmp)) \
        __tran_kfifo_free(__tran_kfifo); \
})

/**
 * kfifo_init - initialize a fifo using a preallocated buffer
 * @fifo: the fifo to assign the buffer
 * @buffer: the preallocated buffer to be used
 * @size: the size of the internal buffer, this have to be a power of 2
 *
 * This macro initializes a fifo using a preallocated buffer.
 *
 * The number of elements will be rounded-up to a power of 2.
 * Return 0 if no error, otherwise an error code.
 */
#define tran_kfifo_init(fifo, buffer, size) \
({ \
    typeof((fifo) + 1) __tmp = (fifo); \
    struct __tran_kfifo *__tran_kfifo = &__tmp->kfifo; \
    __tran_is_kfifo_ptr(__tmp) ? \
    __tran_kfifo_init(__tran_kfifo, buffer, size, sizeof(*__tmp->type)) : \
    -EINVAL; \
})

/**
 * kfifo_in - put data into the fifo
 * @fifo: address of the fifo to be used
 * @buf: the data to be added
 * @n: number of elements to be added
 *
 * This macro copies the given buffer into the fifo and returns the
 * number of copied elements.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these macro.
 */
#define tran_kfifo_in(fifo, buf, n) \
({ \
    typeof((fifo) + 1) __tmp = (fifo); \
    typeof(__tmp->ptr_const) __buf = (buf); \
    unsigned long __n = (n); \
    const size_t __recsize = sizeof(*__tmp->rectype); \
    struct __tran_kfifo *__tran_kfifo = &__tmp->kfifo; \
    (__recsize) ?\
    __tran_kfifo_in_r(__tran_kfifo, __buf, __n, __recsize) : \
    __tran_kfifo_in(__tran_kfifo, __buf, __n); \
})

/**
 * kfifo_out - get data from the fifo
 * @fifo: address of the fifo to be used
 * @buf: pointer to the storage buffer
 * @n: max. number of elements to get
 *
 * This macro get some data from the fifo and return the numbers of elements
 * copied.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these macro.
 */
#define tran_kfifo_out(fifo, buf, n) \
__tran_kfifo_uint_must_check_helper( \
({ \
    typeof((fifo) + 1) __tmp = (fifo); \
    typeof(__tmp->ptr) __buf = (buf); \
    unsigned long __n = (n); \
    const size_t __recsize = sizeof(*__tmp->rectype); \
    struct __tran_kfifo *__tran_kfifo = &__tmp->kfifo; \
    (__recsize) ?\
    __tran_kfifo_out_r(__tran_kfifo, __buf, __n, __recsize) : \
    __tran_kfifo_out(__tran_kfifo, __buf, __n); \
}) \
)

extern int __tran_kfifo_alloc(struct __tran_kfifo *fifo, unsigned int size,
    size_t esize, gfp_t gfp_mask);

extern void __tran_kfifo_free(struct __tran_kfifo *fifo);

extern int __tran_kfifo_init(struct __tran_kfifo *fifo, void *buffer,
    unsigned int size, size_t esize);

extern unsigned int __tran_kfifo_in(struct __tran_kfifo *fifo,
    const void *buf, unsigned int len);

extern unsigned int __tran_kfifo_out(struct __tran_kfifo *fifo,
    void *buf, unsigned int len);

extern unsigned int __tran_kfifo_in_r(struct __tran_kfifo *fifo,
    const void *buf, unsigned int len, size_t recsize);

extern unsigned int __tran_kfifo_out_r(struct __tran_kfifo *fifo,
    void *buf, unsigned int len, size_t recsize);

#endif