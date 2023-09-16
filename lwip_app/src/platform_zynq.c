// 这段代码是平台相关初始化的注释,主要说明了以下内容:
// 这是Zynq平台的特定函数。

// 去除了之前UART初始化代码 和 定时器初始化代码。去除不必要的头文件和定义。
// 又把定时器初始化代码加回来。在定时器回调函数中,增加了对SI #692601的支持。
// SI #692601描述了EmacPs在大量RX流量下的一个硬件Bug:在重RX流量下,由于硬件Bug,有时RX通路会无响应。
//  解决方法是定期检查RX通路的流量状态(通过读取统计寄存器),如果统计寄存器一段时间没有变化,说明RX无流量,就重置RX数据通路。
// 因此增加了对此Bug的处理,通过定时器回调定期检查RX流量,如果停止就重置RX通路作为解决措施。
// 总结一下,这段注释主要是说明代码做了哪些变更,特别是为了解决EmacPs的一个RX问题而增加的定时器定期检查机制。

#include "xparameters.h"
#include "xparameters_ps.h"	/* defines XPAR values */
#include "xil_cache.h"
#include "xscugic.h"
#include "lwip/tcp.h"
#include "xil_printf.h"
#include "platform.h"
#include "platform_config.h"
#include "netif/xadapter.h"


#ifdef PLATFORM_ZYNQ
#include "xscutimer.h"

#define INTC_DEVICE_ID		XPAR_SCUGIC_SINGLE_DEVICE_ID
#define TIMER_DEVICE_ID		XPAR_SCUTIMER_DEVICE_ID
#define INTC_BASE_ADDR		XPAR_SCUGIC_0_CPU_BASEADDR
#define INTC_DIST_BASE_ADDR	XPAR_SCUGIC_0_DIST_BASEADDR
#define TIMER_IRPT_INTR		XPAR_SCUTIMER_INTR

#define RESET_RX_CNTR_LIMIT	400

void tcp_fasttmr(void);
void tcp_slowtmr(void);

static XScuTimer TimerInstance;

static int ResetRxCntr = 0;

extern struct netif *echo_netif;

volatile int TcpFastTmrFlag = 0;
volatile int TcpSlowTmrFlag = 0;

void timer_callback(XScuTimer * TimerInstance){
	static int DetectEthLinkStatus = 0;
///*我们需要按照lwIP指定的时间间隔调用tcp_fasttmr和tcp_slowtmr。计时的绝对精确性不是很重要。*/
	static int odd = 1;
	DetectEthLinkStatus++;
	 TcpFastTmrFlag = 1;

	odd = !odd;
	ResetRxCntr++;
	if (odd) {
		TcpSlowTmrFlag = 1;
	}

	if (ResetRxCntr >= RESET_RX_CNTR_LIMIT) {
		xemacpsif_resetrx_on_no_rxdata(echo_netif);
		ResetRxCntr = 0;
	}
	/* For detecting Ethernet phy linak status periodiclly */
	if (DetectEthLinkStatus == ETH_LINK_DETECT_INTERVAL) {
		eth_link_detect(echo_netif);
		DetectEthLinkStatus = 0;
	}
	XScuTimer_ClearInterruptStatus(TimerInstance);
}











void platform_setup_timer(void){
	int Status = XST_SUCCESS;
	XScuTimer_Config *ConfigPtr;
	int TimerLoadValue = 0;

	ConfigPtr = XScuTimer_LookupConfig(TIMER_DEVICE_ID);
	Status = XScuTimer_CfgInitialize(&TimerInstance, ConfigPtr,
			ConfigPtr->BaseAddr);
	if (Status != XST_SUCCESS) {
		xil_printf("In %s: Scutimer Cfg initialization failed...\r\n",__func__);
		return;
	}

	Status = XScuTimer_SelfTest(&TimerInstance);
	if (Status != XST_SUCCESS) {
		xil_printf("In %s: Scutimer Self test failed...\r\n",
		__func__);
		return;
	}

	XScuTimer_EnableAutoReload(&TimerInstance);
    //设置为 250 毫秒的超时时间。
	TimerLoadValue = XPAR_CPU_CORTEXA9_0_CPU_CLK_FREQ_HZ / 8;

	XScuTimer_LoadTimer(&TimerInstance, TimerLoadValue);
	return;
}

// void platform_setup_interrupts(void){
// 	Xil_ExceptionInit();
// 	XScuGic_DeviceInitialize(INTC_DEVICE_ID);
// 	// 将中断控制器的中断处理程序连接到处理器中的硬件中断处理逻辑。
// 	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_IRQ_INT,(Xil_ExceptionHandler)XScuGic_DeviceInterruptHandler,(void *)INTC_DEVICE_ID);
// 	// 将设备的中断处理程序连接到设备驱动上, 当设备中断发生时将会调用这个处理程序。
// 	// 上面定义的处理程序实现了针对该设备的特定中断处理逻辑。
// 	XScuGic_RegisterHandler(INTC_BASE_ADDR, TIMER_IRPT_INTR,(Xil_ExceptionHandler)timer_callback,(void *)&TimerInstance);
// 	XScuGic_EnableIntr(INTC_DIST_BASE_ADDR, TIMER_IRPT_INTR);
// 	return;
// }


void platform_setup_interrupts(XScuGic *intc){
	XScuGic_Config *intc_cfg;
	intc_cfg = XScuGic_LookupConfig(INTC_DEVICE_ID);
    XScuGic_CfgInitialize(intc, intc_cfg,intc_cfg->CpuBaseAddress);	
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_IRQ_INT,(Xil_ExceptionHandler)XScuGic_DeviceInterruptHandler,(void *)INTC_DEVICE_ID);
	Xil_ExceptionEnable();
	XScuGic_Connect(intc, TIMER_IRPT_INTR, (Xil_ExceptionHandler) timer_callback,(void *) &TimerInstance);
	XScuGic_Enable(intc, TIMER_IRPT_INTR);
	return;
}




void init_platform(XScuGic *intc){
	platform_setup_timer();
	platform_setup_interrupts(intc);
	return;
}


void platform_enable_interrupts(){
    //    启用非关键异常
	Xil_ExceptionEnableMask(XIL_EXCEPTION_IRQ);
	XScuTimer_EnableInterrupt(&TimerInstance);
	XScuTimer_Start(&TimerInstance);
	return;
}



void cleanup_platform(){
	Xil_ICacheDisable();
	Xil_DCacheDisable();
	return;
}
#endif
