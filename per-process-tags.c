#include "linux/array_size.h"
#include "linux/gfp_types.h"
#include "linux/kern_levels.h"
#include <asm/current.h>
#include <linux/fs.h>
#include <linux/hashtable.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

MODULE_AUTHOR("Bruno Henrique Glowaski Morais");
MODULE_DESCRIPTION("An experiment on storing independent per-process data "
                   "without modifying the main kernel.");
MODULE_LICENSE("GPL");

struct ppt_entry {
  pid_t pid;
  char tag[256];
  struct hlist_node next;
};

DEFINE_HASHTABLE(ppt_table, 6);

static ssize_t ppt_write(struct file *file, const char __user *ubuf,
                         size_t count, loff_t *ppos) {
  const pid_t pid = current->pid;
  struct ppt_entry *p_entry = kzalloc(sizeof(struct ppt_entry), GFP_KERNEL);
  p_entry->pid = pid;
  if (copy_from_user(p_entry->tag, ubuf, count)) {
    return -EFAULT;
  }
  p_entry->tag[255] = '\0';
  hash_add_rcu(ppt_table, &p_entry->next, pid);
  return count;
}

static ssize_t ppt_read(struct file *file, char __user *ubuf, size_t count,
                        loff_t *ppos) {
  const size_t buffer_size = 2048;
  char *buffer = kmalloc(buffer_size, GFP_KERNEL);
  size_t bkt, length = 0;
  struct ppt_entry *current_entry;
  hash_for_each_rcu(ppt_table, bkt, current_entry, next) {
    if (length >= buffer_size) {
      break;
    }
    length += scnprintf(buffer + length, buffer_size - length, "%i: %s\n",
                        current_entry->pid, current_entry->tag);
  }
  if (*ppos >= length) {
    return 0;
  }
  if (count > length) {
    count = length;
  }
  if (copy_to_user(ubuf, buffer, count)) {
    kfree(buffer);
    return -EFAULT;
  }
  *ppos += count;
  kfree(buffer);
  return count;
}

static struct proc_ops ppt_fops = {
    .proc_write = ppt_write,
    .proc_read = ppt_read,
};
static struct proc_dir_entry *ppt_dentry;

static int __init ppt_init(void) {
  hash_init(ppt_table);
  ppt_dentry = proc_create("ppt", 0666, NULL, &ppt_fops);
  return 0;
}
static void __exit ppt_exit(void) { proc_remove(ppt_dentry); }

module_init(ppt_init);
module_exit(ppt_exit);
