/*
 * Realtek Semiconductor Corp.
 *
 * arch/rlx/rlxocp0/irq.c
 *   Interrupt and exception initialization for RLX OCP Platform
 *
 * Tony Wu (tonywu@realtek.com.tw)
 * Nov. 7, 2006
 */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/timex.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/irq.h>

#include <asm/bitops.h>
#include <asm/bootinfo.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/irq_cpu.h>
#include <asm/irq_vec.h>
#include <asm/system.h>

#include <asm/rlxregs.h>
#include <asm/rlxbsp.h>

#include "bspchip.h"

static struct irqaction irq_cascade = { 
  .handler = no_action,
  .mask = CPU_MASK_NONE,
  .name = "cascade",
};

#if defined(CONFIG_RTL_819X)
#define __IRAM_GEN			__attribute__  ((section(".iram-gen")))
__IRAM_GEN
#endif
static void bsp_ictl_irq_mask(unsigned int irq)
{
    REG32(BSP_GIMR) &= ~(1 << (irq - BSP_IRQ_ICTL_BASE));
}

#if defined(CONFIG_RTL_819X)
__IRAM_GEN
#endif
static void bsp_ictl_irq_unmask(unsigned int irq)
{
    REG32(BSP_GIMR) |= (1 << (irq - BSP_IRQ_ICTL_BASE));
}

static struct irq_chip bsp_ictl_irq = {
    .typename = "ICTL",
    .ack = bsp_ictl_irq_mask,
    .mask = bsp_ictl_irq_mask,
    .mask_ack = bsp_ictl_irq_mask,
    .unmask = bsp_ictl_irq_unmask,
};

static void bsp_ictl_irq_dispatch(void)
{
    volatile unsigned int pending;
  
    pending = REG32(BSP_GIMR) & REG32(BSP_GISR);

    if (pending & BSP_UART0_IP)
        do_IRQ(BSP_UART0_IRQ);
    else if (pending & BSP_UART1_IP)
        do_IRQ(BSP_UART1_IRQ);
    else if (pending & BSP_TC1_IP)
 	    do_IRQ(BSP_TC1_IRQ);
    else {
        printk("Unknown Interrupt:%x\n",pending);
        spurious_interrupt();
    }
}

#define  CAUSEF_IP345  (0x3800)
#if defined(CONFIG_RTL_819X)
__IRAM_GEN
#endif
void bsp_irq_dispatch(void)
{
	volatile unsigned int pending;
	volatile unsigned int savedIntMask;
	pending = read_c0_cause() & read_c0_status();
	savedIntMask=read_c0_status() & ST0_IM;
	
	if ( pending & CAUSEF_IP345 )
	{
		do
		{
			
			 if (pending & CAUSEF_IP4)
			        do_IRQ(4);	/* 	swcore	*/

			 if (pending & CAUSEF_IP5)
			        do_IRQ(5);	/*	timer	*/
			 
			   if (pending & CAUSEF_IP3)
			        do_IRQ(3);	/*	wlan		*/
		
			pending = read_c0_cause() & read_c0_status();
			
		} while(pending & CAUSEF_IP345);
	}
#if 0
    if (pending & CAUSEF_IP3)
        do_IRQ(3);	/*	wlan		*/
    else if (pending & CAUSEF_IP4)
        do_IRQ(4);	/* 	swcore	*/
    else if (pending & CAUSEF_IP5)
        do_IRQ(5);	/*	timer	*/
#endif
	else if (pending & CAUSEF_IP6)
        do_IRQ(6);
   	else if (pending & CAUSEF_IP2)
        bsp_ictl_irq_dispatch();
    else if (pending & CAUSEF_IP0)
        do_IRQ(0);
    else if (pending & CAUSEF_IP1)
        do_IRQ(1);
    else
        spurious_interrupt();

	write_c0_status((read_c0_status()|savedIntMask));

}

static void __init bsp_ictl_irq_init(unsigned int irq_base)
{
    int i;

    for (i=0; i < BSP_IRQ_ICTL_NUM; i++) 
        set_irq_chip_and_handler(irq_base + i, &bsp_ictl_irq, handle_level_irq);

    setup_irq(BSP_ICTL_IRQ, &irq_cascade);
}

void __init bsp_irq_init(void)
{
    /* disable ict interrupt */
    REG32(BSP_GIMR) = 0;

    /* initialize IRQ action handlers */
    rlx_cpu_irq_init(BSP_IRQ_CPU_BASE);
    rlx_vec_irq_init(BSP_IRQ_LOPI_BASE);
    bsp_ictl_irq_init(BSP_IRQ_ICTL_BASE);

    /* Set IRR */
    REG32(BSP_IRR0) = BSP_IRR0_SETTING;
    REG32(BSP_IRR1) = BSP_IRR1_SETTING;
    REG32(BSP_IRR2) = BSP_IRR2_SETTING;
    REG32(BSP_IRR3) = BSP_IRR3_SETTING;  

    /* enable global interrupt mask */
    REG32(BSP_GIMR) = BSP_TC0_IE | BSP_UART0_IE;
#if defined(CONFIG_PCI) || defined(CONFIG_RTL_PCIE_SIMPLE_INIT)
    REG32(BSP_GIMR) |= (BSP_PCIE_IE | BSP_PCIE2_IE);
#endif
#if defined(CONFIG_USB)
    REG32(BSP_GIMR) |= BSP_USB_H_IE;
#endif
#if defined(CONFIG_RTL_819X_SWCORE) || defined(CONFIG_RTL_ICTEST_SWITCH) || defined(CONFIG_RTL_865X_ETH)
    REG32(BSP_GIMR) |= (BSP_SW_IE);
#endif
#if defined(CONFIG_PCIE_POWER_SAVING)
    REG32(BSP_GIMR) |= BSP_GPIO_ABCD_IE;
#endif

}

#if defined(CONFIG_RTL_8196C) && defined(CONFIG_ARCH_SUSPEND_POSSIBLE)//michaelxxx 
   #define CONFIG_RTL819X_SUSPEND_CHECK_INTERRUPT 
    
   #ifdef CONFIG_RTL819X_SUSPEND_CHECK_INTERRUPT 
   #include <linux/proc_fs.h> 
   #include <linux/kernel_stat.h> 
   #include <asm/uaccess.h> 
   //#define INT_HIGH_WATER_MARK 1850 //for window size = 1, based on LAN->WAN test result 
   //#define INT_LOW_WATER_MARK  1150 
   //#define INT_HIGH_WATER_MARK 9190 //for window size = 5, based on LAN->WAN test result 
   //#define INT_LOW_WATER_MARK  5500 
   #define INT_HIGH_WATER_MARK 3200  //for window size = 5, based on WLAN->WAN test result 
   #define INT_LOW_WATER_MARK  2200 
   #define INT_WINDOW_SIZE_MAX 10 
   static int suspend_check_enable = 1; 
   static int suspend_check_high_water_mark = INT_HIGH_WATER_MARK; 
   static int suspend_check_low_water_mark = INT_LOW_WATER_MARK; 
   static int suspend_check_win_size = 5; 
   static struct timer_list suspend_check_timer; 
   static int index=0; 
   static int eth_int_count[INT_WINDOW_SIZE_MAX]; 
   static int wlan_int_count[INT_WINDOW_SIZE_MAX]; 
   int cpu_can_suspend = 1; 
   int cpu_can_suspend_check_init = 0; 
    
   static int read_proc_suspend_check(char *page, char **start, off_t off, 
           int count, int *eof, void *data) 
   { 
       int len; 
    
       len = sprintf(page, "enable=%d, winsize=%d(%d), high=%d, low=%d, suspend=%d\n", 
                   suspend_check_enable, suspend_check_win_size, INT_WINDOW_SIZE_MAX, 
                   suspend_check_high_water_mark, suspend_check_low_water_mark, cpu_can_suspend); 
    
       if (len <= off+count) 
           *eof = 1; 
       *start = page + off; 
       len -= off; 
       if (len > count) 
           len = count; 
       if (len < 0) 
           len = 0; 
       return len; 
   } 
    
   static int write_proc_suspend_check(struct file *file, const char *buffer, 
                 unsigned long count, void *data) 
   { 
           char tmp[128]; 
    
           if (buffer && !copy_from_user(tmp, buffer, 128)) { 
                   sscanf(tmp, "%d %d %d %d", 
                           &suspend_check_enable, &suspend_check_win_size, 
                           &suspend_check_high_water_mark, &suspend_check_low_water_mark); 
                   if (suspend_check_win_size >= INT_WINDOW_SIZE_MAX) 
                           suspend_check_win_size = INT_WINDOW_SIZE_MAX - 1; 
                   if (suspend_check_enable) { 
                           mod_timer(&suspend_check_timer, jiffies + 100); 
                   } 
                   else { 
                           del_timer(&suspend_check_timer); 
                   } 
           } 
           return count; 
   } 
    
   static void suspend_check_timer_fn(unsigned long arg) 
   { 
           int count, j; 
    
           index++; 
           if (INT_WINDOW_SIZE_MAX <= index) 
                   index = 0; 
           eth_int_count[index] = kstat_irqs(BSP_SWCORE_IRQ); 
           wlan_int_count[index] = kstat_irqs(BSP_PCIE_IRQ); 
           j = index - suspend_check_win_size; 
           if (j < 0) 
                   j += INT_WINDOW_SIZE_MAX; 
           count = (eth_int_count[index] - eth_int_count[j]) + 
                   (wlan_int_count[index]- wlan_int_count[j]); //unit: number of interrupt occurred 
    
           if (cpu_can_suspend) { 
                   if (count > suspend_check_high_water_mark) { 
                           cpu_can_suspend = 0; 
                           //printk("\n<<<RTL8196C LEAVE SLEEP>>>\n"); /* for Debug Only*/ 
                   } 
           } 
           else { 
                   if (count < suspend_check_low_water_mark) { 
                           cpu_can_suspend = 1; 
                           //printk("\n<<<RTL8196C ENTER SLEEP>>>\n"); /* for Debug Only*/ 
                   } 
           } 
   #if 0 /* for Debug Only*/ 
           printk("###index=%d, count=%d (%d+%d) suspend=%d###\n",index, count, 
                   (eth_int_count[index] - eth_int_count[j]), 
                   (wlan_int_count[index]- wlan_int_count[j]), 
                   cpu_can_suspend); 
   #endif 
           mod_timer(&suspend_check_timer, jiffies + 100); 
   } 
    
   void suspend_check_interrupt_init(void) 
   { 
           struct proc_dir_entry *res; 
           int i; 
    
           res = create_proc_entry("suspend_check", 0, NULL); 
           if (res) { 
                   res->read_proc = read_proc_suspend_check; 
                   res->write_proc = write_proc_suspend_check; 
           } 
           else { 
                   printk("unable to create /proc/suspend_check\n"); 
           } 
    
           for (i=0; i<INT_WINDOW_SIZE_MAX; i++) { 
                   wlan_int_count[i] = 0; 
                   eth_int_count[i] = 0; 
           } 
           init_timer(&suspend_check_timer); 
           suspend_check_timer.data = 0; 
           suspend_check_timer.function = suspend_check_timer_fn; 
           suspend_check_timer.expires = jiffies + 100; /* in jiffies */ 
           add_timer(&suspend_check_timer); 
   	}
   #endif // CONFIG_RTL819X_SUSPEND_CHECK_INTERRUPT 
   #endif //CONFIG_RTL8196C 

