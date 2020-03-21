#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include "bignum_k/bn.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
// Show the version on /sys/module/fibdrv/version
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"
#define PROC_FIBONACCI_BASE_DIR "fibonacci"
/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 1000
#define MAX_DIGITS 256

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static int fib_flag = 0;
static DEFINE_MUTEX(fib_mutex);

static ssize_t fib_proc_read(struct file *file,
                             char __user *buffer,
                             size_t count,
                             loff_t *ppos)
{
    int len = 0;
    char kbuf[128];
    printk(KERN_INFO "%s called\n", __func__);
    if (*ppos > 0 || count < 128)
        return 0;
    len = snprintf(kbuf, 16, "fib_flag = %d\n", fib_flag);
    if (copy_to_user(buffer, kbuf, len))
        return -EFAULT;
    *ppos = len;
    return (len);
}

static ssize_t fib_proc_write(struct file *file,
                              const char __user *buffer,
                              size_t count,
                              loff_t *ppos)
{
    char kbuf[128];
    unsigned long len = count;
    int n;

    printk(KERN_INFO "%d (%s)\n", (int) len, __func__);

    if (*ppos > 0 || count > 128)
        return -EFAULT;
    if (copy_from_user(kbuf, buffer, len))
        return -EFAULT;

    n = simple_strtol(kbuf, NULL, 10);
    if (n == 0)
        fib_flag = 0;
    else
        fib_flag = n;

    return (len);
}

static loff_t fib_proc_lseek(struct file *file, loff_t pos, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = pos;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + pos;
        break;
    case 2: /* SEEK_END: */
        new_pos = 128 - pos;
        break;
    }

    if (new_pos > 128)
        new_pos = 128;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

static struct file_operations fib_proc_ops = {
    .owner = THIS_MODULE,
    .read = fib_proc_read,
    .write = fib_proc_write,
    .llseek = fib_proc_lseek,
};

static int fib_procfs_init(void)
{
    /* add /proc */
    struct proc_dir_entry *proc_entry;
    struct proc_dir_entry *named_dir;

    named_dir = proc_mkdir(PROC_FIBONACCI_BASE_DIR, NULL);
    if (!named_dir) {
        printk(KERN_ERR "Couldn't create dir /proc/%s\n",
               PROC_FIBONACCI_BASE_DIR);
        goto cleanup;
    }

    proc_entry = proc_create("fib_flag", 0666, named_dir, &fib_proc_ops);
    if (proc_entry == NULL) {
        printk(KERN_WARNING "fib: unable to create %s/fib_flag entry\n",
               PROC_FIBONACCI_BASE_DIR);
        goto cleanup;
    }
    return 0;
cleanup:
    remove_proc_subtree(PROC_FIBONACCI_BASE_DIR, NULL);
    return -ENOMEM;
}

static void fib_procfs_exit(void)
{
    // remove_proc_entry(PROC_FIBONACCI_BASE_DIR, NULL);
    remove_proc_subtree(PROC_FIBONACCI_BASE_DIR, NULL);
}

struct BigN {
    u8 val[MAX_DIGITS];
};

static inline void add_BigN(struct BigN *output, struct BigN x, struct BigN y)
{
    int ii = 0;
    u8 carry = 0;
    for (ii = 0; ii < MAX_DIGITS; ii++) {
        u8 tmp = x.val[ii] + y.val[ii] + carry;
        output->val[ii] = tmp % 10;
        carry = 0;
        if (tmp > 9)
            carry = 1;
    }
}

unsigned long long fast_doubling(long long k)
{
    unsigned long long a = 0, b = 1;
    for (int i = 31; i >= 0; i--) {
        unsigned long long t1 = a * (2 * b - a);
        unsigned long long t2 = b * b + a * a;
        a = t1;
        b = t2;
        if (k & (1U << i)) {
            t1 = a + b;
            a = b;
            b = t1;
        }
    }
    return a;
}

/* Compute the Nth Fibonnaci number F_n, where
 * F_0 = 0
 * F_1 = 1
 * F_n = F_{n-1} + F_{n-2} for n >= 2.
 *
 * This is based on the matrix identity:
 *        n
 * [ 0 1 ]  = [ F_{n-1}    F_n   ]
 * [ 1 1 ]    [   F_n    F_{n+1} ]
 *
 * Exponentiation uses binary power algorithm from high bit to low bit.
 */
static void bignum_k_fibonacci(long long n, bn *fib)
{
    if (unlikely(n <= 2)) {
        if (n == 0)
            bn_zero(fib);
        else
            bn_set_u32(fib, 1);
        return;
    }

    bn *a1 = fib; /* Use output param fib as a1 */

    bn_t a0, tmp, a;
    bn_init_u32(a0, 0); /*  a0 = 0 */
    bn_set_u32(a1, 1);  /*  a1 = 1 */
    bn_init(tmp);       /* tmp = 0 */
    bn_init(a);

    /* Start at second-highest bit set. */
    for (uint64_t k = ((uint64_t) 1) << (62 - __builtin_clzll(n)); k; k >>= 1) {
        /* Both ways use two squares, two adds, one multipy and one shift. */
        bn_lshift(a0, 1, a); /* a03 = a0 * 2 */
        bn_add(a, a1, a);    /*   ... + a1 */
        bn_sqr(a1, tmp);     /* tmp = a1^2 */
        bn_sqr(a0, a0);      /* a0 = a0 * a0 */
        bn_add(a0, tmp, a0); /*  ... + a1 * a1 */
        bn_mul(a1, a, a1);   /*  a1 = a1 * a */
        if (k & n) {
            bn_swap(a1, a0);    /*  a1 <-> a0 */
            bn_add(a0, a1, a1); /*  a1 += a0 */
        }
    }
    /* Now a1 (alias of output parameter fib) = F[n] */

    bn_free(a0);
    bn_free(tmp);
    bn_free(a);
}


void bignum_k_fast_doubling(char *buf, size_t size, long long k)
{
    bn_t fib = BN_INITIALIZER;
    bignum_k_fibonacci(k, fib);
    printk(KERN_INFO "dutsai Fib(%u)=", k), bn_print_dec(fib), printk("\n");
    bn_free(fib);
}

static void fib_sequence(char *buf, size_t size, long long k)
{
    /* FIXME: use clz/ctz and fast algorithms to speed up */
    struct BigN *fab =
        (struct BigN *) kmalloc((k + 2) * sizeof(struct BigN), GFP_KERNEL);
    char kbuffer[MAX_DIGITS] = {0};
    int msb_idx;
    if (fab == NULL) {
        printk(KERN_ALERT "kmalloc fail.");
        return;
    }

    memset(&(fab[0].val), 0, MAX_DIGITS);
    memset(&(fab[1].val), 0, MAX_DIGITS);
    fab[1].val[0] = 1;

    for (int i = 2; i <= k; i++) {
        add_BigN(&fab[i], fab[i - 1], fab[i - 2]);
    }
    msb_idx = 0;
    for (int i = MAX_DIGITS - 1; i > 0; i--) {
        if (fab[k].val[i] != 0) {
            msb_idx = i;
            break;
        }
    }
    for (int ii = msb_idx; ii >= 0; ii--) {
        kbuffer[msb_idx - ii] = fab[k].val[ii] + 48;
    }

    printk(KERN_INFO "dutsai: size = %ld, k = %lld, %s", size, k, kbuffer);
    copy_to_user(buf, kbuffer, size);
    kfree(fab);
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    int len;
    char kbuf[MAX_DIGITS] = {0};
    unsigned long long result;
    switch (fib_flag) {
    case 1:
        result = fast_doubling(*offset);
        snprintf(kbuf, MAX_DIGITS, "%llu", result);
        len = copy_to_user(buf, kbuf, MAX_DIGITS);
        break;
    case 2:
        bignum_k_fast_doubling(buf, size, *offset);
        break;
    default:
        fib_sequence(buf, size, *offset);
        len = size;
    }
    return len;
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    return 1;
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    cdev_init(fib_cdev, &fib_fops);
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, "fibonacci_class");

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, "fibonacci_dev")) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }

    if (fib_procfs_init()) {
        printk(KERN_ALERT "Failed to create procfs");
        rc = -5;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
    fib_procfs_exit();
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
