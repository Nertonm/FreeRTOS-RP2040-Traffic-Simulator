from typing import Tuple, Dict
from car import Car
from grid import Direcao

# Evitando import circular para manter o código limpo
from typing import TYPE_CHECKING
if TYPE_CHECKING:
    from grid import Grid
    from lock_manager import LockManager, Relogio, Semaforo

class Ambulance(Car):
    def __init__(self, id_carro: int, posicao_inicial: Tuple[int, int], 
                 direcao_inicial: Direcao, grid: 'Grid', lock_manager: 'LockManager', 
                 relogio: 'Relogio', dicionario_semaforos: Dict[int, 'Semaforo']):
        """
        Inicia uma ambulância na memória. Herda de Carro, mas possui 
        velocidade máxima cravada e privilégio de forçar semáforos.
        """
        # Chamamos o construtor da classe pai (Car), mas forçamos a velocidade = 1
        # Isso garante que ela se mova a cada 1 tick do relógio (o mais rápido possível)
        super().__init__(
            id_carro=id_carro, 
            velocidade=1, 
            posicao_inicial=posicao_inicial, 
            direcao_inicial=direcao_inicial,
            grid=grid, 
            lock_manager=lock_manager,
            relogio=relogio,
            dicionario_semaforos=dicionario_semaforos
        )
        
        self.prioridade_ativada: bool = True

    def verificar_semaforo(self, prox_y: int, prox_x: int) -> None:
        """
        SOBRESCRITA:
        A ambulância possui um "radar" que olha várias células à frente na mesma rua.
        Se ela detectar um semáforo (mesmo com carros no meio do caminho), ela força o verde.
        """
        # Começamos a olhar a partir da célula imediatamente à frente
        y_scan, x_scan = prox_y, prox_x
        
        # A ambulância "enxerga" até 6 blocos à frente (ajuste conforme o tamanho das ruas)
        alcance_visao = 6 
        
        for _ in range(alcance_visao):
            celula_alvo = self.grid.obter_celula(y_scan, x_scan)
            
            # Se o radar bateu no fim do mapa ou numa parede, não tem semáforo nessa rua. Para de procurar.
            if not celula_alvo or celula_alvo.is_parede:
                break
                
            # Achamos um semáforo no alcance do radar!
            id_semaforo = celula_alvo.light_id
            if id_semaforo >= 0:
                semaforo = self.dicionario_semaforos.get(id_semaforo)
                if semaforo:
                    # Grita para o semáforo abrir e acorda todos os carros da fila
                    semaforo.forcar_verde()
                break # Já abriu o sinal, o radar cumpriu seu papel
                
            # Se não achou, avança o radar mais um bloco para a frente na mesma direção
            y_scan, x_scan = self._avancar_radar(y_scan, x_scan)

    def _avancar_radar(self, y: int, x: int) -> Tuple[int, int]:
        """Função auxiliar para fazer o radar andar em linha reta."""
        if self.direcao_atual == Direcao.NORTE: return y - 1, x
        if self.direcao_atual == Direcao.SUL: return y + 1, x
        if self.direcao_atual == Direcao.LESTE: return y, x + 1
        if self.direcao_atual == Direcao.OESTE: return y, x - 1
        return y, x