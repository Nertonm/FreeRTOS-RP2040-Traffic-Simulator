import threading

class Renderer(threading.Thread):
    def __init__(self, grid):
        """
        Inicia os parâmetros de configuração e a thread principal de renderização do simulador gráfico/terminal.
        """
        super().__init__()
        self.grid = grid
        self.em_execucao = False

    def run(self):
        """
        Gatilho contínuo da thread de exibição, responsável por coordenar as repetições de frames visuais.
        """
        pass

    def desenhar_frame(self):
        """
        Limpa a tela e compõe os visuais correspondentes ao estado instantâneo do grid, carros e sinais.
        """
        pass

    def desenhar_veiculo(self, veiculo):
        """
        Aplica as marcações visuais necessárias nas coordenadas atuais de um veículo determinado.
        """
        pass

    def desenhar_semaforo(self, semaforo):
        """
        Ilustração gráfica referente ao estado aberto ou fechado (verde/vermelho) de um semáforo individual.
        """
        pass
