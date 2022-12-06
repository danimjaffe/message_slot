/*
I watched and used the following youtube explanation during this ex:
https://www.youtube.com/watch?v=CWihl19mJig 
*/

#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include "message_slot.h"
#include <linux/kernel.h>  
#include <linux/module.h> 
#include <linux/fs.h>      
#include <linux/uaccess.h> 
#include <linux/string.h>   
#include <linux/slab.h>        
#include <linux/errno.h>  
#include <linux/ioctl.h>

MODULE_LICENSE("GPL");

static List_of_Channels *minors[256] = {NULL};

Channel *search_Channel(unsigned int minor_num, unsigned int channel_id) {
    Channel *tmp;
    // need to be 0-255
    if (minor_num < 256) 
    {
        tmp = minors[minor_num]->head;
        while (tmp != NULL) {
            if (tmp->id == channel_id) {
                return tmp;
            }
            tmp = tmp->next;
        }
    }
    return NULL; // didnt find
}

Channel *create_Channel(unsigned int channel_id) {
    Channel *new_channel = kmalloc(sizeof(Channel), GFP_KERNEL);
    if (new_channel) 
    {
        new_channel->id = channel_id;
        new_channel->next = NULL;
        new_channel->msg_len=0;
        return new_channel;
    }
    return NULL;
}

Channel *add_to_Channel(unsigned int minor_number, unsigned int channel_id) {
    Channel *new_channel = create_Channel(channel_id);
    Channel *tmp;
    if (new_channel) 
    {
        tmp = minors[minor_number]->tail;
        // add to the end
        minors[minor_number]->tail = new_channel;
        tmp->next = new_channel;
        return new_channel;
    }
    return NULL;
}


static int dev_open(struct inode *inode, struct file *file) {
    List_of_Channels *new_channel_l;
    Channel *open_first_channel;
    unsigned long minor_num = iminor(inode);

    // if the dev is already open
    if (minors[minor_num] != NULL) 
    { 
        return 0;
    }
    else
    {
        //allocation
        new_channel_l = kmalloc(sizeof(List_of_Channels), GFP_KERNEL);
        if (new_channel_l) 
        {
            open_first_channel = create_Channel(0);
            if (open_first_channel) 
            {
                //initialize
                new_channel_l->tail = open_first_channel;
                new_channel_l->head = open_first_channel;
                minors[minor_num] = new_channel_l;
                file->private_data = (void *) 0; 
                return 0;
            }
        }
    }
    return -ENOSPC;
}

static ssize_t dev_read(struct file *file,
                           char __user * buffer,size_t length, loff_t* offset ){
    Channel *read_c;
    unsigned int minor_n = iminor(file->f_path.dentry->d_inode);
    unsigned long c_id = (unsigned long)file->private_data;
    int i;

    printk( "Invoking dev_read\n");
    if(c_id != 0)
    {     
        read_c = search_Channel(minor_n, c_id);
        //check existance
        if(read_c)
        {
            if(read_c->msg_len != 0)
            {
                // enought sapce for data
                if( read_c->msg_len <= length)
                {
                    for( i = 0; i < read_c->msg_len ; i++)
                    { // write to user buffer
                        if(put_user(read_c->msg_content[i],&buffer[i])!=0)
                        {
                            return -EINVAL;
                        }
                    }
                    return read_c->msg_len;
                }
                else
                {
                    return -ENOSPC;
                }
            }
            else
            {
                return -EWOULDBLOCK;
            }
        }
    }
    return -EINVAL;
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
    channel_w = search_Channel(minor_number, channel_id);
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
    ioctl_c = search_Channel(minor_number, ioctl_param);
    // if first time
    if(!ioctl_c)
    {
        new_c = add_to_Channel(minor_number,ioctl_param);
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
            .read           = dev_read,
            .write          = dev_write,
            .open           = dev_open,
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

