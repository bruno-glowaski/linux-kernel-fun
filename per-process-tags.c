#include "linux/array_size.h"
#include "linux/gfp_types.h"
#include "linux/kern_levels.h"
#include "linux/rculist.h"
#include "linux/rcupdate.h"
#include <asm/current.h>
#include <linux/fs.h>
#include <linux/hashtable.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

MODULE_AUTHOR("Bruno Henrique Glowaski Morais");
MODULE_DESCRIPTION("An experiment on storing independent per-process data "
                   "without modifying the main kernel.");
MODULE_LICENSE("GPL");

#define PROC_NAME "ppt"
#define PROC_MODE 0666

struct ppt_entry {
  pid_t pid;
  char tag[256];
  struct hlist_node next;
};

DEFINE_HASHTABLE(ppt_table, 6);

static ssize_t ppt_write(struct file *file, const char __user *ubuf,
                         size_t count, loff_t *ppos) {
  const pid_t pid = current->pid;
  struct ppt_entry *new_entry = kzalloc(sizeof(struct ppt_entry), GFP_KERNEL);
  struct ppt_entry *current_entry;
  new_entry->pid = pid;
  if (copy_from_user(new_entry->tag, ubuf, count)) {
    return -EFAULT;
  }
  new_entry->tag[255] = '\0';
  hash_for_each_possible_rcu(ppt_table, current_entry, next, pid) {
    if (current_entry->pid == pid) {
      hlist_replace_rcu(&current_entry->next, &new_entry->next);
      synchronize_rcu();
      kfree(current_entry);
      goto done;
    }
  }
  hash_add_rcu(ppt_table, &new_entry->next, pid);
done:
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
  ppt_dentry = proc_create(PROC_NAME, PROC_MODE, NULL, &ppt_fops);
  return 0;
}

static void __exit ppt_exit(void) {
  size_t bkt;
  struct hlist_node *tmp;
  struct ppt_entry *entry;
  proc_remove(ppt_dentry);
  hash_for_each_safe(ppt_table, bkt, tmp, entry, next) {
    hash_del_rcu(&entry->next);
  }
}

module_init(ppt_init);
module_exit(ppt_exit);
