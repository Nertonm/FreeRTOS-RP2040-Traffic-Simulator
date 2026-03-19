import threading

class Car(threading.Thread):
    def __init__(self, id, velocidade, posicao_inicial, grid, lock_manager):
        """
        Inicia uma thread de um veículo básico contendo velocidade, grid compartilhado
        e capacidade de lockar células por meio do lock_manager.
        """
        super().__init__()
        self.id = id
        self.velocidade = velocidade
        self.posicao_atual = posicao_inicial
        self.grid = grid
        self.lock_manager = lock_manager
        self.em_execucao = False

    def run(self):
        """
        Implementação do ciclo de vida da thread do carro, validando o tempo e controlando seu percurso da via.
        """
        pass

    def mover(self):
        """
        Move o carro para a próxima célula adjacente conforme
        direção da via. Respeita mão única e não teletransporta.
        """
        pass

    def verificar_semaforo(self):
        """
        Consulte as condições do semáforo à frente e toma a decisão de seguir ou bloquear a thread.
        """
        pass

    def pode_mover_para(self, x, y):
        """
        Checa e valida se o movimento em direção as coordenadas alvos é possível sem ferir 
        regras de trânsito.
        """
        pass
