import threading
import random
from typing import Tuple, Optional, Dict, TYPE_CHECKING

# Importando as direções do grid para o carro saber para onde virar
from grid import Direcao 

# Evitando import circular
if TYPE_CHECKING:
    from grid import Grid
    from lock_manager import LockManager, Relogio, Semaforo

class Car(threading.Thread):
    def __init__(self, id_carro: int, velocidade: int, posicao_inicial: Tuple[int, int], 
                 direcao_inicial: Direcao, grid: 'Grid', lock_manager: 'LockManager', 
                 relogio: 'Relogio', dicionario_semaforos: Dict[int, 'Semaforo']):
        """
        Thread de um veículo básico. Navega pelo grid respeitando os ticks do relógio, 
        a mão das vias, os sinais e os locks de concorrência.
        """
        super().__init__(daemon=True)
        self.id_carro: int = id_carro
        self.velocidade: int = velocidade
        self.posicao_atual: Tuple[int, int] = posicao_inicial
        self.direcao_atual: Direcao = direcao_inicial
        
        self.grid: 'Grid' = grid
        self.lock_manager: 'LockManager' = lock_manager
        
        self.relogio: 'Relogio' = relogio
        self.dicionario_semaforos: Dict[int, 'Semaforo'] = dicionario_semaforos
        
        self.ultima_posicao_no_cruzamento = None
        
        self.em_execucao: bool = False
        self.ticks_acumulados: int = 0

    def run(self) -> None:
        """
        Loop de vida do veículo. Controla o momento exato de andar com base na velocidade.
        """
        self.em_execucao = True
        nasceu = False
        
        # TENTA NASCER: Espera até a célula de spawn estar livre
        while self.em_execucao and not nasceu:
            nasceu = self.lock_manager.adquirir_celula(*self.posicao_atual)
            if not nasceu:
                self.relogio.esperar_tick()

        if not nasceu:
            return

        while self.em_execucao:
            self.relogio.esperar_tick()
            self.ticks_acumulados += 1

            # Chegou a hora de tentar andar?
            if self.ticks_acumulados >= self.velocidade:
                # CORREÇÃO 1: Só zera os ticks se o carro de fato conseguiu andar.
                # Se bater em um obstáculo, no próximo tick ele tenta de novo sem perder a vez.
                moveu_com_sucesso = self.mover()
                if moveu_com_sucesso:
                    self.ticks_acumulados = 0

        # Encerrou a simulação ou saiu do mapa. Libera a vaga que estava ocupando.
        if nasceu:
            self.lock_manager.liberar_celula(*self.posicao_atual)

    def mover(self) -> bool:
        """
        Tenta mover o carro para a próxima célula. 
        Retorna True se andou, False se foi bloqueado por sinal ou outro carro.
        """
        # CORREÇÃO 2: Escolher o caminho no cruzamento
        self.definir_direcao_cruzamento()

        prox_y, prox_x = self.calcular_proxima_posicao()

        # Validação física: Parede, contramão ou fim de mapa?
        if not self.pode_mover_para(prox_y, prox_x):
            # Se for None, tentou sair do mapa. O carro encerra a viagem.
            if self.grid.obter_celula(prox_y, prox_x) is None:
                self.em_execucao = False
            return False

        # Respeitar Sinal: Se estiver vermelho, a thread vai dormir aqui dentro
        self.verificar_semaforo(prox_y, prox_x)

        # CORREÇÃO 3: Exclusão Mútua mais segura
        # Só libera a célula antiga SE conseguir adquirir a nova com sucesso
        if self.lock_manager.adquirir_celula(prox_y, prox_x):
            y_antigo, x_antigo = self.posicao_atual
            
            # Atualiza o GPS interno
            self.posicao_atual = (prox_y, prox_x)
            
            # Libera a rua de trás em segurança
            self.lock_manager.liberar_celula(y_antigo, x_antigo)
            return True
            
        return False # Não conseguiu pegar o lock (tem carro na frente)

    def calcular_pos_com(self, direcao: Direcao) -> Tuple[int, int]:
        y, x = self.posicao_atual

        if direcao == Direcao.NORTE: return y - 1, x
        if direcao == Direcao.SUL: return y + 1, x
        if direcao == Direcao.LESTE: return y, x + 1
        if direcao == Direcao.OESTE: return y, x - 1
        
        raise ValueError(f"Direção inválida: {direcao}")

    def definir_direcao_cruzamento(self) -> None:
        """
        Se estiver em um cruzamento, escolhe uma nova direção permitida para virar.
        Possui uma trava para evitar que o carro dê um retorno (180 graus) na mesma via.
        """
        celula_atual = self.grid.obter_celula(*self.posicao_atual)
        
        # Só muda de direção se estiver pisando em um '[]' (cruzamento)
        if celula_atual and celula_atual.is_cruzamento:
            if self.posicao_atual != self.ultima_posicao_no_cruzamento:
                # Mapeamento do "caminho de volta" para evitar retorno
                direcao_reversa = {
                    Direcao.NORTE: Direcao.SUL,
                    Direcao.SUL: Direcao.NORTE,
                    Direcao.LESTE: Direcao.OESTE,
                    Direcao.OESTE: Direcao.LESTE
                }.get(self.direcao_atual)
    
                direcoes_possiveis = [Direcao.NORTE, Direcao.SUL, Direcao.LESTE, Direcao.OESTE]
                opcoes_validas = []
    
                for direcao in direcoes_possiveis:
                    if direcao == direcao_reversa:
                        continue
                        
                    # O carro pode ir para qualquer lado permitido pelo cruzamento, MENOS dar ré
                    if celula_atual.aceita_direcao(direcao):
                        prox_y, prox_x = self.calcular_pos_com(direcao)
                        celula_viz = self.grid.obter_celula(prox_y, prox_x)
                        
                        if celula_viz and celula_viz.aceita_direcao(direcao):
                            opcoes_validas.append(direcao)
                
                if opcoes_validas:
                    self.direcao_atual = random.choice(opcoes_validas)
                    
                self.ultima_posicao_no_cruzamento = self.posicao_atual
        else:
            self.ultima_posicao_no_cruzamento = None

    def verificar_semaforo(self, prox_y: int, prox_x: int) -> None:
        """
        Pausa a thread do veículo caso a próxima casa tenha um semáforo fechado.
        """
        id_semaforo = self.grid.obter_id_semaforo(prox_y, prox_x)

        if id_semaforo >= 0:
            semaforo = self.dicionario_semaforos.get(id_semaforo)
            if semaforo:
                semaforo.esperar_verde()

    def pode_mover_para(self, y: int, x: int) -> bool:
        """
        Valida se o bloco de destino aceita carros vindo da nossa direção atual.
        """
        celula_destino = self.grid.obter_celula(y, x)
        
        if not celula_destino or celula_destino.is_parede:
            return False

        return celula_destino.aceita_direcao(self.direcao_atual)

    def calcular_proxima_posicao(self) -> Tuple[int, int]:
        """
        Calcula o próximo índice (y, x) com base para onde o nariz do carro aponta.
        """
        y, x = self.posicao_atual

        if self.direcao_atual == Direcao.NORTE: return y - 1, x
        if self.direcao_atual == Direcao.SUL: return y + 1, x
        if self.direcao_atual == Direcao.LESTE: return y, x + 1
        if self.direcao_atual == Direcao.OESTE: return y, x - 1

        raise ValueError(f"Direção inválida: {self.direcao_atual}")