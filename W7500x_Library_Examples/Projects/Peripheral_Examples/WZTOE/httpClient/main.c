/*******************************************************************************************************************************************************
 * Copyright ¨Ï 2016 <WIZnet Co.,Ltd.> 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the ¡°Software¡±), 
 * to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED ¡°AS IS¡±, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*********************************************************************************************************************************************************/
/**
  ******************************************************************************
  * @file    WZTOE/httpClient/main.c 
  * @author  Eric Jung
  * @version V1.0.0
  * @date    01-Aug-2016
  * @brief   Main program body
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, WIZnet SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2015 WIZnet Co.,Ltd.</center></h2>
  ******************************************************************************
  */ 
/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include "W7500x_uart.h"
#include "W7500x_crg.h"
#include "W7500x_wztoe.h"
#include "W7500x_dualtimer.h"
#include "W7500x_miim.h"
#include "wizchip_conf.h"

#include "dhcp.h"
#include "dns.h"
#include "httpClient.h"

/* Private typedef -----------------------------------------------------------*/
UART_InitTypeDef UART_InitStructure;
GPIO_InitTypeDef GPIO_InitDef;

/* Private define ------------------------------------------------------------*/
#define __DEF_USED_MDIO__ 
#define __DEF_USED_IC101AG__ //for W7500 Test main Board V001

#define _MAIN_DEBUG_

#define TICKRATE_HZ1 (1000)		/* 1000 ticks per second, for SysTick */
#define TICKRATE_HZ2 (1)		/* 1 ticks per second, for Timer0 */
volatile uint32_t msTicks; 		/* counts 1ms timeTicks */

#ifndef DATA_BUF_SIZE
	#define DATA_BUF_SIZE 2048
#endif

///////////////////////////
// Demo Firmware Version //
///////////////////////////
#define VER_H		1
#define VER_L		00

////////////////
// Sockets    //
////////////////
#define SOCK_HTTPC  0
#define SOCK_DHCP   1
#define SOCK_DNS    2

////////////////
// DHCP client//
////////////////
#define __USE_DHCP__
uint8_t flag_process_dhcp_success = 0;

////////////////
// DNS client //
////////////////
#define __USE_DNS__
uint8_t flag_process_dns_success = 0;
uint8_t dns_server[4] = {8, 8, 8, 8};           // Secondary DNS server IP
uint8_t Domain_IP[4]  = {0, };                  // Translated IP address by DNS Server

////////////////
// Dual Timer //
////////////////
volatile uint16_t msec_cnt = 0;
volatile uint16_t sec_cnt = 0;
volatile uint32_t min_cnt = 0;


/* Private function prototypes -----------------------------------------------*/
void W7500x_Init(void);
void W7500x_WZTOE_Init(void);
int8_t process_dhcp(void);
int8_t process_dns(void);

void Timer_Configuration(void);
void Timer_IRQ_Handler(void);

void delay(__IO uint32_t milliseconds); //Notice: used ioLibray
void TimingDelay_Decrement(void);
void SysTick_Handler(void);

// Callback function : User defined DHCP callback functions
void W7500x_dhcp_assign(void);
void W7500x_dhcp_conflict(void);

/* Private variables ---------------------------------------------------------*/
static __IO uint32_t TimingDelay;

// Shared buffer declaration
uint8_t g_send_buf[DATA_BUF_SIZE];
uint8_t g_recv_buf[DATA_BUF_SIZE];

// Network Information
uint8_t mac_addr[6] = {0x00, 0x08, 0xDC, 0x71, 0x72, 0x77}; 
uint8_t src_addr[4] = {192, 168,  77,  9};
uint8_t gw_addr[4]  = {192, 168,  77,  1};
uint8_t sub_addr[4] = {255, 255, 255,  0};

// Example domain name
uint8_t Domain_name[] = "www.accuweather.com";
uint8_t URI[] = "/en/kr/sourth-korea-weather";
uint8_t flag_sent_http_request = DISABLE;

/**
 * @brief    Main routine for WIZwiki-W7500
 * @return   Function should not exit.
 */
int main(void)
{
	uint8_t tmp[6];
	
	uint16_t len = 0, i = 0;
	uint8_t tmpbuf[200] = {0, };
	
	/* W7500x MCU Initialization */
	W7500x_Init();
	
	/* W7500x WZTOE (Hardwired TCP/IP stack) Initialization */
	W7500x_WZTOE_Init();
	
	/* Set the MAC address to WIZCHIP */
	setSHAR(mac_addr);
	
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// W7500x Application: DHCP client / DNS client handler
	////////////////////////////////////////////////////////////////////////////////////////////////////
	
	/* Network Configuration - DHCP client */
	// Initialize Network Information: DHCP or Static IP allocation
#ifdef __USE_DHCP__
	if(process_dhcp() == DHCP_IP_LEASED) // DHCP success
	{
		flag_process_dhcp_success = ENABLE;
	}
	else // DHCP failed
#endif
	{
		// Set default static IP settings
		setSIPR(src_addr);
		setGAR(gw_addr);
		setSUBR(sub_addr);
	}
	
	/* DNS client */
#ifdef __USE_DNS__
	if(process_dns()) // DNS success
	{
		flag_process_dns_success = ENABLE;
	}
#endif
	
	/* Network Configuration (Default setting) */
	printf("====== W%d NET CONF ======\r\n",_WIZCHIP_);
	getSHAR(tmp); printf("MAC ADDRESS : %.2X:%.2X:%.2X:%.2X:%.2X:%.2X\r\n",tmp[0],tmp[1],tmp[2],tmp[3],tmp[4],tmp[5]); 
	getSIPR(tmp); printf("IP ADDRESS : %.3d.%.3d.%.3d.%.3d\r\n",tmp[0],tmp[1],tmp[2],tmp[3]); 
	getGAR(tmp);  printf("GW ADDRESS : %.3d.%.3d.%.3d.%.3d\r\n",tmp[0],tmp[1],tmp[2],tmp[3]); 
	getSUBR(tmp); printf("SN MASK: %.3d.%.3d.%.3d.%.3d\r\n",tmp[0],tmp[1],tmp[2],tmp[3]);
	
#ifdef __USE_DHCP__
	if(flag_process_dhcp_success == ENABLE)
	{
		printf(" # DHCP IP Leased time : %u seconds\r\n", getDHCPLeasetime());
	}
	else 
	{
		printf(" # DHCP Failed\r\n");
	}
#endif

#ifdef __USE_DNS__
	if(flag_process_dns_success == ENABLE)
	{
		printf(" # DNS: %s => %d.%d.%d.%d\r\n", Domain_name, Domain_IP[0], Domain_IP[1], Domain_IP[2], Domain_IP[3]);
	}
	else 
	{
		printf(" # DNS Failed\r\n");
	}
#endif
	
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// W7500x Application: Main Routine
	////////////////////////////////////////////////////////////////////////////////////////////////////
	
	httpc_init(SOCK_HTTPC, Domain_IP, 80, g_send_buf, g_recv_buf);
	
	while(1)
	{
		////////////////////////////////////////////
		// HTTP socket process handler
		////////////////////////////////////////////
		httpc_connection_handler();
		
		if(httpc_isSockOpen)
		{
			httpc_connect();
		}
		
		// HTTP client example	
		if(httpc_isConnected) 
		{
			if(!flag_sent_http_request)
			{
				// Send: HTTP request
				request.method = (uint8_t *)HTTP_GET;
				request.uri = (uint8_t *)URI;
				request.host = (uint8_t *)Domain_name;
				
				// HTTP client example #1: Function for send HTTP request (header and body fields are integrated)
				{
					httpc_send(&request, g_recv_buf, g_send_buf, 0);
				}
				
				// HTTP client example #2: Separate functions for HTTP request - default header + body
				{
					//httpc_send_header(&request, g_recv_buf, NULL, len);
					//httpc_send_body(g_send_buf, len); // Send HTTP requset message body
				}
				
				// HTTP client example #3: Separate functions for HTTP request with custom header fields - default header + custom header + body
				{
					//httpc_add_customHeader_field(tmpbuf, "Custom-Auth", "auth_method_string"); // custom header field extended - example #1
					//httpc_add_customHeader_field(tmpbuf, "Key", "auth_key_string"); // custom header field extended - example #2
					//httpc_send_header(&request, g_recv_buf, tmpbuf, len);
					//httpc_send_body(g_send_buf, len);
				}
				
				flag_sent_http_request = ENABLE;
			}
			
			// Recv: HTTP response
			if(httpc_isReceived > 0)
			{
				len = httpc_recv(g_recv_buf, httpc_isReceived);
				
				printf(" >> HTTP Response - Received len: %d\r\n", len);
				printf("======================================================\r\n");
				for(i = 0; i < len; i++) printf("%c", g_recv_buf[i]);
				printf("\r\n");
				printf("======================================================\r\n");
			}
		}
		
#ifdef __USE_DHCP__
		DHCP_run(); // DHCP renew
#endif
	}
}


/* Public functions **********************************************************/

void W7500x_Init(void)
{
	/* Set Systme init */
	SystemInit();

	/* UART Init */
	//UART_StructInit(&UART_InitStructure);
	//UART_Init(UART1,&UART_InitStructure);
	S_UART_Init(115200);
	
	/* Dual Timer Init */
	Timer_Configuration();

	/* SysTick_Config */
	SysTick_Config((GetSystemClock()/1000));
}

void W7500x_WZTOE_Init(void)
{
	/* Set WZ_100US Register */
	setTIC100US((GetSystemClock()/10000));

#ifdef __DEF_USED_IC101AG__ //For using IC+101AG
	*(volatile uint32_t *)(0x41003068) = 0x64; //TXD0 - set PAD strengh and pull-up
	*(volatile uint32_t *)(0x4100306C) = 0x64; //TXD1 - set PAD strengh and pull-up
	*(volatile uint32_t *)(0x41003070) = 0x64; //TXD2 - set PAD strengh and pull-up
	*(volatile uint32_t *)(0x41003074) = 0x64; //TXD3 - set PAD strengh and pull-up
	*(volatile uint32_t *)(0x41003050) = 0x64; //TXE  - set PAD strengh and pull-up
#endif	

#ifdef __DEF_USED_MDIO__ 
	/* mdio Init */
	mdio_init(GPIOB, MDC, MDIO );
	/* PHY Link Check via gpio mdio */
	while( link() == 0x0 )
	{
		printf(".");  
		delay(500);
	}
	printf("PHY is linked. \r\n");  
#else
	delay(1000);
#endif
}

int8_t process_dhcp(void)
{
	uint8_t ret = 0;
	uint8_t dhcp_retry = 0;

#ifdef _MAIN_DEBUG_
	printf(" - DHCP Client running\r\n");
#endif
	DHCP_init(SOCK_DHCP, g_send_buf);
	reg_dhcp_cbfunc(W7500x_dhcp_assign, W7500x_dhcp_assign, W7500x_dhcp_conflict);
	
	while(1)
	{
		ret = DHCP_run();
		
		if(ret == DHCP_IP_LEASED)
		{
#ifdef _MAIN_DEBUG_
			printf(" - DHCP Success\r\n");
#endif
			break;
		}
		else if(ret == DHCP_FAILED)
		{
			dhcp_retry++;
#ifdef _MAIN_DEBUG_
			if(dhcp_retry <= 3) printf(" - DHCP Timeout occurred and retry [%d]\r\n", dhcp_retry);
#endif
		}

		if(dhcp_retry > 3)
		{
#ifdef _MAIN_DEBUG_
			printf(" - DHCP Failed\r\n\r\n");
#endif
			DHCP_stop();
			break;
		}
	}
	
	return ret;
}


void W7500x_dhcp_assign(void)
{
	wiz_NetInfo gWIZNETINFO;

	getIPfromDHCP(gWIZNETINFO.ip);
	getGWfromDHCP(gWIZNETINFO.gw);
	getSNfromDHCP(gWIZNETINFO.sn);
	getDNSfromDHCP(gWIZNETINFO.dns);
	getSHAR(gWIZNETINFO.mac);
	
#ifdef __USE_DHCP__
	gWIZNETINFO.dhcp = NETINFO_DHCP;
#else
	gWIZNETINFO.dhcp = NETINFO_STATIC;
#endif

	ctlnetwork(CN_SET_NETINFO, (void*) &gWIZNETINFO);
}


void W7500x_dhcp_conflict(void)
{
	// todo
	;
}


int8_t process_dns(void)
{
	int8_t ret = 0;
	uint8_t dns_retry = 0;
	
#ifdef _MAIN_DEBUG_
	printf(" - DNS Client running\r\n");
#endif
	
	DNS_init(SOCK_DNS, g_send_buf);
	
	while(1) 
	{
		if((ret = DNS_run(dns_server, (uint8_t *)Domain_name, Domain_IP)) == 1)
		{
#ifdef _MAIN_DEBUG_
			printf(" - DNS Success\r\n");
#endif
			break;
		}
		else
		{
			dns_retry++;
#ifdef _MAIN_DEBUG_
			if(dns_retry <= 2) printf(" - DNS Timeout occurred and retry [%d]\r\n", dns_retry);
#endif
		}

		if(dns_retry > 2) {
#ifdef _MAIN_DEBUG_
			printf(" - DNS Failed\r\n\r\n");
#endif
			break;
		}
#ifdef __USE_DHCP__
		DHCP_run();
#endif
	}
	
	return ret;
}

void Timer_Configuration(void)
{
	DUALTIMER_InitTypDef Dualtimer_InitStructure;
	
	NVIC_EnableIRQ(DUALTIMER0_IRQn);
	
	/* Dualtimer 0_0 clock enable */
	DUALTIMER_ClockEnable(DUALTIMER0_0);

	/* Dualtimer 0_0 configuration */
	Dualtimer_InitStructure.TimerLoad = GetSystemClock() / 1000;
	Dualtimer_InitStructure.TimerControl_Mode = DUALTIMER_TimerControl_Periodic;
	Dualtimer_InitStructure.TimerControl_OneShot = DUALTIMER_TimerControl_Wrapping;
	Dualtimer_InitStructure.TimerControl_Pre = DUALTIMER_TimerControl_Pre_1;
	Dualtimer_InitStructure.TimerControl_Size = DUALTIMER_TimerControl_Size_32;

	DUALTIMER_Init(DUALTIMER0_0, &Dualtimer_InitStructure);

	/* Dualtimer 0_0 Interrupt enable */
	DUALTIMER_IntConfig(DUALTIMER0_0, ENABLE);

	/* Dualtimer 0_0 start */
	DUALTIMER_Start(DUALTIMER0_0);
}

void Timer_IRQ_Handler(void)
{
	if(DUALTIMER_GetIntStatus(DUALTIMER0_0))
	{
		DUALTIMER_IntClear(DUALTIMER0_0);
		
		msec_cnt++; // millisecond counter
		
		/* Second Process */
		if(msec_cnt >= 1000 - 1) //second //if((msec_cnt % 1000) == 0) 
		{
			msec_cnt = 0;
			sec_cnt++;
			
			DHCP_time_handler();	// Time counter for DHCP timeout
			DNS_time_handler();		// Time counter for DNS timeout
		}
		
		/* Minute Process */
		if(sec_cnt >= 60)
		{
			sec_cnt = 0;
			min_cnt++;
		}
	}

	if(DUALTIMER_GetIntStatus(DUALTIMER0_1))
	{
		DUALTIMER_IntClear(DUALTIMER0_1);
	}
}


/**
  * @brief  Inserts a delay time.
  * @param  nTime: specifies the delay time length, in milliseconds.
  * @retval None
  */
void delay(__IO uint32_t milliseconds)
{
  TimingDelay = milliseconds;

  while(TimingDelay != 0);
}

/**
  * @brief  Decrements the TimingDelay variable.
  * @param  None
  * @retval None
  */
void TimingDelay_Decrement(void)
{
  if (TimingDelay != 0x00)
  { 
    TimingDelay--;
  }
}
