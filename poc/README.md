# Simulador de Tráfego Urbano (POC)

Esta prova de conceito implementa a arquitetura básica de um simulador de tráfego concorrente. O objetivo principal é modelar um ambiente onde veículos navegam de forma assíncrona respeitando regras de trânsito, semáforos e concorrência física.

## Estrutura de Arquivos

*   **main.py**: Ponto de entrada. Instancia o mapa, os gerenciadores e as threads dos veículos.
*   **grid.py**: Estrutura do mapa. Define a malha viária, cruzamentos e direções permitidas.
*   **lock_manager.py**: Gerenciamento de concorrência. Contém o `LockManager` para o acesso exclusivo às células métricas, o `Relogio` global responsável pelos ticks de tempo e os objetos `Semaforo`.
*   **car.py**: Thread base dos veículos. Define a lógica de movimentação, verificação de semáforos e respeito à mão de direção.
*   **ambulance.py**: Veículo de emergência. Herda de `Car` e adiciona a capacidade de forçar a abertura de semáforos e solicitar prioridade de passagem.
*   **render.py**: Motor de visualização. Thread focada em exibir o estado atual do grid e dos atores.
