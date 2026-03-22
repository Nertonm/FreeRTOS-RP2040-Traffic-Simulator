import random
from typing import Dict, List, Union

from grid import Grid, TOTAL_ROWS, COLS, SPAWN_POINTS
from lock_manager import LockManager, Relogio, Semaforo, ControladorSemaforo
from car import Car
from ambulance import Ambulance
from render import CursesRenderer, Renderer


def criar_semaforos(grid: Grid) -> Dict[int, Semaforo]:
    """
    Cria pares de semáforos para cada cruzamento:
      - H (id 0-15): fluxo LESTE/OESTE — começa verde (fase inicial)
      - V (id 16-31): fluxo NORTE/SUL  — começa vermelho
    """
    semaforos: Dict[int, Semaforo] = {}
    for id_h in grid.semaforos_por_coordenada.values():
        if id_h >= 0 and id_h not in semaforos:
            sem_h = Semaforo(id_h)
            sem_h.abrir()
            semaforos[id_h] = sem_h

            sem_v = Semaforo(id_h + 16)   # par vertical, começa fechado
            semaforos[id_h + 16] = sem_v
    return semaforos


def criar_veiculos(
    grid: Grid,
    lock_manager: LockManager,
    relogio: Relogio,
    semaforos: Dict[int, Semaforo],
    quantidade: int,
) -> List[Union[Car, Ambulance]]:
    """
    Constrói instâncias de veículos e ambulâncias distribuídas pelos SPAWN_POINTS.
    O veículo de id 0 é sempre uma Ambulance. Os demais são Car com velocidades
    variadas (1 tick = rápido, 2 = médio, 4 = lento).
    """
    veiculos: List[Union[Car, Ambulance]] = []
    velocidades = [1, 2, 4]

    for i in range(quantidade):
        spawn = SPAWN_POINTS[i % len(SPAWN_POINTS)]
        posicao_global = (
            spawn.regiao * grid.linhas_por_regiao + spawn.linha,
            spawn.coluna,
        )

        if i == 0:
            veiculo: Union[Car, Ambulance] = Ambulance(
                id_carro=i,
                posicao_inicial=posicao_global,
                direcao_inicial=spawn.direcao,
                grid=grid,
                lock_manager=lock_manager,
                relogio=relogio,
                dicionario_semaforos=semaforos,
            )
        else:
            veiculo = Car(
                id_carro=i,
                velocidade=random.choice(velocidades),
                posicao_inicial=posicao_global,
                direcao_inicial=spawn.direcao,
                grid=grid,
                lock_manager=lock_manager,
                relogio=relogio,
                dicionario_semaforos=semaforos,
            )

        veiculos.append(veiculo)

    return veiculos


def iniciar_simulacao(
    grid: Grid,
    veiculos: List[Union[Car, Ambulance]],
    renderer: Renderer,
    relogio: Relogio,
    controlador: ControladorSemaforo,
) -> None:
    """Inicia o relógio global, o controlador de semáforos e todas as threads de veículos."""
    relogio.start()
    controlador.start()
    for veiculo in veiculos:
        veiculo.start()


def encerrar_simulacao(
    relogio: Relogio,
    veiculos: List[Union[Car, Ambulance]],
) -> None:
    """Sinaliza encerramento para todas as threads em execução."""
    relogio.em_execucao = False
    for veiculo in veiculos:
        veiculo.em_execucao = False


if __name__ == "__main__":
    grid = Grid(linhas_por_regiao=TOTAL_ROWS // 3, colunas=COLS)
    grid.carregar_mapa_padrao()

    lock_manager = LockManager(grid.rows, grid.cols)
    relogio = Relogio()

    semaforos = criar_semaforos(grid)
    veiculos = criar_veiculos(
        grid,
        lock_manager,
        relogio,
        semaforos,
        quantidade=20,
    )

    renderer = CursesRenderer(
        grid,
        veiculos,
        semaforos,
        relogio,
    )

    controlador = ControladorSemaforo(semaforos, relogio, ticks_fase=8)

    iniciar_simulacao(grid, veiculos, renderer, relogio, controlador)

    try:
        renderer.loop()
    except KeyboardInterrupt:
        controlador.em_execucao = False
        encerrar_simulacao(relogio, veiculos)
