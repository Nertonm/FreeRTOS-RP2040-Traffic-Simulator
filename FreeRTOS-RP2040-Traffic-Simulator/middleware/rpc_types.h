/**
 * @file rpc_types.h
 * @brief Definições de tipos compartilhados para comunicação RPC.
 *
 * Centraliza estruturas e enums usados tanto pela camada de rede (rpc_client)
 * quanto pelo middleware (shared_state), evitando dependências circulares.
 */

#ifndef RPC_TYPES_H
#define RPC_TYPES_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Tipos de mensagem RPC (mantendo heartbeat do clicker + simulador).
 */
typedef enum {
  MSG_HEARTBEAT = 0x01,    /**< Keep-alive/registro existente do clicker. */
  MSG_SYNC_TICK = 0x10,    /**< Centro → Norte/Sul: uint32_t tick_value. */
  MSG_TRANSFER_CAR = 0x11, /**< Nós → Centro → Destino: CarState struct. */
  MSG_ACK = 0x12,          /**< Confirmação de transferência. */
} MsgType;

/**
 * @brief Estrutura padrão para troca de mensagens TCP no simulador.
 */
typedef struct {
  MsgType type;
  uint8_t src_node;
  uint8_t dst_node;
  uint8_t payload[64];
  uint8_t payload_len;
} RpcMessage;

/**
 * @brief Códigos de erro retornados pelas operações RPC.
 */
typedef enum {
  RPC_OK = 0,            /**< Operação bem-sucedida. */
  RPC_TIMEOUT,           /**< Servidor não respondeu dentro do tempo limite. */
  RPC_DISCONNECTED,      /**< Socket fechado ou sem conexão física. */
  RPC_LAMPORT_VIOLATION, /**< Servidor rejeitou a requisição por timestamp inválido. */
  RPC_RATE_EXCEEDED,     /**< Servidor limitou a taxa de cliques (anti-spam). */
  RPC_PARSE_ERROR        /**< Resposta JSON do servidor está mal formatada. */
} RpcError;

/**
 * @brief Resultado detalhado de operações de clique e sincronização.
 *
 * @note Campos não preenchidos nos resultados são inicializados com zero (memset).
 */
typedef struct {
  bool success;             /**< Verdadeiro se a operação foi bem-sucedida. */
  RpcError error_code;      /**< Código de erro detalhado. */
  int global_score;         /**< Pontuação global acumulada de todos os nós. */
  int local_score;          /**< Pontuação individual deste nó. */
  uint32_t lamport_ts;      /**< Timestamp Lamport atualizado pelo servidor. */
  bool milestone_triggered; /**< Indica se um novo marco de pontuação foi atingido. */
  int milestone_value;     /**< Valor do marco atingido (válido se milestone_triggered for true). */
  int accepted_clicks;     /**< Quantidade de cliques aceitos pelo servidor. */
  int powerup_remaining_s; /**< Segundos de power-up ativo retornados pelo servidor (0 se inativo).
                            */
} RpcClickResult;

#endif // RPC_TYPES_H
