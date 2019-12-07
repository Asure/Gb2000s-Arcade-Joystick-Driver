/*
 *  Arcade Joystick Driver for Pandora Box 4
 *
 *  Copyright (c) 2018 Vanni Brutto
 *
 *  based on the mk_arcade_joystick_rpi by Matthieu Proucelle
 *  and on the gamecon driver by Vojtech Pavlik, and Markus Hiienkari
 */


/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/jiffies.h>

#include <linux/ioport.h>
#include <asm/io.h>

//#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0)
#define HAVE_TIMER_SETUP
//#endif

MODULE_AUTHOR("Matthieu Proucelle");
MODULE_DESCRIPTION("Gb2000s Arcade Joystick Driver");
MODULE_LICENSE("GPL");

#define MK_DEVICES		2

#define GPIO_BASE        0x01c20800 /* GPIO controller */

static volatile unsigned int gpio = 0xffffffff;
static volatile unsigned int reg = GPIO_BASE;
static volatile unsigned int dev_base = 0x0;
static volatile unsigned long long int mk_jiffies = 0x0;


struct mk_config {
  int args[MK_DEVICES];
  unsigned int nargs;
};

static struct mk_config mk_cfg __initdata;

module_param_array_named(map, mk_cfg.args, int, &(mk_cfg.nargs), 0);
MODULE_PARM_DESC(map, "Enable or disable GPIO Arcade Joystick");

struct gpio_config {
  int mk_arcade_gpio_maps_custom[11];
  unsigned int nargs;
};

static struct gpio_config gpio_cfg __initdata;

module_param_array_named(gpio, gpio_cfg.mk_arcade_gpio_maps_custom, int, &(gpio_cfg.nargs), 0);
MODULE_PARM_DESC(gpio, "Numbers of custom GPIO for Arcade Joystick");

#define MK_REFRESH_TIME	HZ/100

struct mk_pad {
  struct input_dev *dev;
  char phys[32];
};

struct mk_nin_gpio {
  unsigned pad_id;
  unsigned cmd_setinputs;
  unsigned cmd_setoutputs;
  unsigned valid_bits;
  unsigned request;
  unsigned request_len;
  unsigned response_len;
  unsigned response_bufsize;
};

struct mk {
  struct mk_pad pads[MK_DEVICES];
  struct timer_list timer;
  int used;
  struct mutex mutex;
};

struct mk_subdev {
  unsigned int idx;
};

static struct mk *mk_base;

static const int mk_data_size = 24;

static const int mk_max_arcade_buttons = 12;

static const int mk_arcade_gpio_maps[] =  { 
    0x20*4+0x08,
    0x20*4+0x09,
    0x20*4+0x0a,
    0x20*4+0x0b,

    0x20*4+0x00,
    0x20*4+0x01,
    0x20*4+0x02,
    0x20*4+0x03,
    0x20*4+0x04,
    0x20*4+0x05,
    0x20*4+0x06,
    0x20*4+0x07,
    0x20*4+0x08,

    };





static const short mk_arcade_gpio_btn[] = {
  BTN_START, BTN_SELECT, BTN_A, BTN_B, BTN_TR, BTN_Y, BTN_X, BTN_TL
};

static const char *mk_names[] = {
    "Gb2000s v2.26 JAMMA Controller0" ,
    "Gb2000s v2.26 JAMMA Controller1" ,
};


static int readGpio(unsigned int gpio_reg, int pio)
{
  int port = (pio>>5) & 0xf;
  int bit  = (pio>>0) & 0x1f;
  unsigned int regData =  *((volatile unsigned int *) (gpio_reg+0x10+0x24*port));
  return  (regData>>bit)&0x1;
}

static void writeGpio(unsigned int gpio_reg, int pio, int d)
{
  int port = (pio>>5) & 0xf;
  int bit  = (pio>>0) & 0x1f;
  int regOff = 0x10+0x24*port;
  unsigned int regData =  *((volatile unsigned int *) (gpio_reg+regOff));
  if (d)
    regData |=  (1<<bit);
  else
    regData &= ~(1<<bit);
  *((volatile unsigned int *) (gpio_reg+regOff)) = regData;
  return ;
}

static void writeGpioCfg(unsigned int gpio_reg, int pio, bool out)
{
  int port = (pio>>5) & 0x1f;
  int bit  = (pio>>3) & 0x3;
  int bit0 = (pio>>0) & 0x7;
  int regOff = 0x00+0x24*port+0x04*bit;
  unsigned int regData =  *((volatile unsigned int *) (gpio_reg+regOff));

  int offset = bit0*4;

  regData &= ~(0xf<<(offset));
  if(out)
    regData |=  (0x01<<(offset));
  else
    regData |=  (0x00<<(offset));

  *((volatile unsigned int *) (gpio_reg+regOff)) = regData;
  return ;
}


enum{
    
  VK_AXI_UP, 
  VK_AXI_DOWN, 
  VK_AXI_LEFT, 
  VK_AXI_RIGHT, 
  VK_BTN_START, 
  VK_BTN_SELECT, 
  VK_BTN_A, 
  VK_BTN_B, 
  VK_BTN_TR, 
  VK_BTN_Y, 
  VK_BTN_X, 
  VK_BTN_TL,

  NUM_VKS
  
};

static void mk_gpio_read_packet( unsigned char *data) {
  int i;
  unsigned long long int tmp_jiffies;
  tmp_jiffies = jiffies_to_msecs(jiffies - mk_jiffies);


  // PIO mapping
  // SETTING      --  BTN_SELECT   1
  // COIN         --  BTN_SELECT   0
  // 1P_START     --  BTN_START    0
  // 1P_UP        --  BTN_UP       0
  // 1P_DOWN      --  BTN_DOWN     0     
  // 1P_LEFT      --  BTN_LEFT     0
  // 1P_RIGHT     --  BTN_RIGHT    0
  // 1P_A         --  BTN_A        0
  // 1P_B         --  BTN_B        0
  // 1P_C         --  BTN_TR       0
  // 1P_D         --  BTN_Y        0
  // 1P_E         --  BTN_X        0
  // 1P_F         --  BTN_TL       0
  // 2P_START     --  BTN_START    1
  // 2P_UP        --  BTN_UP       1
  // 2P_DOWN      --  BTN_DOWN     1
  // 2P_LEFT      --  BTN_LEFT     1
  // 2P_RIGHT     --  BTN_RIGHT    1
  // 2P_A         --  BTN_A        1
  // 2P_B         --  BTN_B        1
  // 2P_C         --  BTN_TR       1
  // 2P_D         --  BTN_Y        1
  // 2P_E         --  BTN_X        1
  // 2P_F         --  BTN_TL       1
  
  unsigned int d;  
  int idx;

  // 0
  writeGpio(gpio, mk_arcade_gpio_maps[0], 0x00); 
  writeGpio(gpio, mk_arcade_gpio_maps[1], 0x01); 
  writeGpio(gpio, mk_arcade_gpio_maps[2], 0x01);
  writeGpio(gpio, mk_arcade_gpio_maps[3], 0x01);
  idx = VK_BTN_SELECT +NUM_VKS* 1; d=readGpio(gpio, mk_arcade_gpio_maps[4 ]);if(d==0)data[idx]=1; else data[idx]=0;
  idx = VK_BTN_SELECT +NUM_VKS* 0; d=readGpio(gpio, mk_arcade_gpio_maps[5 ]);if(d==0)data[idx]=1; else data[idx]=0;
  idx = VK_BTN_START  +NUM_VKS* 0; d=readGpio(gpio, mk_arcade_gpio_maps[6 ]);if(d==0)data[idx]=1; else data[idx]=0;
  idx = VK_AXI_UP     +NUM_VKS* 0; d=readGpio(gpio, mk_arcade_gpio_maps[7 ]);if(d==0)data[idx]=1; else data[idx]=0;
  idx = VK_AXI_DOWN   +NUM_VKS* 0; d=readGpio(gpio, mk_arcade_gpio_maps[8 ]);if(d==0)data[idx]=1; else data[idx]=0;
  idx = VK_AXI_LEFT   +NUM_VKS* 0; d=readGpio(gpio, mk_arcade_gpio_maps[9 ]);if(d==0)data[idx]=1; else data[idx]=0;
  idx = VK_AXI_RIGHT  +NUM_VKS* 0; d=readGpio(gpio, mk_arcade_gpio_maps[10]);if(d==0)data[idx]=1; else data[idx]=0;
  idx = VK_BTN_A      +NUM_VKS* 0; d=readGpio(gpio, mk_arcade_gpio_maps[11]);if(d==0)data[idx]=1; else data[idx]=0;

  // 1
  writeGpio(gpio, mk_arcade_gpio_maps[0], 0x01); 
  writeGpio(gpio, mk_arcade_gpio_maps[1], 0x00); 
  writeGpio(gpio, mk_arcade_gpio_maps[2], 0x01);
  writeGpio(gpio, mk_arcade_gpio_maps[3], 0x01);
  idx = VK_BTN_B      +NUM_VKS* 0; d=readGpio(gpio, mk_arcade_gpio_maps[4 ]);if(d==0)data[idx]=1; else data[idx]=0;    
  idx = VK_BTN_TR     +NUM_VKS* 0; d=readGpio(gpio, mk_arcade_gpio_maps[5 ]);if(d==0)data[idx]=1; else data[idx]=0;    
  idx = VK_BTN_Y      +NUM_VKS* 0; d=readGpio(gpio, mk_arcade_gpio_maps[6 ]);if(d==0)data[idx]=1; else data[idx]=0;    
  idx = VK_BTN_X      +NUM_VKS* 0; d=readGpio(gpio, mk_arcade_gpio_maps[7 ]);if(d==0)data[idx]=1; else data[idx]=0;    
  idx = VK_BTN_TL     +NUM_VKS* 0; d=readGpio(gpio, mk_arcade_gpio_maps[8 ]);if(d==0)data[idx]=1; else data[idx]=0;    
  idx = VK_BTN_START  +NUM_VKS* 1; d=readGpio(gpio, mk_arcade_gpio_maps[9 ]);if(d==0)data[idx]=1; else data[idx]=0;    
  idx = VK_AXI_UP     +NUM_VKS* 1; d=readGpio(gpio, mk_arcade_gpio_maps[10]);if(d==0)data[idx]=1; else data[idx]=0;    
  idx = VK_AXI_DOWN   +NUM_VKS* 1; d=readGpio(gpio, mk_arcade_gpio_maps[11]);if(d==0)data[idx]=1; else data[idx]=0;    
  
  // 2
  writeGpio(gpio, mk_arcade_gpio_maps[0], 0x01); 
  writeGpio(gpio, mk_arcade_gpio_maps[1], 0x01); 
  writeGpio(gpio, mk_arcade_gpio_maps[2], 0x00);
  writeGpio(gpio, mk_arcade_gpio_maps[3], 0x01);
  idx = VK_AXI_LEFT   +NUM_VKS* 1; d=readGpio(gpio, mk_arcade_gpio_maps[4 ]);if(d==0)data[idx]=1; else data[idx]=0;
  idx = VK_AXI_RIGHT  +NUM_VKS* 1; d=readGpio(gpio, mk_arcade_gpio_maps[5 ]);if(d==0)data[idx]=1; else data[idx]=0;
  idx = VK_BTN_A      +NUM_VKS* 1; d=readGpio(gpio, mk_arcade_gpio_maps[6 ]);if(d==0)data[idx]=1; else data[idx]=0;
  idx = VK_BTN_B      +NUM_VKS* 1; d=readGpio(gpio, mk_arcade_gpio_maps[7 ]);if(d==0)data[idx]=1; else data[idx]=0;
  idx = VK_BTN_TR     +NUM_VKS* 1; d=readGpio(gpio, mk_arcade_gpio_maps[8 ]);if(d==0)data[idx]=1; else data[idx]=0;
  idx = VK_BTN_Y      +NUM_VKS* 1; d=readGpio(gpio, mk_arcade_gpio_maps[9 ]);if(d==0)data[idx]=1; else data[idx]=0;
  idx = VK_BTN_X      +NUM_VKS* 1; d=readGpio(gpio, mk_arcade_gpio_maps[10]);if(d==0)data[idx]=1; else data[idx]=0;
  idx = VK_BTN_TL     +NUM_VKS* 1; d=readGpio(gpio, mk_arcade_gpio_maps[11]);if(d==0)data[idx]=1; else data[idx]=0;

  mk_jiffies = jiffies;

}

static void mk_input_report(struct mk_pad * pad, unsigned char * data) {
  struct input_dev * dev = pad->dev;
  int j;
  input_report_abs(dev, ABS_Y, !data[0]-!data[1]);
  input_report_abs(dev, ABS_X, !data[2]-!data[3]);
  for (j = 4; j < mk_max_arcade_buttons; j++) {
    input_report_key(dev, mk_arcade_gpio_btn[j - 4], data[j]);
  }
  input_sync(dev);
}

static void mk_process_packet(struct mk *mk) {

  unsigned char data[mk_data_size];

  mk_gpio_read_packet(data);
  mk_input_report(&mk->pads[0], &data[NUM_VKS*0]);
  mk_input_report(&mk->pads[1], &data[NUM_VKS*1]);

}

/*
 * mk_timer() initiates reads of console pads data.
 */

#ifdef HAVE_TIMER_SETUP
static void mk_timer(struct timer_list *t)
{
  struct mk *mk = from_timer(mk, t, timer);
#else
static void mk_timer(unsigned long private)
{
    struct mk *mk = (void *) private;
#endif

  mk_process_packet(mk);
  mod_timer(&mk->timer, jiffies + MK_REFRESH_TIME);
}

static int mk_open(struct input_dev *dev) {
  struct mk *mk = input_get_drvdata(dev);
  int err;

  err = mutex_lock_interruptible(&mk->mutex);
  if (err)
    return err;

  if (!mk->used++)
    mod_timer(&mk->timer, jiffies + MK_REFRESH_TIME);

  mutex_unlock(&mk->mutex);
  return 0;
}

static void mk_close(struct input_dev *dev) {
  struct mk *mk = input_get_drvdata(dev);

  mutex_lock(&mk->mutex);
  if (!--mk->used) {
    del_timer_sync(&mk->timer);
  }
  mutex_unlock(&mk->mutex);
}


int readKey(int key_code) {
  int port = (key_code>>5) & 0xf;
  int bit = (key_code>>0) & 0x1f;
  printk("mk_port:%08x, bit:%08x\n", port, bit); //000007ff
  printk("mk_addr:%08x\n", (gpio+0x10+0x24*port)); //000007ff
  unsigned int regData =  *((volatile unsigned int *) (gpio+0x10+0x24*port));
  printk("mk_regData:%08x\n", regData);
  printk("mk_regData (shift):%08x\n", (regData >> bit));

  return   (regData>>bit)&0x1;
}

static int __init mk_setup_pad(struct mk *mk, int idx) {
  struct mk_pad *pad = &mk->pads[idx];
  struct input_dev *input_dev;
  int i;
  int err;

  pad->dev = input_dev = input_allocate_device();
  if (!input_dev) {
    pr_err("Not enough memory for input device\n");
    return -ENOMEM;
  }

  snprintf(pad->phys, sizeof (pad->phys),
      "input%d", idx);

  input_dev->name = mk_names[idx];
  input_dev->phys = pad->phys;
  input_dev->id.bustype = BUS_PARPORT;
  input_dev->id.vendor = 0x0001;
  input_dev->id.product = idx;
  input_dev->id.version = 0x0100;

  input_set_drvdata(input_dev, mk);

  input_dev->open = mk_open;
  input_dev->close = mk_close;

  input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);

  for (i = 0; i < 2; i++)
    input_set_abs_params(input_dev, ABS_X + i, -1, 1, 0, 0);

  for (i = 0; i < (mk_max_arcade_buttons - 4); i++)
    __set_bit(mk_arcade_gpio_btn[i], input_dev->keybit);

  printk("GPIO configured for pad%d\n", idx);


  err = input_register_device(pad->dev);
  if (err)
    goto err_free_dev;

  return 0;

err_free_dev:
  input_free_device(pad->dev);
  pad->dev = NULL;
  return err;
}

static struct mk __init *mk_probe(void) {
  struct mk *mk;
  int i;
  int count = 0;
  int err;

  mk = kzalloc(sizeof (struct mk), GFP_KERNEL);
  if (!mk) {
    pr_err("Not enough memory\n");
    err = -ENOMEM;
    goto err_out;
  }

  mutex_init(&mk->mutex);
//  setup_timer(&mk->timer, mk_timer, (long) mk);
#ifdef HAVE_TIMER_SETUP
  timer_setup(&mk->timer, mk_timer, 0);
#else
  setup_timer(&mk->timer, mk_timer, (long) mk);
#endif

  for (i = 0; i < MK_DEVICES; i++) {

    err = mk_setup_pad(mk, i);
    if (err)
      goto err_unreg_devs;

    count++;
  }

  // initial gpio direction
  writeGpioCfg(gpio, mk_arcade_gpio_maps[0 ], true); 
  writeGpioCfg(gpio, mk_arcade_gpio_maps[1 ], true); 
  writeGpioCfg(gpio, mk_arcade_gpio_maps[2 ], true); 
  writeGpioCfg(gpio, mk_arcade_gpio_maps[3 ], true); 

  writeGpioCfg(gpio, mk_arcade_gpio_maps[4 ], false);
  writeGpioCfg(gpio, mk_arcade_gpio_maps[5 ], false);
  writeGpioCfg(gpio, mk_arcade_gpio_maps[6 ], false);
  writeGpioCfg(gpio, mk_arcade_gpio_maps[7 ], false);
  writeGpioCfg(gpio, mk_arcade_gpio_maps[8 ], false);
  writeGpioCfg(gpio, mk_arcade_gpio_maps[9 ], false);
  writeGpioCfg(gpio, mk_arcade_gpio_maps[10], false);
  writeGpioCfg(gpio, mk_arcade_gpio_maps[11], false);


  if (count == 0) {
    pr_err("No valid devices specified\n");
    err = -EINVAL;
    goto err_free_mk;
  }

  return mk;

err_unreg_devs:
  while (--i >= 0)
    if (mk->pads[i].dev)
      input_unregister_device(mk->pads[i].dev);
err_free_mk:
  kfree(mk);
err_out:
  return ERR_PTR(err);
}

static void mk_remove(struct mk *mk) {
  int i;

  for (i = 0; i < MK_DEVICES; i++)
    if (mk->pads[i].dev)
      input_unregister_device(mk->pads[i].dev);
  kfree(mk);
}

static int __init mk_init(void) {
  /* Set up gpio pointer for direct register access */
  void* mem = NULL;
  unsigned int offset;
  printk("GPIO_BASE%08x\n", GPIO_BASE);
  dev_base = reg & (~(0x1000-1));
  printk("mk_dev_base:%08x\n", dev_base); 
  if ((mem = ioremap(dev_base, 0x1000)) == NULL) {
    pr_err("io remap failed\n");
    return -EBUSY;
  }
  offset = reg & ( (0x1000-1));
  gpio = ((unsigned int)mem)+offset;

  printk("mk_gpio_reg:%08x\n", gpio);
  mk_base = mk_probe();
  return 0;
}

static void __exit mk_exit(void) {
  if (mk_base)
    mk_remove(mk_base);

  iounmap(gpio);
}

module_init(mk_init);
module_exit(mk_exit);
