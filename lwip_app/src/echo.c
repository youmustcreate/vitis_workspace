#include <stdio.h>
#include <string.h>

#include "lwip/err.h"
#include "lwip/tcp.h"
#include "xil_printf.h"
#include "xaxidma.h"
#include "xgpio.h"

#define AXI_GPIO0_DEV_ID	    XPAR_AXI_GPIO_0_DEVICE_ID   // 0
#define DATA_DIRECTION_MASK     0x00000000

int Gpiopl_init(XGpio *InstancePtr, u32 DeviceId, u32 DirectionMask);



int transfer_data() {
	return 0;
}

void print_app_header(){
	xil_printf("\n\r\n\r-----lwIP TCP echo server ------\n\r");
}


err_t recv_callback(void *arg, struct tcp_pcb *tpcb,struct pbuf *p, err_t err){
	XGpio Gpio;
	Gpiopl_init(&Gpio, AXI_GPIO0_DEV_ID, DATA_DIRECTION_MASK);/*initial PL's AXI GPIO*/
	/* do not read the packet if we are not in ESTABLISHED state */
	// 是否处于ESTABLISHED状态,如果不是则关闭连接并返回。
	if (!p) {
		tcp_close(tpcb);
		tcp_recv(tpcb, NULL);
		return ERR_OK;
	}
// 获取要关闭的 TCP连接控制块pcb
// 调用tcp_close()关闭连接
// 判断返回值,成功则err返回ERR_OK
// 注意如果是服务器端,在关闭连接后需要同时调用tcp_accept()来等待并接受新连接。
// 另外,tcp_close()只是发起关闭连接的请求,双方还需要通过四次挥手来完成连接关闭。
// 所以不能立即释放pcb,需要在收到FIN ACK后再释放。
// 总之,tcp_close()是一个主动关闭TCP连接的LWIP库API函数。但需要正确处理各种情况。

//所以tcp_recv()是用来注册TCP连接的数据接收和处理的回调函数。
// 注册TCP连接的数据接收回调函数。
// pcb: 已建立的TCP连接控制块
// recv: 接收回调函数指针
// 当TCP连接pcb收到新数据时,LWIP会调用recv回调函数。
// 在recv回调中可以处理收到的数据。
// 通过回调参数可以获取连接pcb等信息。

	/* indicate that the packet has been received */
	tcp_recved(tpcb, p->len);  // 通过tcp_recved()通知LWIP已收到的数据。
	// tcp_recved()用于通知LWIP已经处理了指定长度的数据,重要性在于可以及时释放内存,更新ACK,防止数据占用内存过多。
// 	当应用程序通过回调函数接收并处理了len字节的数据后,调用tcp_recved()通知堆栈。
//  TCP堆栈会将确认号 更新为当前序号+len,并发送ACK给对端。
//  这个通知可以防止收到大量数据时导致内存占用过多。

	/* echo back the payload */
	/* in this case, we assume that the payload is < TCP_SND_BUF */
	// 回显接收到的数据,使用tcp_write()函数发送回去。
	if (tcp_sndbuf(tpcb) > p->len) {
		err = tcp_write(tpcb, p->payload, p->len, 1);  // 这里是 echo  TCP_WRITE_FLAG_COPY = 1 
    	XGpio_DiscreteWrite(&Gpio, 1, 1); //Mask – is the value to be written to the discretes register.  离散寄存器
    	XGpio_DiscreteWrite(&Gpio, 1, 0); // 27 * 20ns 的 脉冲
	} else    // 先判断发送缓冲区是否有足够空间,不够则打印提示。
		xil_printf("no space in tcp_sndbuf\n\r");
	/* free the received pbuf */
	pbuf_free(p);
	// 释放接收到的pbuf。
	return ERR_OK;
}


















// pcb: 要发送数据的TCP连接控制块
// data: 发送的数据内容
// len: 数据长度
// apiflags: TCP_WRITE_FLAG_{COPY/NOCOPY}等标志
//根据返回值判断是否发送成功


// 其中apiflags参数有两种可选值:
// TCP_WRITE_FLAG_COPY: 内存复制模式
// TCP_WRITE_FLAG_NOCOPY: 零复制模式
// TCP_WRITE_FLAG_COPY意味着会将要发送的数据从应用提供的内存buffer中复制一份到LWIP内部缓冲区中,然后从内部缓冲区发送出去。
// 也就是说,LWIP会复制一次数据。
// 这种模式下应用提供的buffer可以在tcp_write返回后立即重用。
// TCP_WRITE_FLAG_NOCOPY意味着不复制,直接从应用提供的buffer发送数据。
// 这种模式效率更高,但是需要保证在数据发送完毕前不能重用该buffer。
// 所以通过apiflags参数,应用程序可以选择LWIP是复制还是直接引用 buffer来发送数据。


err_t accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err){
	static int connection = 1;
	/* set the receive callback for this connection */
	// 为新接受的连接newpcb设置receive回调函数为recv_callback。
	tcp_recv(newpcb, recv_callback);

	/* just use an integer number indicating the connection id as the
	   callback argument */
	   //使用connection变量作为回调参数,通过tcp_arg()关联到newpcb上。
	tcp_arg(newpcb, (void*)(UINTPTR)connection);

	/* increment for subsequent accepted connections */
	connection++;   // 为后续新的连接准备回调参数。

	return ERR_OK;
}
// 当有新连接接入时,通过accept_callback可以为这个连接设置receive回调函数,并关联一个唯一的连接ID。
// 这实现了TCP服务器用于接受和处理新的连接的逻辑。








int start_application(){
	struct tcp_pcb *pcb;
	err_t err;
	unsigned port = 7;

	pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
	if (!pcb) {
		xil_printf("Error creating PCB. Out of Memory\n\r");
		return -1;
	}
	err = tcp_bind(pcb, IP_ANY_TYPE, port);
	if (err != ERR_OK) {
		xil_printf("Unable to bind to port %d: err = %d\n\r", port, err);
		return -2;
	}
	tcp_arg(pcb, NULL);
	pcb = tcp_listen(pcb);
	if (!pcb) {
		xil_printf("Out of memory while tcp_listen\n\r");
		return -3;
	}
	tcp_accept(pcb, accept_callback);  /* specify callback to use for incoming connections */  // 等待接收新的连接
	xil_printf("TCP echo server started @ port %d\n\r", port);
	return 0;
}




int Gpiopl_init(XGpio *InstancePtr, u32 DeviceId, u32 DirectionMask){
	int Status;
	Status = XGpio_Initialize(InstancePtr, DeviceId);
	if (Status != XST_SUCCESS) {
		xil_printf("AXI GPIO %d config failed!\r\n", DeviceId);
		return XST_FAILURE;
	}
	XGpio_SetDataDirection(InstancePtr, 1, DirectionMask);
	XGpio_DiscreteClear(InstancePtr, 1, 0);
    return 1;
}