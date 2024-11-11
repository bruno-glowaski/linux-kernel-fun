#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include "fibonacci-deferred.h"

MODULE_AUTHOR("Bruno Henrique Glowaski Morais");
MODULE_DESCRIPTION("Experimenting with kernel tasks by creating a deferred "
                   "task to fill a page with fibonacci numbers.");
MODULE_LICENSE("GPL");

#define DEVICE_NAME "deffib"
#define DEVICE_CLASS "fib"

static dev_t fib_major_number;
static struct class *fib_class = NULL;
static struct device *fib_device = NULL;
static struct cdev fib_cdev;

#define THREAD_NAME "deferred_fibonacci"

static int fib_kthread(void __user *data) {
  struct page *page;
  struct fib_page *dst;
  size_t i;
  long r;

  printk(KERN_INFO "Mapping page into kernel space...\n", data, (long)data,
         &page);

  if ((r = get_user_pages((long)data, 1, 1, &page)) < 1) {
    printk(KERN_INFO "Failed to get user pages... Return %li\n", r);
    return -1;
  }
  if ((dst = vmap(&page, 1, VM_WRITE | VM_READ, PAGE_KERNEL)) == NULL) {
    printk(KERN_INFO "Failed to vmap pages...\n");
    return -1;
  }
  printk(KERN_INFO "Values offset %li", offsetof(struct fib_page, values));
  printk(KERN_INFO "Mappped to addresses: Base -> %p; Count -> %p; Values -> "
                   "%p;",
         dst, &dst->count, &dst->values);

  printk(KERN_INFO "Setting initial values...\n");
  atomic_set(&dst->count, 2);
  dst->values[0] = 1;
  dst->values[1] = 1;
  schedule();

  printk(KERN_INFO "Setting other values...\n");
  while ((i = atomic_read(&dst->count)) < FIBONACCI_COUNT(PAGE_SIZE)) {
    printk(KERN_INFO "Writting fib[%li]...\n", i);
    dst->values[i] = dst->values[i - 1] + dst->values[i - 2];
    atomic_fetch_inc_release(&dst->count);
    schedule();
  }

  vunmap(dst);
  unpin_user_page(page);
  return 0;
}

static long fib_ioctl(struct file *file, unsigned int request,
                      unsigned long param) {
  printk(KERN_INFO "Received request with address %p (%li)\n",
         (void __user *)param, param);
  if (param % PAGE_SIZE != 0) {
    printk(KERN_INFO "Error: invalid page aligment\n");
    return -EFAULT;
  }
  if (!access_ok((void __user *)param, PAGE_SIZE)) {
    printk(KERN_INFO "Error: unauthorized memory region\n");
    return -EFAULT;
  }
  kthread_run(fib_kthread, (void __user *)param, THREAD_NAME);
  return 0;
}

static struct file_operations fops = {
    .unlocked_ioctl = fib_ioctl,
};

static int __init fibonacci_init(void) {
  long r;

  r = alloc_chrdev_region(&fib_major_number, 0, 1, DEVICE_NAME);
  if (r < 0) {
    printk(KERN_INFO "Failed to allocate device region...\n");
    return r;
  }

  fib_class = class_create(DEVICE_CLASS);
  if (IS_ERR(fib_class)) {
    printk(KERN_INFO "Failed to create device class...\n");
    r = PTR_ERR(fib_class);
    goto error_0;
  }

  fib_device =
      device_create(fib_class, NULL, fib_major_number, NULL, DEVICE_NAME);
  if (IS_ERR(fib_device)) {
    printk(KERN_INFO "Failed to create device...\n");
    r = PTR_ERR(fib_device);
    goto error_1;
  }

  cdev_init(&fib_cdev, &fops);
  r = cdev_add(&fib_cdev, fib_major_number, 1);
  if (r < 0) {
    printk(KERN_INFO "Failed to add character device...\n");
    goto error_2;
  }

  return 0;

error_2:
  device_destroy(fib_class, fib_major_number);
error_1:
  class_destroy(fib_class);
error_0:
  unregister_chrdev_region(fib_major_number, 1);
  return r;
}
static void __exit fibonacci_exit(void) {
  cdev_del(&fib_cdev);
  device_destroy(fib_class, fib_major_number);
  class_destroy(fib_class);
  unregister_chrdev_region(fib_major_number, 1);
}

module_init(fibonacci_init);
module_exit(fibonacci_exit);
