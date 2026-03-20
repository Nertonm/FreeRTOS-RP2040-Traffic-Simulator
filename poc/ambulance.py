from typing import Tuple
from car import Car

# Evitando import circular
from typing import TYPE_CHECKING
if TYPE_CHECKING:
    from grid import Grid
    from lock_manager import LockManager

class Ambulance(Car):
    def __init__(self, id_carro: int, posicao_inicial: Tuple[int, int], grid: 'Grid', lock_manager: 'LockManager'):
        """
        Inicia uma ambulância na memória, que usufrui da base do objeto de um carro 
        normal para adicionar comportamento prioritário.
        """
        # Ambulancia nao tem limitador estrito de velocidade normal, sendo a mais veloz
        super().__init__(id_carro=id_carro, velocidade=1, posicao_inicial=posicao_inicial, grid=grid, lock_manager=lock_manager)
        
        self.prioridade_ativada: bool = False

    def run(self) -> None:
        """
        Fluxo principal de comportamento, mesclando o roteamento padrão e garantindo passagem em emergências.
        """
        pass

    def acionar_prioridade(self, id_cruzamento: int) -> None:
        """
        Ativa privilégios de alto tráfego no cruzamento destino, manipulando semáforos ou removendo bloqueios.
        """
        pass
