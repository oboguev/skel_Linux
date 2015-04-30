#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/smp.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/rwsem.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/stop_machine.h>

#include "mytest.h"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Sergey Oboguev");
MODULE_DESCRIPTION("My Test Module");

typedef enum { TE_1 = 1, TE_2 = 2} TE;

#define NTIMERS  4
#define NTHREADS 4
#define NDEVICES 2
#define DEVICE_NAME "mytest"

/* module parameters (visible in /sys/modules/mytest/paramaters,
   can also have change callbacks hooked) */
static unsigned int par_a1 = 777;
module_param_named(a1, par_a1, uint, 0644);
MODULE_PARM_DESC(a1, "Description of a1");

static char* par_s1 = "default_s1";
module_param_named(s1, par_s1, charp, 0644);
MODULE_PARM_DESC(s1, "Description of s1");

static struct my_timer_list
{
    struct timer_list   m_tmr;
    unsigned int        m_index;
}
tmr[NTIMERS];

static struct my_thread_struct
{
    struct task_struct*  m_task;
    unsigned int         m_index;
}
kthreads[NTHREADS];

struct mytest_dev
{
    struct cdev          cdev;
    bool                 cdev_added;
    struct device*       cdev_device;
    dev_t                devno;
    struct rw_semaphore  rwsem;
    char*                buffer;
    size_t               buffer_size;
    wait_queue_head_t    out_wait_q;
};

static dev_t my_cdev_devno = 0;
static unsigned int my_cdev_major = 0;
static unsigned int my_cdev_minor = 0;
static struct class* my_cdev_class = NULL;
static struct mytest_dev mytest_devs[NDEVICES];
static const size_t my_cdev_maxsize = 8 * 1024 * 1024;

static int my_cdev_open(struct inode *, struct file *);
static int my_cdev_release(struct inode *, struct file *);
static ssize_t my_cdev_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t my_cdev_write(struct file *, const char __user *, size_t, loff_t *);
static loff_t my_cdev_llseek(struct file *, loff_t, int);
static unsigned int my_cdev_poll(struct file *, struct poll_table_struct *);
static long my_cdev_unlocked_ioctl(struct file *, unsigned int, unsigned long);
static long my_cdev_compat_ioctl(struct file *, unsigned int, unsigned long);

static struct file_operations my_cdev_ops =
{
    .owner           =  THIS_MODULE,
    .open            =  my_cdev_open,
    .release         =  my_cdev_release,
    .read            =  my_cdev_read,
    .write           =  my_cdev_write,
    .llseek          =  my_cdev_llseek,
    .poll            =  my_cdev_poll,
    .unlocked_ioctl  =  my_cdev_unlocked_ioctl,
    .compat_ioctl    =  my_cdev_compat_ioctl,
};

static void my_smp_function(void* arg);
static int my_stop_machine_function(void* arg);
static void release_all(void);
static void my_callout(unsigned long arg);
static int my_thread_func(void* data);
static void display_prio(struct my_thread_struct*);
static void call_fs_sync(void);

static int mytest_init(void)
{
    int error;
    int k;

    printk(KERN_ALERT "Loading mytest ...\n");
    printk(KERN_ALERT "mytest: enum = %d bytes\n", (int) sizeof(TE));               // 4 on x86/x64
    printk(KERN_ALERT "mytest: short = %d bytes\n", (int) sizeof(short));           // 2
    printk(KERN_ALERT "mytest: int = %d bytes\n", (int) sizeof(int));               // 4
    printk(KERN_ALERT "mytest: long = %d bytes\n", (int) sizeof(long));             // 4 (x86) or 8 (x64)
    printk(KERN_ALERT "mytest: long long = %d bytes\n", (int) sizeof(long long));   // 8

#ifdef CONFIG_X86
    printk(KERN_ALERT "mytest: CONFIG_X86 is defined\n");
#else
    printk(KERN_ALERT "mytest: CONFIG_X86 is not defined\n");
#endif

#ifdef CONFIG_X86_64
    printk(KERN_ALERT "mytest: CONFIG_X86_64 is defined\n");
#else
    printk(KERN_ALERT "mytest: CONFIG_X86_64 is not defined\n");
#endif

    printk(KERN_ALERT "mytest: PAGE_SIZE=%ld\n", PAGE_SIZE);    // x64: 4096
    printk(KERN_ALERT "mytest: PAGE_SHIFT=%d\n", PAGE_SHIFT);   // x64: 12

    printk(KERN_ALERT "mytest: parameter a1=%u\n", par_a1);
    printk(KERN_ALERT "mytest: parameter s1=%s\n", par_s1);

    /*
     *
     */
    preempt_disable();
    printk(KERN_ALERT "mytest: issuing MP calls from CPU %d ...\n", smp_processor_id());
    on_each_cpu_mask(cpu_online_mask, my_smp_function, "test message", true);
    preempt_enable();
    printk(KERN_ALERT "mytest: MP calls completed\n");

    /*
     * my_stop_machine_function() will be called only on one CPU, with irqs disabled,
     * but other cpus will be blocked at the time for the duration of the call
     */
    printk(KERN_ALERT "mytest: issuing stop-machine calls from CPU %d ...\n", smp_processor_id());
    if (stop_machine(my_stop_machine_function, "my stop-machine-message", NULL))
        printk(KERN_ALERT "mytest: stop-machine calls failed\n");
    else
        printk(KERN_ALERT "mytest: stop-machine calls completed\n");

    for (k = 0;  k < NDEVICES;  k++)
    {
        struct mytest_dev* dev = &mytest_devs[k];
        dev->cdev_added = false;
        dev->cdev_device = NULL;
        dev->buffer = NULL;
        dev->buffer_size = 0;
        init_waitqueue_head(&dev->out_wait_q);
    }

    for (k = 0;  k < NTIMERS;  k++)
    {
        struct my_timer_list* t = &tmr[k];
        init_timer(&t->m_tmr);
        t->m_index = k;
        t->m_tmr.function =  my_callout;
        t->m_tmr.data = (unsigned long) t;
        mod_timer(& t->m_tmr, jiffies + msecs_to_jiffies(8 * 1000));
    }

    for (k = 0;  k < NTHREADS;  k++)
    {
        struct my_thread_struct* t = &kthreads[k];
        t->m_task = NULL;
        t->m_index = k;
    }

    for (k = 0;  k < NTHREADS;  k++)
    {
        struct my_thread_struct* t = &kthreads[k];
        struct task_struct* task = kthread_run(my_thread_func, t, "mytest/%d", k);
        if (IS_ERR(task))
            printk(KERN_ALERT "mytest: Failed to create thread.\n");
        else
            t->m_task = task;
    }

    error = alloc_chrdev_region(&my_cdev_devno, 0, NDEVICES, DEVICE_NAME);
    if (error)  
    {
        my_cdev_devno = 0;
        printk(KERN_ALERT "mytest: Unable to allocate device number.\n");
        release_all();
        return error;
    }
    my_cdev_major = MAJOR(my_cdev_devno);
    my_cdev_minor = MINOR(my_cdev_devno);

    my_cdev_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(my_cdev_class))
    {
        error = PTR_ERR(my_cdev_class);
        my_cdev_class = NULL;
        printk(KERN_ALERT "mytest: Unable to create class.\n");
        release_all();
        return error;
    }

    for (k = 0;  k < NDEVICES;  k++)
    {
        struct mytest_dev* dev = &mytest_devs[k];
        struct cdev* cdev = &dev->cdev;

        dev->devno = MKDEV(my_cdev_major, my_cdev_minor + k);
        init_rwsem(& dev->rwsem);

        cdev_init(cdev, &my_cdev_ops);
        cdev->owner = THIS_MODULE;
        init_waitqueue_head(&dev->out_wait_q);

        error = cdev_add(cdev, dev->devno, 1);
        if (error)
        {
            printk(KERN_ALERT "mytest: Unable to add device.\n");
            release_all();
            return error;
        }
        else
        {
            dev->cdev_added = true;
        }

        dev->cdev_device = device_create(my_cdev_class, NULL, dev->devno, NULL, DEVICE_NAME "%d", k);
        if (IS_ERR(dev->cdev_device))
        {
            error = PTR_ERR(dev->cdev_device);
            dev->cdev_device = NULL;
            printk(KERN_ALERT "mytest: Unable to create device.\n");
            release_all();
            return error;
        }
    }

    printk(KERN_ALERT "mytest: Finished loading.\n");

    return 0;
}

static void mytest_exit(void)
{
    release_all();
    printk(KERN_ALERT "Unloading mytest ...\n");
}

module_init(mytest_init);
module_exit(mytest_exit);

/*
 * Executed on each CPU with IRQs disabled.
 * On CPUs other than invoking on_each_cpu_mask(), executed in IPI context.
 * On CPUs invoking on_each_cpu_mask(), executed in non-IRQ context.
 */
static void my_smp_function(void* arg)
{
    printk(KERN_ALERT "mytest: MP call on cpu %d: %s (in_interrupt=%lx, in_irq=%lx, preempt_count=%x, irqs_disabled=%x)\n", 
           smp_processor_id(), (const char*) arg, in_interrupt(), in_irq(), preempt_count(), irqs_disabled());
}

/*
 * Executed on one CPU with IRQs disabled.
 */
static int my_stop_machine_function(void* arg)
{
    printk(KERN_ALERT "mytest: Stop Machine call on cpu %d: %s (in_interrupt=%lx, in_irq=%lx, preempt_count=%x, irqs_disabled=%x)\n", 
           smp_processor_id(), (const char*) arg, in_interrupt(), in_irq(), preempt_count(), irqs_disabled());
    return 0;
}

static void release_all(void)
{
    int k;

    /*
     * If this is a normal (not forced) unloading or failure during initial 
     * loading, then by the time we get called all references to devices should
     * be gone. This is ensured by device references causing to increment 
     * reference count on the module.
     *
     * Module unloader (delete_module) waits till dynamic reference count on 
     * the module drops to zero and refuses to unload module otherwise
     * (except for force unloading, then all bets are off).
     *
     * Reference count on the module is incremented and decremented by calls
     * such as fops_get/fops_put, cdev_get/cdev_put and anon_inode_getfile,
     * which in turn call module_get/module_put.
     *
     * Specifically, 
     *     sys_open -> ... -> finish_open -> do_dentry_open -> chrdev_open.
     *
     * chrdev_open either calls cdev_get directrly or calls
     * kobj_lookup -> "lock" = exact_lock -> cdev_get.
     *
     * In addition, chrdev_open cals calls fops_get.
     *
     * During module unloading, delete_module calls either stop_machine or
     * synchronize_sched to disable acquisiton of references (such as opening 
     * devices) while module is being unloaded.
     */
    for (k = 0;  k < NDEVICES;  k++)
    {
        struct mytest_dev* dev = &mytest_devs[k];
        struct cdev* cdev = &dev->cdev;

        if (dev->cdev_device)
        {
            device_destroy(my_cdev_class, dev->devno);
            dev->cdev_device = NULL;
        }

        if (dev->cdev_added)
        {
            cdev_del(cdev);
            dev->cdev_added = false;
        }

        /*
         * This wake up does not have any effect during normal module unloading,
         * since by that time all references to the device should be gone. 
         * It can be of some use in case of force-unloading (all bets are off then, 
         * of course) or if this code snippet is used in other contexts.
         */
        wake_up_all(&dev->out_wait_q);

        if (dev->buffer)
        {
            kfree(dev->buffer);
            dev->buffer = NULL;
        }
        dev->buffer_size = 0;
    }

    if (my_cdev_class)
    {
        class_destroy(my_cdev_class);
        my_cdev_class = NULL;
    }

    if (my_cdev_devno)
    {
        unregister_chrdev_region(my_cdev_devno, NDEVICES);
        my_cdev_devno = 0;
    }

    printk(KERN_ALERT "mytest: Stopping timers...\n");
    for (k = 0;  k < NTIMERS;  k++)
    {
        struct my_timer_list* t = &tmr[k];
        del_timer_sync(& t->m_tmr);
    }

    printk(KERN_ALERT "mytest: Stopping threads...\n");
    for (k = 0;  k < NTHREADS;  k++)
    {
        struct my_thread_struct* t = &kthreads[k];
        if (t->m_task)
            kthread_stop(t->m_task);
    }
    printk(KERN_ALERT "mytest: Stopped threads\n");
}

static void my_callout(unsigned long arg)
{
    struct my_timer_list* t;

    BUILD_BUG_ON(sizeof(unsigned long) < sizeof(void*));

    t = (struct my_timer_list*) arg;
    printk(KERN_ALERT "mytest: timer %d (cpu %d, in_interrupt=%lx, in_irq=%lx, in_softirq=%lx, in_serving_softirq=%lx, preempt_count=%x)\n", 
           t->m_index, smp_processor_id(), in_interrupt(), in_irq(), in_softirq(), in_serving_softirq(), preempt_count());
    mod_timer(& t->m_tmr, jiffies + msecs_to_jiffies(8 * 1000));
}

static int my_thread_func(void* data)
{
    struct my_thread_struct* t = (struct my_thread_struct*) data;
    // int old_nice;
    // int new_nice;

    printk(KERN_ALERT "mytest: thread %d\n", t->m_index);

    // old_nice = task_nice(current);
    // set_user_nice(current, -10)
    // new_nice = task_nice(current);
    // printk(KERN_ALERT "mytest: thread %d (nice: %d -> %d)\n", t->m_index, old_nice, new_nice);

    if (t->m_index == 0)
    {
        struct sched_param sp;
        int err;
        display_prio(t);
        sp.sched_priority = MAX_RT_PRIO - 1;
        err = sched_setscheduler(current, SCHED_RR, &sp);
        if (err)
            printk(KERN_ALERT "mytest: failed to set thread priority, error: %d\n", err);
        display_prio(t);
    }

    for (;;)
    {
        schedule_timeout_killable(8 * HZ);
        if (kthread_should_stop())
        {
            printk(KERN_ALERT "mytest: thread %d exiting...\n", t->m_index);
            return 0;
        }
        printk(KERN_ALERT "mytest: thread %d\n", t->m_index);
    }

    // do_exit();
    // return 0;
}

static void display_prio(struct my_thread_struct* t)
{
    switch (current->policy)
    {
    case SCHED_NORMAL:
        printk(KERN_ALERT "mytest: thread %d (SCHED_NORMAL, nice=%d)\n", t->m_index, task_nice(current));
        break;

    case SCHED_FIFO:
        printk(KERN_ALERT "mytest: thread %d (SCHED_FIFO, rt_prio=%u)\n", t->m_index, current->rt_priority);
        break;

    case SCHED_RR:
        printk(KERN_ALERT "mytest: thread %d (SCHED_RR, rt_prio=%u)\n", t->m_index, current->rt_priority);
        break;

    case SCHED_BATCH:
        printk(KERN_ALERT "mytest: thread %d (SCHED_BATCH, nice=%d)\n", t->m_index, task_nice(current));
        break;

    case SCHED_IDLE:
        printk(KERN_ALERT "mytest: thread %d (SCHED_IDLE, nice=%d)\n", t->m_index, task_nice(current));
        break;

    default:
        printk(KERN_ALERT "mytest: thread %d (SCHED class %u)\n", t->m_index, current->policy);
        break;
    }
}

static int my_cdev_open(struct inode* inode, struct file* filp)
{
    unsigned int mj = imajor(inode);
    unsigned int mn = iminor(inode);
    struct mytest_dev* dev;
    
    if (mj != my_cdev_major || mn < my_cdev_minor || mn >= my_cdev_minor + NDEVICES)
        return -ENODEV;
    
    /* store a pointer to struct mytest_dev here for other methods */
    dev = &mytest_devs[mn - my_cdev_minor];
    filp->private_data = dev; 
    
    if (inode->i_cdev != &dev->cdev)
        return -ENODEV;
    
    /* may want to initialized dev here on first or next opening */

    return 0;
}

static int my_cdev_release(struct inode* inode, struct file* filp)
{
    return 0;
}

/* loff_t is long long */
/* size_t is ulong */
static ssize_t my_cdev_write(struct file* filp, const char __user* buf, size_t count, loff_t* fpos)
{

    struct mytest_dev *dev = (struct mytest_dev*) filp->private_data;
    char* p;

    /* As of 3.8.x Linux does not have killable/interruptible rw lock */
    /* If it had and we got interrupted, should return -EINTR */
    down_write(& dev->rwsem);

    if (dev->buffer_size >= my_cdev_maxsize)
        count = 0;
    else
        count = min(count, my_cdev_maxsize - dev->buffer_size);

    if (count == 0)
    {
        up_write(& dev->rwsem);
        return 0;
    }

    /* write to our device is append-only, so we ignore the value of *fpos on input */

    if (dev->buffer == NULL)
    {
        p = kmalloc(count, GFP_KERNEL);
    }
    else
    {
        p = kmalloc(dev->buffer_size + count, GFP_KERNEL);
        if (p) memcpy(p, dev->buffer, dev->buffer_size);
    }

    if (p == NULL)
    {
        up_write(& dev->rwsem);
        return -ENOMEM;
    }

    if (copy_from_user(p + dev->buffer_size, buf, count))
    {
        if (p != dev->buffer)
            kfree(p);
        up_write(& dev->rwsem);
        return -EFAULT;
    }

    if (p != dev->buffer)
    {
        if (dev->buffer);
            kfree(dev->buffer);
        dev->buffer = p;
    }

    dev->buffer_size += count;
    *fpos += count;

    wake_up_interruptible(&dev->out_wait_q);
        
    up_write(& dev->rwsem);

    return count;
}

static ssize_t my_cdev_read(struct file* filp, char __user* buf, size_t count, loff_t* fpos)
{
    struct mytest_dev *dev = (struct mytest_dev*) filp->private_data;
    loff_t offset;

    /* As of 3.8.x Linux does not have killable/interruptible rw lock */
    /* If it had and we got interrupted, should return -EINTR */
    down_read(& dev->rwsem);

    offset = *fpos;
    if (offset < 0)
    {
        up_read(& dev->rwsem);
        return -EINVAL;
    }
    if (offset >= dev->buffer_size)
    {
        up_read(& dev->rwsem);
        return 0;
    }

    count = min(count, dev->buffer_size - (size_t) offset);
    if (count == 0)
    {
        up_read(& dev->rwsem);
        return 0;
    }

    if (copy_to_user(buf, dev->buffer + offset, count))
    {
        up_read(& dev->rwsem);
        return -EFAULT;
    }
    
    *fpos += count;

    up_read(& dev->rwsem);

    return count;
}

static loff_t my_cdev_llseek(struct file* filp, loff_t offset, int whence)
{
    struct mytest_dev *dev = (struct mytest_dev*) filp->private_data;
    loff_t newpos = 0;
    
    /* As of 3.8.x Linux does not have killable/interruptible rw lock */
    /* If it had and we got interrupted, should return -EINTR */
    down_read(& dev->rwsem);

    switch(whence)
    {
    case SEEK_SET:
        newpos = offset;
        break;

    case SEEK_CUR:
        newpos = filp->f_pos + offset;
        break;

    case SEEK_END:
        newpos = dev->buffer_size + offset;
        break;

    default:
        up_read(& dev->rwsem);
        return -EINVAL;
    }

    if (newpos < 0 || newpos > dev->buffer_size) 
        return -EINVAL;
    
    up_read(& dev->rwsem);

    filp->f_pos = newpos;
    return newpos;
}

static unsigned int my_cdev_poll(struct file* filp, struct poll_table_struct* wait)
{
    struct mytest_dev *dev = (struct mytest_dev*) filp->private_data;
    unsigned int mask = 0;

    down_read(& dev->rwsem);

    poll_wait(filp, &dev->out_wait_q, wait);

    if (filp->f_pos < dev->buffer_size)
        mask |= POLLIN | POLLRDNORM;

    if (dev->buffer_size < my_cdev_maxsize)
        mask |= POLLOUT | POLLWRNORM;

    up_read(& dev->rwsem);

    return mask;
}

/*
 * New ioctl interface as of 2.6.11.
 * BKL is not taken prior to the call.
 * inode is available as filp->f_dentry->d_inode.
 */
static long my_cdev_unlocked_ioctl(struct file* filp, unsigned int cmd, unsigned long arg)
{
    // struct mytest_dev *dev = (struct mytest_dev*) filp->private_data;
    long error;

    if (cmd == IOC_MYTEST_PRINT)
    {
        char buf[135];
        error = strncpy_from_user(buf, (const char __user*) arg, sizeof(buf));
        if (error < 0)  return error;
        buf[sizeof(buf) - 1] = 0;
        printk(KERN_ALERT "mytest user print: [%s]\n", buf);
        return 0;
    }
    else if (cmd == IOC_MYTEST_PANIC)
    {
        char buf[135];
        if (!capable(CAP_SYS_ADMIN))
            return -EPERM;
        error = strncpy_from_user(buf, (const char __user*) arg, sizeof(buf));
        if (error < 0)  return error;
        buf[sizeof(buf) - 1] = 0;
        call_fs_sync(); call_fs_sync(); call_fs_sync();
        panic("mytest user panic: [%s]", buf);
        return 0;
    }
    else if (cmd == IOC_MYTEST_OOPS)
    {
        static volatile int res;
        static volatile int zero = 0;

        if (!capable(CAP_SYS_ADMIN))
            return -EPERM;

        printk(KERN_ALERT "mytest: about do to oops ...\n");
        call_fs_sync(); call_fs_sync(); call_fs_sync();
        res = 100 / zero;
        printk(KERN_ALERT "mytest: completed oops.\n");
        return 0;
    }
    else
    {
        return -EINVAL;
    }
}

/*
 * Used by 32-bit processes on 64-bit systems.
 * Should provide conversion of arguments if required.
 * BKL is not taken prior to the call.
 * inode is available as filp->f_dentry->d_inode.
 */
static long my_cdev_compat_ioctl(struct file* filp, unsigned int cmd, unsigned long arg)
{
    return my_cdev_unlocked_ioctl(filp, cmd, arg);
}

static void call_fs_sync(void)
{
    // sys_sync, emergency_sync and syscall table are not exported from kernel

    // emergency_sync();
    // set_current_state(TASK_UNINTERRUPTIBLE);
    // schedule_timeout(HZ / 10);
}
