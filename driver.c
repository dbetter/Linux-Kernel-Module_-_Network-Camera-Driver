/*
 * Owner: Daniel Better
 *
 * driver.c:
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>	/* for put_user */
#include <asm/spinlock.h>	/* for read/write spinlock */
#include <linux/spinlock.h>	/* for read/write spinlock */
#include <linux/spinlock_types.h>
#include <linux/ioctl.h>	/* for ioctl */
#include <linux/slab.h>
#include <linux/timer.h>	/* for timers */
#include <linux/wait.h>		/* for wait queue */
#include <linux/sched.h>
#include <linux/delay.h>
#include <asm/delay.h>
#include "common.h"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Network Camera Driver");
MODULE_AUTHOR("Daniel Better");


/* Prototypes */
int init_module(void);
void cleanup_module(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);
long device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param);




#define SUCCESS 0
#define MAX_FRAME 40

static int Major;			/* Major number assigned to our device driver */
static int Device_Open = 0;	/* Is device open?  
				 			* Used to prevent multiple access to device */
static int currFrameCount = 0, totalFrameCount = 0;
static int pic_width = 0;
static int pic_height = 0;
static int frameToSend = 1;
static int framesFinished = 0; 
static int isGrabberWorking = 0;
static int count = 0;
static DEFINE_SPINLOCK(mr_lock);
unsigned long flags;

//wait queues definition 
wait_queue_head_t read_queue;
wait_queue_head_t write_queue;
DECLARE_WAIT_QUEUE_HEAD (read_queue);
DECLARE_WAIT_QUEUE_HEAD (write_queue);



struct file_operations fops = 
{
	.read = device_read,
	.write = device_write,
	.unlocked_ioctl = device_ioctl,
	.open = device_open,
	.release = device_release
};


typedef struct Frame
{
	void* framePtr;
	int frameNumber;
	struct list_head list;
}Frame;


Frame frameList;



/*
 * This function is called when the module is loaded
 */
int init_module(void)
{
	INIT_LIST_HEAD(&frameList.list);
        Major = register_chrdev(0, DEVICE_FILE_NAME, &fops);

	if (Major < 0)
	{
	  printk(KERN_ALERT "Registering char device failed with %d\n", Major);
	  return Major;
	}

	printk(KERN_INFO "I was assigned major number %d. \n", Major);
	printk(KERN_INFO "To talk to the driver, create a dev file with\n");
	printk(KERN_INFO "'mknod %s c %d 0'.\n", DEVICE_FILE_NAME, Major);
	printk(KERN_INFO "Try various minor numbers.\n");
	printk(KERN_INFO "Remove the device file and module when done.\n");

	return SUCCESS;
}




/*
 * This function is called when the module is unloaded
 */
void cleanup_module(void)
{
	struct Frame *aFrame, *tmp;
	int sumFreed = 0;

    //Unregister the device 
	unregister_chrdev(Major, DEVICE_FILE_NAME);
	 
    //free the frames in frameList and delete the nodes 
	list_for_each_entry_safe(aFrame, tmp, &frameList.list, list)
	{
         	printk(KERN_INFO "freeing node %d\n", aFrame->frameNumber);
		kfree(aFrame->framePtr);
	        list_del(&aFrame->list);
		sumFreed++;
	}
	printk(KERN_INFO "device_read was used %d times\n", count);
	printk(KERN_INFO "overall freed  %d nodes\n", sumFreed);
}




/*
 * Called when a process tries to open the device file
 */
static int device_open(struct inode *inode, struct file *file)
{
	static int counter = 0;
	Device_Open++;
	printk(KERN_INFO "I've already been used %d times!\n", counter++);
	return SUCCESS;
}


/*
 * Called when a process closes the device file. 
 */
static int device_release(struct inode *inode, struct file *file)
{
	Device_Open--;
	return 0;
}


/*
 * Called when a process tries to read from the device file. 
 */
static ssize_t device_read(struct file *filp,	
			   char *buffer,	/* buffer to fill with data */
			   size_t length,	/* length of the buffer     */
			   loff_t * offset)
{

	struct Frame *headFrame;
	

	if (currFrameCount > 0)
	{
	    //lock-down device
		sleep_on_timeout(&read_queue, (HZ/25)); //(HZ/25) for PAL frame-rate display

		isGrabberWorking = 1;
		spin_lock_irqsave(&mr_lock, flags);

		headFrame = list_entry(frameList.list.next, struct Frame, list);

		if (headFrame->frameNumber == frameToSend)
		{
			copy_to_user(buffer, headFrame->framePtr, length); 
			kfree(headFrame->framePtr);
			list_del(frameList.list.next);
			frameToSend++;
			currFrameCount--;
			count++;		
		}

	    //unlock-device
		spin_unlock_irqrestore(&mr_lock, flags);
		wake_up(&write_queue);

		return (pic_width*pic_height*3);
	}

	else return 0;
}



/*
 * Called when a process tries to write to the device file. 
 */
static ssize_t device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{

	void *aNewFrame;
	Frame *pFrame;

	wait_event(write_queue, (currFrameCount < MAX_FRAME));
   //lock device
	spin_lock_irqsave(&mr_lock, flags);

   //allocate space for a new struct frame object
	pFrame = (Frame*)kmalloc(sizeof (struct Frame), GFP_KERNEL);

   //allocate space for the frame to be copied from user-space -> kernel space
	aNewFrame = kmalloc((pic_width*pic_height)*3, GFP_KERNEL);

   //copy frame using copy_from_user
	copy_from_user(aNewFrame, buff, ((pic_width*pic_height)*3)); 
	pFrame->framePtr = aNewFrame;
	currFrameCount++;
	totalFrameCount++;
	pFrame->frameNumber = totalFrameCount;
	list_add_tail(&(pFrame->list), &(frameList.list));

    //unlock-device
	spin_unlock_irqrestore(&mr_lock, flags);

	return ( (pic_width*pic_height)*3);
}


/*
 * Called when a process tries to communicate with the device file. 
 */
long device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) 
{
	switch (ioctl_num)
	{
		case IOCTL_SET_PIC_WIDTH:
		{
			copy_from_user(&pic_width, (int *)ioctl_param, 4); 
			printk(KERN_ALERT "width recieved is %d\n", pic_width);
			return 1;
		}
		break;

		case IOCTL_SET_PIC_HEIGHT:
		{
			get_user(pic_height, (int *)ioctl_param);
			printk(KERN_ALERT "height recieved is %d\n", pic_height);
			return 1;
		}
		break;

		case IOCTL_SEND_PIC_WIDTH:
		{
			put_user(pic_width, (int*)ioctl_param);
			return 1;
		}
		break;

		case IOCTL_SEND_PIC_HEIGHT:
		{
			put_user(pic_height, (int*)ioctl_param);
			return 1;
		}
		break;
		
		case IOCTL_SET_END_OF_FRAMES:
		{
			get_user(framesFinished, (int *)ioctl_param); 
			return 1;
		}
		break;
		
		case IOCTL_SEND_END_OF_FRAMES:
		{
			if ((list_empty(&frameList.list) != 0) && (framesFinished != 0)) //meaning list is empty
			{
				printk (KERN_INFO "framesFinished is finished: %d\n", framesFinished);			
				put_user(0, (int*)ioctl_param); 
				return 1;
			}
			else
			{
				put_user(1, (int*)ioctl_param);
				return 1;
			}

		}
	}
	return SUCCESS; //return 0 
}

















