#ifndef __PLATFORM_H_
#define __PLATFORM_H_

#define ETH_LINK_DETECT_INTERVAL 4

void init_platform();
void cleanup_platform();

void platform_setup_timer();
void platform_enable_interrupts();
#endif

// 通过定义这些平台相关的接口,上层网络代码可以通过调用这些函数 来实现 定时任务、中断处理等与平台相关的操作。
// 而无需了解底层具体的硬件细节。这样可以提高代码的可移植性。
// chard是一个抽象了底层平台差异的一层,上层都用统一的platform接口与平台进行交互。