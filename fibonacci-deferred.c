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

struct fib_task {
  struct page *page;
  struct fib_page *dst;
};

static int fib_kthread(void *data) {
  struct fib_task *task = (struct fib_task *)data;
  struct page *page = task->page;
  struct fib_page *dst = task->dst;
  size_t i;

  printk(KERN_INFO "Deallocating task struct...\n");
  kfree(task);

  printk(KERN_INFO "Mappped to addresses: Base -> %px; Count -> %px; Values -> "
                   "%px;\n",
         dst, &dst->count, &dst->values);

  printk(KERN_INFO "Setting initial values...\n");
  atomic_set(&dst->count, 2);
  printk(KERN_INFO "Count set!\n");
  dst->values[0] = 1;
  printk(KERN_INFO "fib[0] set!\n");
  dst->values[1] = 1;
  printk(KERN_INFO "fib[1] set!\n");
  printk(KERN_INFO "Scheduling...\n");
  schedule();

  printk(KERN_INFO "Setting other values...\n");
  while ((i = atomic_read(&dst->count)) < FIBONACCI_COUNT(PAGE_SIZE)) {
    printk(KERN_INFO "Writting fib[%li]...\n", i);
    dst->values[i] = dst->values[i - 1] + dst->values[i - 2];
    atomic_fetch_inc_release(&dst->count);
    printk(KERN_INFO "Scheduling...\n");
    schedule();
  }

  vunmap(dst);
  unpin_user_page(page);
  return 0;
}
static long fib_ioctl(struct file *file, unsigned int request,
                      unsigned long param) {
  void __user *usr_addr = (void __user *)param;
  struct fib_task *task;
  long r;

  printk(KERN_INFO "Received request with address %px.\n", usr_addr);
  if (param % PAGE_SIZE != 0) {
    printk(KERN_INFO "Error: invalid page aligment\n");
    return -EINVAL;
  }

  if (!access_ok(usr_addr, PAGE_SIZE)) {
    printk(KERN_INFO "Error: unauthorized memory region\n");
    return -EACCES;
  }

  printk(KERN_INFO "Allocating task struct...\n");
  if ((task = kmalloc(sizeof(struct fib_task), GFP_KERNEL)) == NULL) {
    printk(KERN_INFO "Failed to allocate task struct...\n");
    return -ENOMEM;
  }

  printk(KERN_INFO "Mapping page at %px into kernel space...\n", usr_addr);
  if ((r = pin_user_pages_fast(param, 1, FOLL_LONGTERM | FOLL_WRITE,
                               &task->page)) < 1) {
    printk(KERN_INFO "Failed to get user pages... Return %li\n", r);
    goto error_with_alloc;
  }

  if ((task->dst = vmap(&task->page, 1, VM_WRITE | VM_READ, PAGE_SHARED)) ==
      NULL) {
    printk(KERN_INFO "Failed to vmap pages...\n");
    r = -ENOMEM;
    goto error_with_pinned_page;
  }

  kthread_run(fib_kthread, task, THREAD_NAME);

  return 0;

error_with_pinned_page:
  unpin_user_page(task->page);
error_with_alloc:
  kfree(task);
  return r;
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
