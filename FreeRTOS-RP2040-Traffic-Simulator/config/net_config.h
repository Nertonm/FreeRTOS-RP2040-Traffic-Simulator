/**
 * @file net_config.h
 * @brief Configurações de credenciais e parâmetros de rede WiFi.
 *
 * @author
 * @date 2026-03-01
 */

#ifndef NET_CONFIG_H
#define NET_CONFIG_H

/* Inclusão opcional de secrets.h para builds locais.
 * Copie firmware/config/secrets.h.template para firmware/config/secrets.h
 * e preencha com suas credenciais reais. Nunca commite secrets.h. */
#if __has_include("secrets.h")
#include "secrets.h"
#endif

/** @name Credenciais WiFi */
/** @{ */
#ifndef WIFI_SSID
#define WIFI_SSID "" /**< Nome do ponto de acesso WiFi. */
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "" /**< Senha da rede WiFi. */
#endif
/** @} */

/* FALLBACK_SERVER_IP não é definido aqui intencionalmente.
 * O valor padrão real (192.168.0.10) vive em rpc_client.c via #ifndef guard.
 * Sobrescreva com -DFALLBACK_SERVER_IP="x.x.x.x" no cmake ou via secrets.h. */

#endif /* NET_CONFIG_H */
