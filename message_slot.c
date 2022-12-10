#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include <linux/kernel.h>  
#include <linux/module.h> 
#include <linux/fs.h>      
#include <linux/uaccess.h> 
#include <linux/string.h>   
#include <linux/slab.h>        
#include <linux/errno.h>  
#include <linux/ioctl.h>
#include "message_slot.h"

MODULE_LICENSE("GPL");

//================== MAIN DATA STRUCTURE ===========================

static Channel_List *minors[256] = {NULL};

//================== STRUCT FUNCTIONS ===========================

Channel *channel_Lookup(unsigned int minor, unsigned int c_id) {
    Channel *tmp;
    if (minor < 256) {
        tmp = minors[minor]->head;
        while (tmp != NULL) {
            if (tmp->id == c_id) {
                return tmp;
            }
            tmp = tmp->next;
        }
    }
    return NULL;
}

Channel *channel_init(unsigned int c_id) {
    Channel *new_ch = kmalloc(sizeof(Channel), GFP_KERNEL);
    if (!new_ch){
        return NULL;
    }
    new_ch->next = NULL;
    new_ch->msg_len=0;
    new_ch->id = c_id;
    return new_ch;
}

Channel *add_new_Channel(unsigned int minor, unsigned int c_id) {
    Channel *new_ch = channel_init(c_id);
    Channel *tmp;

    if (!new_ch)
    {
        return NULL;
    }
    tmp = minors[minor]->tail;
    minors[minor]->tail = new_ch;
    tmp->next = new_ch;
    return new_ch;
    
}

//================== DEVICE FUNCTIONS ===========================

static int device_open(struct inode *inode, struct file *file) {
    Channel_List *new_channel_list;
    Channel *first_channel;
    unsigned long minor = iminor(inode);
    printk("Invoking device_open\n");
    // if the device is open already
    if (minors[minor] != NULL){ 
        return 0;
    }
    else{
        new_channel_list = kmalloc(sizeof(Channel_List), GFP_KERNEL);
        if (new_channel_list) {
            first_channel = channel_init(0);
            if (first_channel) {
                new_channel_list->head = first_channel;
                new_channel_list->tail = first_channel;
                minors[minor] = new_channel_list;
                file->private_data = (void *) 0; 
                return 0;
            }
        }
    }
    return -ENOSPC;
}

static ssize_t device_read(struct file *file,
                           char __user * buffer,size_t length, loff_t* offset ){
    Channel *channel;
    unsigned int minor = iminor(file->f_path.dentry->d_inode);
    unsigned long c_id = (unsigned long)file->private_data;
    int i;
    printk( "Invoking device_read\n");
    if (c_id==0){
        return -EINVAL;
    }
    channel = channel_Lookup(minor, c_id);
    if (!channel){
        return -EINVAL;
    }
    if (channel->msg_len == 0){
        return -EWOULDBLOCK;
    }
    if (channel->msg_len > length){
        return -ENOSPC;
    }
    for( i = 0; i < channel->msg_len ; i++){ 
        // write to buffer
        if(put_user(channel->msg_content[i],&buffer[i])!=0){
            return -EINVAL;
        }
    }
    return channel->msg_len;
}


static ssize_t dev_write(struct file *file, const char __user* buffer,size_t length,loff_t* offset){
    int i;
    Channel *channel_w;
    char cur_buffer[BUFFER_LEN];
    unsigned int minor_number;
    unsigned long channel_id;
    printk("Invoking dev_write\n");
    channel_id = (unsigned long)file->private_data;
    if(channel_id == 0)
    {
        return -EINVAL;
    }
    if(length > BUFFER_LEN)
    {
        return -EMSGSIZE;
    }
    if(length == 0)
    { 
        return -EMSGSIZE;
    }
    minor_number = iminor(file->f_path.dentry->d_inode);
    channel_w = channel_Lookup(minor_number, channel_id);
    // if not exist
    if(!channel_w)
    { 
        return -EINVAL;
    }
    for(i = 0; i < length ;i++)
    {
        // if not valid
        if(get_user(cur_buffer[i],&buffer[i])!=0)
        {
            return -EINVAL;
        }
    }
    // update only if valid
    for(i = 0; i < length ; i++)
    {
        channel_w->msg_content[i]= cur_buffer[i]; 
    }
    channel_w->msg_len = length; //updated the length

    return length; // if success return the written msg length
}

static long dev_ioctl(struct file *file, unsigned int ioctl_command_id, unsigned long ioctl_param) {
    Channel* ioctl_c;
    Channel* new_c;
    unsigned int minor_number;
    printk("Invoking dev_ioctl\n");
    if((ioctl_param == 0)||(MSG_SLOT_CHANNEL != ioctl_command_id)) 
    {
        return -EINVAL;
    }
    minor_number = iminor(file->f_path.dentry->d_inode);
    ioctl_c = channel_Lookup(minor_number, ioctl_param);
    // if first time
    if(!ioctl_c)
    {
        new_c = add_new_Channel(minor_number,ioctl_param);
        if(!new_c)
        {
            return -ENOSPC;
        }
    }
    file->private_data = (void*)ioctl_param;
    return 0;
}

// The functions that will be called
struct file_operations fops =
        {
            .owner          = THIS_MODULE,
            .read           = device_read,
            .write          = dev_write,
            .open           = device_open,
            .unlocked_ioctl = dev_ioctl
        };

// load the module to the kernel
static int __init simple_init(void) 
{
    // Register the character device 
    if (register_chrdev(MAJOR_NUM, DEVICE_NAME, &fops) >= 0) 
    {
        printk("Registeration success\nPlease create a device file:\n");
        printk("mknod /dev/FILE_NAME c %d MINOR_NUMBER\n", MAJOR_NUM);
        return 0;
    }
    else
    {
        printk(KERN_ALERT "Registraion failed: %d\n", MAJOR_NUM );
        return -1;
    }
}

// unload the module to the kernel
static void __exit simple_cleanup(void) {
    int i;
    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
    printk("Remove device: %d\n", MAJOR_NUM);
    for(i=0 ; i<= 255 ;i++)
    {
        if(minors[i]!= NULL)
        { 
            Channel* free_c = minors[i]->head;
            while(free_c!= NULL)
            {
                Channel* next_c = free_c->next;
                kfree(free_c);
                free_c = next_c;
            }
            kfree(minors[i]);
        }
    }
}
// register the two above-mentioned functions
module_init(simple_init);
module_exit(simple_cleanup);

