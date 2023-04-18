#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/kfifo.h>
#include <linux/ioctl.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include "process_ioctl.h"

#define MSG_LEN 		64

#define MAX_USERS 		20

#define FIFO_SIZE 		128

typedef struct pdata {
	int pid;
	int free;
	struct kfifo_rec_ptr_1 fifo;
	spinlock_t slock;
} pdata;

pdata p[MAX_USERS];

static int N = 0;

static long int my_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	int i, ret = -1, idx = -1;
	if(cmd != CONNECT_USER)
	{
		return -1;
	}
	
	for(i = 0; i < N; i++)
	{
		if(p[i].free == 0)
		{
			ret = kfifo_alloc(&p[i].fifo, FIFO_SIZE, GFP_KERNEL);
			if (!ret) 
			{
				spin_lock_init(&p[i].slock);
				p[i].pid = get_current()->pid;
				p[i].free = 1;
				idx = i;
			}
			break;
		}
	}
	if(idx < 0 && N+1 < MAX_USERS) 
	{
		ret = kfifo_alloc(&p[N].fifo, FIFO_SIZE, GFP_KERNEL);
		if (!ret) 
		{
			spin_lock_init(&p[N].slock);
			p[N].pid = get_current()->pid;
			p[N].free = 1;
			idx = N;
			N++;
		}
	}
	
	if(idx >= 0)
	{
		char module_buf[64];
		sprintf(module_buf, "P%d - Joined", idx + 1);
		for(i = 0; i < N; i++)
		{
			if(i != idx && p[i].free == 1)
			{
				kfifo_in_spinlocked(&p[i].fifo, module_buf, sizeof(module_buf), &p[i].slock);
			}
		}
	}
	copy_to_user((int *)arg, &idx, sizeof(int));
	return 1;
}


static int myopen(struct inode *inode, struct file *file)
{
	printk("Device opened by P%d\n", get_current()->pid);
	return 0;
}


static int myclose(struct inode *inodep, struct file *filp) {
	int i, idx = -1;
	char module_buf[64];

	for(i = 0; i < N; i++)
	{
		if(p[i].pid == get_current()->pid)
		{
			kfifo_free(&p[i].fifo);
			p[i].free = 0;
			idx = i;
			sprintf(module_buf, "P%d - Left", idx + 1);
			break;
		}	
	}
	for(i = 0; i < N; i++)
	{
		if(i != idx && p[i].free == 1)
		{
			kfifo_in_spinlocked(&p[i].fifo, module_buf, sizeof(module_buf), &p[i].slock);
		}
	}
	return 0;
}


static ssize_t mywrite(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
	int i, length = -1;
	char arr[MSG_LEN];
	char module_buf[MSG_LEN];
	
	if(copy_from_user(arr, buf, sizeof(arr)))
	{
		return -1;
	}

	length = sprintf(module_buf, "P%d: ", arr[0] + 1);
	
	memcpy(module_buf + length, arr + 1, 64 - length);

	for(i = 0; i < N; i++)
	{
		if(i != arr[0] && p[i].free == 1)
		{
			kfifo_in_spinlocked(&p[i].fifo, module_buf, sizeof(module_buf), &p[i].slock);
		}
	}
	return 1;
}

static ssize_t myread(struct file *file, char __user *buf, size_t len, loff_t *ppos)
{
	int i, ret = 0;
	for(i = 0; i < N; i++)
	{
		if(p[i].pid == get_current()->pid)
		{
			char arr[MSG_LEN];
			int length = kfifo_out_spinlocked(&p[i].fifo, arr, sizeof(arr), &p[i].slock);
			if(length > 0)
			{
				arr[length] = '\0';
				copy_to_user(buf, arr, sizeof(arr));
				ret = length + 1;
			}
			break;
		}
	} 
	return ret; 
}

static const struct file_operations myfops = {
    .owner					= THIS_MODULE,
    .read					= myread,
    .write					= mywrite,
    .open					= myopen,
    .release				= myclose,
    .llseek 				= no_llseek,
	.unlocked_ioctl 		= my_ioctl,
};


struct miscdevice mydevice = 
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = "mihir",
    .fops = &myfops,
    .mode = S_IRUGO | S_IWUGO,
};

static int __init my_init(void)
{
	if (misc_register(&mydevice) != 0) 
	{
		printk("device registration failed\n");
		return -1;
	}


	printk("Character device registered\n");
	return 0;
}

static void __exit my_exit(void)
{
	printk("Character device unregistered\n");
	misc_deregister(&mydevice);
}

module_init(my_init)
module_exit(my_exit)
MODULE_DESCRIPTION("Chat character device\n");
MODULE_AUTHOR("Mihir Dalvi <mdalvi3@binghamton.edu>");
MODULE_LICENSE("GPL");