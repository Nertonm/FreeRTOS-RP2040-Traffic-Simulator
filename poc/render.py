import threading

# Evitando import circular
from typing import TYPE_CHECKING
if TYPE_CHECKING:
    from grid import Grid
    from car import Car
    from lock_manager import Semaforo

class Renderer(threading.Thread):
    def __init__(self, grid: 'Grid'):
        """
        Inicia os parâmetros de configuração e a thread principal de renderização do simulador gráfico/terminal.
        """
        super().__init__()
        self.grid: 'Grid' = grid
        self.em_execucao: bool = False

    def run(self) -> None:
        """
        Gatilho contínuo da thread de exibição, responsável por coordenar as repetições de frames visuais.
        """
        pass

    def desenhar_frame(self) -> None:
        """
        Limpa a tela e compõe os visuais correspondentes ao estado instantâneo do grid, carros e sinais.
        """
        pass

    def desenhar_veiculo(self, veiculo: 'Car') -> None:
        """
        Aplica as marcações visuais necessárias nas coordenadas atuais de um veículo determinado.
        """
        pass

    def desenhar_semaforo(self, semaforo: 'Semaforo') -> None:
        """
        Ilustração gráfica referente ao estado aberto ou fechado (verde/vermelho) de um semáforo individual.
        """
        pass
