class Grid:
    def __init__(self, rows, cols):
        """
        Inicializa o grid com as dimensões especificadas.
        """
        self.rows = rows
        self.cols = cols
        self.mapa = None
        self.cruzamentos = None
        self.vias = None

    def criar_mapa(self):
        """
        Cria a estrutura de dados interna para representar o mapa da cidade.
        """
        pass

    def adicionar_cruzamento(self, x, y):
        """
        Registra um cruzamento nas coordenadas informadas de (x, y).
        """
        pass

    def definir_via(self, x1, y1, x2, y2, direcao):
        """
        Define uma via de tráfego entre dois pontos e estabelece sua direção.
        """
        pass

    def get_celula(self, x, y):
        """
        Retorna as informações contidas na célula localizada nas coordenadas (x, y).
        """
        pass

    def esta_ocupada(self, x, y):
        """
        Verifica se a célula especificada por (x, y) encontra-se ocupada atualmente.
        """
        pass
