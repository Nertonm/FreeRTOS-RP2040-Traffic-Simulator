from car import Car

class Ambulance(Car):
    def __init__(self, id, posicao_inicial, grid, lock_manager):
        """
        Inicia uma ambulância na memória, que usufrui da base do objeto de um carro 
        normal para adicionar comportamento prioritário.
        """
        super().__init__(id=id, velocidade=None, posicao_inicial=posicao_inicial, grid=grid, lock_manager=lock_manager)
        self.prioridade_ativada = False

    def run(self):
        """
        Fluxo principal de comportamento, mesclando o roteamento padrão e garantindo passagem em emergências.
        """
        pass

    def acionar_prioridade(self, cruzamento):
        """
        Ativa privilégios de alto tráfego no cruzamento destino, manipulando semáforos ou removendo bloqueios.
        """
        pass
