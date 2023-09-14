#include "xil_exception.h"
#include "xscugic.h"
#include <stdio.h>
#include "xaxidma.h"
#include "xparameters.h"
#include "netif/xadapter.h"
#include "platform.h"
#include "platform_config.h"
#include "xil_printf.h"
#include "lwip/tcp.h"
#include "xil_cache.h"
#include "sleep.h"


#define DMA_DEV_ID          XPAR_AXIDMA_0_DEVICE_ID      // 0  基地址 4040 0000
#define RX_INTR_ID          XPAR_FABRIC_AXIDMA_0_VEC_ID  //  61U

#define INTC_DEVICE_ID      XPAR_SCUGIC_SINGLE_DEVICE_ID  // 0
#define RESET_TIMEOUT_COUNTER   10000    

#define DDR_BASE_ADDR        XPAR_PS7_DDR_0_S_AXI_BASEADDR   //0x0010 0000    DDR内存控制器的基地址
#define MEM_BASE_ADDR       (DDR_BASE_ADDR + 0x01000000)     //0x0110 0000    在DDR基地址后面偏移了0x01000000(16MB)。 MEM_BASE_ADDR就指向了DDR内存空间的16MB偏移位置。
#define RX_BUFFER_BASE      (MEM_BASE_ADDR + 0x00300000)     //0x0140 0000
#define MAX_PKT_LEN             100                          //  100个字节


static void rx_intr_handler(void *callback);
static int  setup_intr_system(XScuGic * int_ins_ptr, XAxiDma * axidma_ptr, u16 rx_intr_id);


static XAxiDma axidma;
static XScuGic intc;

volatile int rx_done;
volatile int error;

void print_app_header();
int  start_application();
int  transfer_data();
void tcp_fasttmr(void);
void tcp_slowtmr(void);

void lwip_init();   //  /* lwIP 中缺少的声明 */

// #if LWIP_IPV6==0
// #if LWIP_DHCP==1
// extern volatile int dhcp_timoutcntr;
// err_t dhcp_start(struct netif *netif);
// #endif
// #endif







extern volatile int TcpFastTmrFlag;
extern volatile int TcpSlowTmrFlag;
static struct netif server_netif;
struct netif *echo_netif;



void print_ip(char *msg, ip_addr_t *ip){
	print(msg);
	xil_printf("%d.%d.%d.%d\n\r", ip4_addr1(ip), ip4_addr2(ip),	ip4_addr3(ip), ip4_addr4(ip));
}

void print_ip_settings(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw){
	print_ip("Board IP: ", ip);
	print_ip("Netmask : ", mask);
	print_ip("Gateway : ", gw);
}









int main(){
	int status;
    // Xil_DCacheInvalidateRange((UINTPTR) rx_buffer_ptr, MAX_PKT_LEN);

	static XAxiDma axidma;
    XAxiDma_Config *config;
    config = XAxiDma_LookupConfig(DMA_DEV_ID);
    if (!config) {
        xil_printf("No config found for %d\r\n", DMA_DEV_ID);
        return XST_FAILURE;
    }
    status = XAxiDma_CfgInitialize(&axidma, config);
    if (status != XST_SUCCESS) {
        xil_printf("Initialization failed %d\r\n", status);
        return XST_FAILURE;
    }
    status = setup_intr_system(&intc, &axidma, RX_INTR_ID);
    if (status != XST_SUCCESS) {
        xil_printf("Failed intr setup\r\n");
        return XST_FAILURE;
    }
    rx_done = 0;
    error   = 0;



    uint8_t *rx_buffer_ptr;     // uint8_t类型的指针  指向接收缓冲区的起始地址
    rx_buffer_ptr = (uint8_t *) RX_BUFFER_BASE;     // 定义好的接收缓冲区的 起始地址常量。  强制类型转换为uint8_t指针类型

    // status = XAxiDma_SimpleTransfer(&axidma, (UINTPTR) rx_buffer_ptr , MAX_PKT_LEN, XAXIDMA_DEVICE_TO_DMA);
    // if (status != XST_SUCCESS) {
    //     return XST_FAILURE;
    // }
    // Xil_DCacheFlushRange((UINTPTR) rx_buffer_ptr, MAX_PKT_LEN);
    // for(int i = 0; i < 100; i++) {
    //     xil_printf("%02X ",* (rx_buffer_ptr + i ) );
    // }
    // xil_printf("\n");
    // xil_printf("end\n");





	ip_addr_t ipaddr, netmask, gw;
	unsigned char mac_ethernet_address[] = { 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };
	echo_netif = &server_netif;
	init_platform();


	IP4_ADDR(&ipaddr,  192, 168,   1, 10);
	IP4_ADDR(&netmask, 255, 255, 255,  0);
	IP4_ADDR(&gw,      192, 168,   1,  1);

	print_app_header();
	lwip_init();
	if (!xemac_add(echo_netif, &ipaddr, &netmask, &gw, mac_ethernet_address,PLATFORM_EMAC_BASEADDR)) {
		xil_printf("Error adding N/W interface\n\r");
		return -1;
	}


	netif_set_default(echo_netif);

	// platform_enable_interrupts();

	netif_set_up(echo_netif);



	print_ip_settings(&ipaddr, &netmask, &gw);

	start_application();
	
    /* receive and process packets */
	while (1) {
		if (TcpFastTmrFlag) {
			tcp_fasttmr();
			TcpFastTmrFlag = 0;
		}
		if (TcpSlowTmrFlag) {
			tcp_slowtmr();
			TcpSlowTmrFlag = 0;
		}
		xemacif_input(echo_netif);
		transfer_data();
	}
	// cleanup_platform();
	return 0;
}






//------------------------------------------------------------------------------------
// 接收中断
//定义了接收中断的处理函数rx_intr_handler
//在中断处理函数中,会获取中断状态,acknowledge中断,检查是否有错误中断,如果有错误则重置DMA,检查是否有完成中断,如果有则设置rx_done标志。
static void rx_intr_handler(void *callback){
    u32 irq_status;
    int timeout;
    
    XAxiDma *axidma_inst = (XAxiDma *) callback;

    irq_status = XAxiDma_IntrGetIrq(axidma_inst, XAXIDMA_DEVICE_TO_DMA);
    
    XAxiDma_IntrAckIrq(axidma_inst, irq_status, XAXIDMA_DEVICE_TO_DMA);

    if ((irq_status & XAXIDMA_IRQ_ERROR_MASK)) {
        error = 1;
        XAxiDma_Reset(axidma_inst);
        timeout = RESET_TIMEOUT_COUNTER;
        while (timeout) {
            if (XAxiDma_ResetIsDone(axidma_inst))
                break;
            timeout -= 1;
        }
        return;
    }
    if ((irq_status & XAXIDMA_IRQ_IOC_MASK))
        rx_done = 1;
}




static int setup_intr_system(XScuGic * int_ins_ptr, XAxiDma * axidma_ptr, u16 rx_intr_id){
    int status;
    XScuGic_Config *intc_config;
    //1、初始化中断控制器驱动
    intc_config = XScuGic_LookupConfig(INTC_DEVICE_ID);   //初始化中断控制器GIC
    if (NULL == intc_config) {
        return XST_FAILURE;
    }

    status = XScuGic_CfgInitialize(int_ins_ptr, intc_config,intc_config->CpuBaseAddress);
    if (status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    //设置DMA接收通道的中断优先级为0xA0,触发模式为边沿触发(0x3)
    XScuGic_SetPriorityTriggerType(int_ins_ptr, rx_intr_id, 0xA0, 0x3);

    status = XScuGic_Connect(int_ins_ptr, rx_intr_id,(Xil_InterruptHandler) rx_intr_handler, axidma_ptr);
    if (status != XST_SUCCESS) {
        return status;
    }
    // 注册中断处理函数rx_intr_handler,并使能
    XScuGic_Enable(int_ins_ptr, rx_intr_id);

    // 启用来自硬件的中断
    Xil_ExceptionInit();
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,(Xil_ExceptionHandler) XScuGic_InterruptHandler,(void *) int_ins_ptr);
    Xil_ExceptionEnable();

    //  使能DMA的中断
    XAxiDma_IntrEnable(&axidma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
    return XST_SUCCESS;
}

