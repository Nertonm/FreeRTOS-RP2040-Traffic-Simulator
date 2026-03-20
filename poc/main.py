from typing import List, Union

from grid import Grid, TOTAL_ROWS, COLS, SPAWN_POINTS
from lock_manager import LockManager, Relogio, Semaforo
from car import Car
from ambulance import Ambulance
from render import Renderer


def criar_semaforos(grid: Grid) -> List[Semaforo]:
    """Instancia o objeto de tráfego em todo cruzamento mapeado no Grid."""
    return []


def criar_veiculos(grid: Grid, lock_manager: LockManager, quantidade: int) -> List[Union[Car, Ambulance]]:
    """
    Constrói instâncias iniciais de veículos e ambulâncias com base nos dados do ambiente.
    Retorna a lista de instâncias com as posições iniciais preenchidas.
    """
    return []


def iniciar_simulacao(grid: Grid, veiculos: List[Union[Car, Ambulance]], renderer: Renderer, relogio: Relogio) -> None:
    """
    Ativador sincronizado global das threads dos diferentes agentes, delegando a atuação para cada 
    instância rodar ao mesmo tempo.
    """
    pass


def encerrar_simulacao() -> None:
    """
    Metodologia segura de parar fluxos em andamento de todas as threads que rodam paralelamente e 
    assegurar limpeza de recursos.
    """
    pass


if __name__ == "__main__":
    grid_instancia = Grid(linhas_por_regiao=TOTAL_ROWS // 3, colunas=COLS)
    grid_instancia.carregar_mapa_padrao()

    lock_manager_instancia = LockManager(grid_instancia)
    relogio_instancia = Relogio()
    renderer_instancia = Renderer(grid_instancia)

    semaforos = criar_semaforos(grid_instancia)
    veiculos_instancias = criar_veiculos(grid_instancia, lock_manager_instancia, quantidade=20)

    # Dar partida as threads, iniciando o evento global
    iniciar_simulacao(grid_instancia, veiculos_instancias, renderer_instancia, relogio_instancia)

    # Mantendo simulador ativo
    try:
        pass
    except KeyboardInterrupt:
        encerrar_simulacao()
