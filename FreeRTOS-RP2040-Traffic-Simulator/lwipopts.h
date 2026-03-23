/**
 * @file lwipopts.h
 * @brief Configurações da pilha de rede lwIP para o RP2040.
 *
 * Define parâmetros de performance, depuração e funcionalidades ativadas
 * para o driver de rede CYW43439.
 */

#ifndef _LWIPOPTS_EXAMPLE_COMMONH_H
#define _LWIPOPTS_EXAMPLE_COMMONH_H

/** @name Configurações de Sistema */
/** @{ */
#ifndef NO_SYS
#define NO_SYS 0 /**< Utiliza o kernel FreeRTOS. */
#endif
#ifndef LWIP_SOCKET
#define LWIP_SOCKET 1 /**< Habilita API de Sockets. */
#endif

/* Evita conflito de struct timeval com newlib (sys/_timeval.h) */
#define LWIP_TIMEVAL_PRIVATE 0

/** Mailbox sizes - obrigatórios com NO_SYS=0 (FreeRTOS) */
#define TCPIP_MBOX_SIZE 8
#define DEFAULT_RAW_RECVMBOX_SIZE 8
#define DEFAULT_UDP_RECVMBOX_SIZE 8
#define DEFAULT_TCP_RECVMBOX_SIZE 8
#define DEFAULT_ACCEPTMBOX_SIZE 8

/** Thread configuration - obrigatório com NO_SYS=0 (FreeRTOS) */
#define TCPIP_THREAD_STACKSIZE 1024
#define TCPIP_THREAD_PRIO 6 /* configMAX_PRIORITIES(8) - 2 */
#define DEFAULT_THREAD_STACKSIZE 1024
#define DEFAULT_THREAD_PRIO 5 /* configMAX_PRIORITIES(8) - 3 */
#define LWIP_FREERTOS_THREAD_STACKSIZE_IS_STACKWORDS 1
/** @} */

/** @name Gerenciamento de Memória */
/** @{ */
#define MEM_ALIGNMENT 4
#ifndef MEM_SIZE
#define MEM_SIZE 4000
#endif
#define PBUF_POOL_SIZE 24
/** @} */

/** @name Protocolos Habilitados */
/** @{ */
#define LWIP_ARP 1
#define LWIP_ETHERNET 1
#define LWIP_ICMP 1
#define LWIP_RAW 1
#define LWIP_DHCP 1
#define LWIP_IPV4 1
#define LWIP_TCP 1
#define LWIP_UDP 1
#define LWIP_DNS 1
/** @} */

/** @name Configurações TCP */
/** @{ */
#define TCP_WND (8 * TCP_MSS)
#define TCP_MSS 1460
#define TCP_SND_BUF (8 * TCP_MSS)

/* Garante que o pool de segmentos TCP nunca seja menor que a fila de envio */
#define MEMP_NUM_TCP_SEG TCP_SND_QUEUELEN
/** @} */

#ifndef NDEBUG
#define LWIP_DEBUG 1
#define LWIP_STATS 1
#define LWIP_STATS_DISPLAY 1
#endif

#endif /* __LWIPOPTS_H__ */
