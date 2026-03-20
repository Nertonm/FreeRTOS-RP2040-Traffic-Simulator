import threading
from typing import Tuple, Optional, Dict

# Importando o Enum de direções no grid
from grid import Direcao 

# Evitando import circular (boa prática pra manter o código limpo)
from typing import TYPE_CHECKING
if TYPE_CHECKING:
    from grid import Grid
    from lock_manager import LockManager, Relogio, Semaforo

class Car(threading.Thread):
    def __init__(self, id_carro: int, velocidade: int, posicao_inicial: Tuple[int, int], 
                 direcao_inicial: Direcao, grid: 'Grid', lock_manager: 'LockManager', 
                 relogio: 'Relogio', dicionario_semaforos: Dict[int, 'Semaforo']):
        """
        Thread de um veículo básico. Navega pelo grid respeitando os ticks do relógio, 
        a mão das vias e os locks de concorrência.
        """
        super().__init__()
        self.id_carro: int = id_carro
        self.velocidade: int = velocidade
        self.posicao_atual: Tuple[int, int] = posicao_inicial
        self.direcao_atual: Direcao = direcao_inicial
        
        self.grid: 'Grid' = grid
        self.lock_manager: 'LockManager' = lock_manager
        
        # Injeções de dependência necessárias para a física e sinais funcionarem
        self.relogio: 'Relogio' = relogio
        self.dicionario_semaforos = dicionario_semaforos
        
        self.em_execucao: bool = False
        self.ticks_acumulados: int = 0

    def run(self) -> None:
        """
        Loop de vida do veículo. Controla o momento exato de andar com base na velocidade.
        """
        self.em_execucao = True
        
        # TENTA NASCER: Como o Thiago usou blocking=False no acquire, se tiver um carro no spawn, 
        # a gente espera o próximo tick do relógio pra tentar nascer de novo.
        while self.em_execucao and not self.lock_manager.adquirir_celula(*self.posicao_atual):
            self.relogio.esperar_tick()

        while self.em_execucao:
            # Sincroniza a thread com o compasso do simulador (evita que o carro corra solto)
            self.relogio.esperar_tick()
            self.ticks_acumulados += 1

            # Apenas tenta se mover se acumulou "energia" suficiente (ticks da sua categoria)
            if self.ticks_acumulados >= self.velocidade:
                self.mover()
                self.ticks_acumulados = 0

        # O carro saiu do while (fim do mapa ou simulação encerrada). Libera a vaga.
        self.lock_manager.liberar_celula(*self.posicao_atual)

    def mover(self) -> None:
        """
        Calcula o próximo passo, valida regras de trânsito e, se possível, efetua a troca de células
        usando Exclusão Mútua para não bater em ninguém.
        """
        prox_y, prox_x = self.calcular_proxima_posicao()

        # 1. Validação física: Parede, contramão ou fim de mapa?
        if not self.pode_mover_para(prox_y, prox_x):
            # Se for None, significa que tentou sair do mapa. O carro encerra a viagem.
            if self.grid.obter_celula(prox_y, prox_x) is None:
                self.em_execucao = False
            return

        # 2. Respeitar Sinal: Checa a próxima célula ANTES de pedir o Lock dela.
        self.verificar_semaforo(prox_y, prox_x)

        # 3. Exclusão Mútua:
        # Se retornar True, a célula tá livre e agora é nossa. Se False, tem carro na frente.
        if self.lock_manager.adquirir_celula(prox_y, prox_x):
            y_antigo, x_antigo = self.posicao_atual
            
            # Atualiza o "GPS" interno
            self.posicao_atual = (prox_y, prox_x)
            
            # Libera a rua de trás pro colega poder andar
            self.lock_manager.liberar_celula(y_antigo, x_antigo)

    def verificar_semaforo(self, prox_y: int, prox_x: int) -> None:
        """
        Se a próxima casa tiver semáforo, garante que a thread hiberne se estiver vermelho.
        """
        id_semaforo = self.grid.obter_id_semaforo(prox_y, prox_x)

        if id_semaforo >= 0:
            semaforo = self.dicionario_semaforos.get(id_semaforo)
            if semaforo:
                # O Condition.wait() do Thiago entra em ação aqui, zerando o uso de CPU.
                semaforo.esperar_verde()

    def pode_mover_para(self, y: int, x: int) -> bool:
        """
        Valida se o bloco de destino aceita carros vindo da nossa direção.
        """
        celula_destino = self.grid.obter_celula(y, x)
        
        # Bateu num prédio, calçada ou caiu no abismo
        if not celula_destino or celula_destino.is_parede:
            return False

        # Delega para a inteligência da própria célula se ela aceita o nosso sentido (Ex: Mão única)
        return celula_destino.aceita_direcao(self.direcao_atual)

    def calcular_proxima_posicao(self) -> Tuple[int, int]:
        """
        Matemática básica de matriz para saber qual o próximo índice baseado pra onde o nariz do carro aponta.
        """
        y, x = self.posicao_atual

        if self.direcao_atual == Direcao.NORTE:
            return y - 1, x
        elif self.direcao_atual == Direcao.SUL:
            return y + 1, x
        elif self.direcao_atual == Direcao.LESTE:
            return y, x + 1
        elif self.direcao_atual == Direcao.OESTE:
            return y, x - 1

        return y, x