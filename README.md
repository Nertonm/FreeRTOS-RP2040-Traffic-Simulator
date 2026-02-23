# FreeRTOS-RP2040-Traffic-Simulator

Simulador de tráfego urbano distribuído em três Raspberry Pi Pico W, desenvolvido como projeto final da disciplina de Programação Concorrente (UFCA), 2025.2.

---

## Descrição

O sistema simula uma malha viária dividida em três regiões geográficas (Norte, Centro e Sul), onde cada região é gerenciada por uma placa independente. Carros são tasks FreeRTOS que competem por células da
malha usando exclusão mútua. Uma ambulância possui prioridade absoluta e força semáforos verdes ao se aproximar.

A comunicação entre placas é feita via TCP/IP sobre WiFi. O relógio global é sincronizado por ... .

A visualização ocorre em dois níveis:
- **OLED 128x64**: visão detalhada do quarteirão local
- **Matriz LED 5x5**: estado geral de toda a região

---

## Arquitetura

### Hardware

| Placa   | Região  | Cidade      |
|---------|---------|-------------|
| Pico W  | Norte   | Crato       |
| Pico W  | Centro  | Juazeiro    |
| Pico W  | Sul     | Barbalha    |

Cada placa possui:
- Display OLED SSD1306 128x64 (I2C)
- Matriz LED WS2812B 5x5 (GPIO7, via PIO)
- Botões A, B, C
- Joystick analógico (ADC)
- LED RGB (PWM)
- Buzzer (PWM)

### Topologia de Rede

```
[Norte] - TCP - [Centro] - TCP - [Sul]
```

Centro atua como servidor TCP. Norte e Sul conectam como clientes. Toda comunicação entre regiões passa pelo Centro.

### Tasks FreeRTOS

| Task               | Prioridade | Responsabilidade              |
|--------------------|------------|-------------------------------|
| Ambulância         | 4          | Movimento prioritário         |
| Relógio Global     | 3          | Tick sincronizado (DEFINIR)   |
| Carros (N tasks)   | 2          | Movimento e exclusão mútua    |
| Semáforos          | 1          | Ciclo verde/vermelho          |
| Display            | 0          | Atualização OLED e LED        |

### Sincronização

- **Mutex por célula**: 75 `SemaphoreHandle_t`, um por célula do grid.
  - Avaliar se uma por placa server
- **Event Groups**: carros dormem no vermelho via `xEventGroupWaitBits()`
- **Lock único**: carro nunca segura dois mutexes simultaneamente
- **Backoff exponencial**: dessincronização automática em contenção

---

## Requisitos

### Software

- [Pico SDK](https://github.com/raspberrypi/pico-sdk) v2.2.0
- [FreeRTOS-Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel)
- [picotool](https://github.com/raspberrypi/picotool) v2.2.0 (opcional)

### Hardware

- 3x Raspberry Pi Pico W
- 3x Display OLED SSD1306 128x64 (I2C)
- 3x Matriz LED WS2812B 5x5
- Rede WiFi 2.4 GHz disponível
- Cabo USB para gravação

---

## Instalação

### 1. Clonar o repositório

```bash
git clone https://github.com/Nertonm/FreeRTOS-RP2040-Traffic-Simulator.git
cd FreeRTOS-RP2040-Traffic-Simulator
```

### 2. Configurar credenciais WiFi

Edite o `CMakeLists.txt` na seção `target_compile_definitions`:

```cmake
target_compile_definitions(FreeRTOS-RP2040-Traffic-Simulator PRIVATE
    WIFI_SSID="sua_rede"
    WIFI_PASSWORD="sua_senha"
    CYW43_HOST_NAME="semafreeRP"
)
```
O IP do servidor (placa Centro) deve ser configurado antes de compilar os binários de Norte e Sul. Descubra o IP do Centro conectando via USB e observando o log serial após a primeira inicialização.

---

## Compilação

### Via extensão VS Code (recomendado)

1. Abrir a pasta `FreeRTOS-RP2040-Traffic-Simulator/` no VS Code
2. A extensão Raspberry Pi Pico detecta o projeto automaticamente
3. Clicar em **Build** na barra inferior

### Via terminal

cmake

Binário gerado:

```
build/FreeRTOS-RP2040-Traffic-Simulator.uf2
```

---

## Gravação nas Placas

1. Segurar o botão **BOOTSEL** ao conectar o cabo USB
2. A placa monta como unidade de armazenamento (`RPI-RP2`)
3. Copiar o `.uf2` para a unidade

```bash
# Linux
cp build/FreeRTOS-RP2040-Traffic-Simulator.uf2 /media/$USER/RPI-RP2/
```

---

## Execução

### Ordem de inicialização

1. Ligar a placa **Centro** primeiro — inicia o servidor TCP
2. Ligar **Norte** e **Sul** — conectam ao Centro
3. Aguardar LED verde em todas as placas — sistema pronto

### Verificação no OLED

A barra inferior do display indica o estado da conexão:

| Indicador   | Significado              |
|-------------|--------------------------|
| `WiFi:OK`   | Conectado à rede         |
| `WiFi:CONN` | Conectando...            |
| `WiFi:DISC` | Sem conexão (checar IP)  |

---

## Controles

| Controle       | Ação                              |
|----------------|-----------------------------------|
| Botão A        | Spawna carro aleatório            |
| Botão B        | Spawna ambulância                 |
| Botão C        | Pausa / Resume simulação          |
| Joystick Y+    | Aumenta velocidade (até 2.0x)     |
| Joystick Y-    | Diminui velocidade (até 0.5x)     |

---

## Visualização

### OLED (visão por região)

```
│ CENTRO  | Tick:0042 | C:07  │
RUA COM O CARRO E SINAL IMAGEM
 ▸PLAY 1.0x    WiFi:OK


[C] = Carro normal
[A] = Ambulância
[●] = Semáforo verde
[○] = Semáforo vermelho
```

### Matriz LED (visão macro da região)

| Cor            | Significado               |
|----------------|---------------------------|
| Verde          | 0–2 carros no quarteirão  |
| Amarelo        | 3–4 carros                |
| Vermelho       | 5+ carros (congestionado) |
| Azul piscante  | Ambulância presente       |
| Roxo piscante  | Transferência em curso    |

---

## Protocolo de Comunicação

Mensagens trocadas entre placas (formato binário, máx. 128 bytes):

| Tipo                  | Direção                         | Descrição                         |
|-----------------------|---------------------------------|-----------------------------------|
| `MSG_TRANSFER_CAR`    | Norte/Sul → Centro → Destino    | Migração de carro entre regiões   |
| `MSG_SYNC_TICK`       | Centro → Norte/Sul              | Sincronização de relógio          |
| `MSG_HEARTBEAT`       | Bidirecional                    | Keep-alive a cada 5s              |

---

## Decisões de Design

**Lock único por célula**
Carros adquirem lock apenas da célula destino, nunca da origem.
Isso torna matematicamente impossível a criação de ciclos de espera,
prevenindo deadlock por construção.

**Event Groups no lugar de condition variables**
FreeRTOS não possui `pthread_cond_t`. `EventGroupHandle_t` oferece
comportamento equivalente com broadcast automático e integração
nativa com o scheduler, sem consumo de CPU em espera.

**PIO para WS2812B**
O protocolo one-wire da matriz LED exige timing preciso (~800 kHz).
O uso do PIO do RP2040 garante esse timing sem interferir nas demais tasks.

**WiFi no Core 0**
O driver CYW43 exige execução exclusiva no Core 0. Toda a lógica
de rede é isolada neste core. Carros, semáforos e display operam
no Core 1.

---

## Estrutura do Projeto

```
FreeRTOS-RP2040-Traffic-Simulator/

```

---

## Análise de Memória

| Componente          | Estimativa |
|---------------------|------------|
| Mutexes (75)        | 7.5 KB     |
| Tasks carros (20)   | 10.0 KB    |
| Grid global         | 11.0 KB    |
| WiFi stack (CYW43)  | 40.0 KB    |
| FreeRTOS kernel     | 5.0 KB     |
| Buffers de display  | 2.5 KB     |
| **Total estimado**  | **~76 KB** |

RP2040 dispõe de 264 KB de RAM. Margem aproximada de 188 KB livre.

---

## Critérios de Avaliação Atendidos

| Critério                                    | Implementação               |
|---------------------------------------------|-----------------------------|
| Exclusão mútua (mutex)                      | Mutex por célula            |
| Semáforos de trânsito funcionando           | Event Groups                |
| Threads dormem no vermelho                  | `xEventGroupWaitBits()`     |
| Ambulância força semáforos verdes           | Prioridade 4 + broadcast    |
| Ausência de deadlock                        | Lock único + backoff        |
| 8+ cruzamentos                              | Distribuídos nas 3 regiões  |
| 10–20 carros simultâneos                    | Tasks dinâmicas             |
| Interface visual (não apenas logs)          | OLED + Matriz LED           |
| Mão única implementada                      | Enum de direção por célula  |
| 3 velocidades distintas                     | Contador por task           |
| Versionamento Git com commits de todos      | Repositório público         |

---

## Equipe

| Nome              | 
|-------------------|
| Thiago Nerton     |
| David Hudson      |

---
