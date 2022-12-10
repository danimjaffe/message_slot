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


static ssize_t device_write(struct file *file, const char __user* buffer,size_t length,loff_t* offset){
    int i;
    Channel *channel;
    char cur_buffer[BUFFER_LEN];
    unsigned int minor;
    unsigned long c_id;
    printk("Invoking device_write\n");
    if(length == 0 || length > BUFFER_LEN){ 
        return -EMSGSIZE;
    }
    c_id = (unsigned long)file->private_data;
    if(c_id == 0){
        return -EINVAL;
    }
    minor = iminor(file->f_path.dentry->d_inode);
    channel = channel_Lookup(minor, c_id);
    if(!channel){ 
        return -EINVAL;
    }
    for(i = 0; i < length ;i++){
        if(get_user(cur_buffer[i],&buffer[i])!=0){
            return -EINVAL;
        }
    }
    for(i = 0; i < length ; i++){
        channel->msg_content[i]= cur_buffer[i]; 
    }
    channel->msg_len = length;
    return length;
}

static long device_ioctl(struct file *file, unsigned int ioctl_command_id, unsigned long ioctl_param) {
    Channel* ioctl_channel, *new_channel;
    unsigned int minor;
    printk("Invoking device_ioctl\n");
    if(MSG_SLOT_CHANNEL != ioctl_command_id || ioctl_param == 0) {
        return -EINVAL;
    }
    minor = iminor(file->f_path.dentry->d_inode);
    ioctl_channel = channel_Lookup(minor, ioctl_param);
    if(!ioctl_channel){
        new_channel = add_new_Channel(minor,ioctl_param);
        if(!new_channel){
            return -ENOSPC;
        }
    }
    file->private_data = (void*)ioctl_param;
    return 0;
}


//==================== DEVICE SETUP =============================
struct file_operations fops =
        {
            .owner          = THIS_MODULE,
            .read           = device_read,
            .write          = device_write,
            .open           = device_open,
            .unlocked_ioctl = device_ioctl
        };

// Init module and register the device similar to ex6 example
static int __init simple_init(void) 
{
    int rc;
    rc = register_chrdev(MAJOR_NUM, DEVICE_NAME, &fops);
    if (rc<0){
        printk(KERN_ALERT "Failed registraion: %d\n", MAJOR_NUM );
        return rc;
    }
    printk("The Registeration was successful\n");
    printk("In order to create a device file write:\n");
    printk("mknod /dev/FILE_NAME c %d MINOR_NUMBER\n", MAJOR_NUM);
    return 0;
}

// unload the module and unregister the device
static void __exit simple_cleanup(void) {
    int i;
    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
    for(i=0 ; i<= 255 ;i++){
        if(minors[i]!= NULL){ 
            Channel* channel = minors[i]->head;
            while(channel!= NULL){
                Channel* next_channel = channel->next;
                kfree(channel);
                channel = next_channel;
            }
            kfree(minors[i]);
        }
    }
    printk("Device %d is removed\n", MAJOR_NUM);
}

module_init(simple_init);
module_exit(simple_cleanup);

