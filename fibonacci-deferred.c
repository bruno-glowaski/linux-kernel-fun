#include "linux/printk.h"
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

MODULE_AUTHOR("Bruno Henrique Glowaski Morais");
MODULE_DESCRIPTION("Experimenting with kernel tasks by creating a deferred "
                   "task to fill a page with fibonacci numbers.");
MODULE_LICENSE("GPL");

struct fibonacci_page {
  atomic_t done;
  uint64_t values[(PAGE_SIZE - sizeof(atomic_t)) / sizeof(uint64_t)];
} __aligned(PAGE_SIZE);

#define DEVICE_NAME "deffib"
#define DEVICE_CLASS "fib"

static dev_t fib_major_number;
static struct class *fib_class = NULL;
static struct device *fib_device = NULL;
static struct cdev fib_cdev;

static long fib_ioctl(struct file *file, unsigned int request,
                      unsigned long param) {
  if (param % PAGE_SIZE != 0) {
    return -EFAULT;
  }
  if (access_ok((void *)param, PAGE_SIZE)) {
    return -EFAULT;
  }
  return 0;
}

static struct file_operations fops = {
    .unlocked_ioctl = fib_ioctl,
};

static int __init fibonacci_init(void) {
  long r;

  r = alloc_chrdev_region(&fib_major_number, 0, 1, DEVICE_NAME);
  if (r < 0) {
    printk(KERN_INFO "Failed to allocate device region...");
    return r;
  }

  fib_class = class_create(DEVICE_CLASS);
  if (IS_ERR(fib_class)) {
    printk(KERN_INFO "Failed to create device class...");
    r = PTR_ERR(fib_class);
    goto error_0;
  }

  fib_device =
      device_create(fib_class, NULL, fib_major_number, NULL, DEVICE_NAME);
  if (IS_ERR(fib_device)) {
    printk(KERN_INFO "Failed to create device...");
    r = PTR_ERR(fib_device);
    goto error_1;
  }

  cdev_init(&fib_cdev, &fops);
  r = cdev_add(&fib_cdev, fib_major_number, 1);
  if (r < 0) {
    printk(KERN_INFO "Failed to add character device...");
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
