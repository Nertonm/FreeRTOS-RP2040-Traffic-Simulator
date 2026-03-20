import threading
from typing import Tuple, Optional

# Evitando import circular
from typing import TYPE_CHECKING
if TYPE_CHECKING:
    from grid import Grid
    from lock_manager import LockManager

class Car(threading.Thread):
    def __init__(self, id_carro: int, velocidade: int, posicao_inicial: Tuple[int, int], grid: 'Grid', lock_manager: 'LockManager'):
        """
        Inicia uma thread de um veículo básico contendo velocidade, grid compartilhado
        e capacidade de lockar células por meio do lock_manager.
        """
        super().__init__()
        self.id_carro: int = id_carro
        self.velocidade: int = velocidade
        self.posicao_atual: Tuple[int, int] = posicao_inicial
        
        self.grid: 'Grid' = grid
        self.lock_manager: 'LockManager' = lock_manager
        
        self.em_execucao: bool = False

    def run(self) -> None:
        """
        Implementação do ciclo de vida da thread do carro, validando o tempo e controlando seu percurso da via.
        """
        pass

    def mover(self) -> None:
        """
        Move o carro para a próxima célula adjacente conforme
        direção da via. Respeita mão única e não teletransporta.
        """
        pass

    def verificar_semaforo(self) -> None:
        """
        Consulte as condições do semáforo à frente e toma a decisão de seguir ou bloquear a thread.
        """
        pass

    def pode_mover_para(self, y: int, x: int) -> bool:
        """
        Checa e valida se o movimento em direção as coordenadas alvos é possível sem ferir 
        regras de trânsito.
        """
        return False
