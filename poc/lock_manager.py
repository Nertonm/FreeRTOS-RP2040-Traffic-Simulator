import logging
import threading
from typing import List, Tuple, TYPE_CHECKING

if TYPE_CHECKING:
    from grid import Grid

logger = logging.getLogger(__name__)


class LockManager:
    """Gerencia a concorrência espacial no grid, evitando colisões de veículos."""

    def __init__(self, rows: int, cols: int) -> None:
        self.rows = rows
        self.cols = cols
        
        # Uma trava (Lock) exclusiva por coordenada do mapa
        self.locks: List[List[threading.Lock]] = [
            [threading.Lock() for _ in range(cols)]
            for _ in range(rows)
        ]

    def _verificar_limites(self, row: int, col: int) -> None:
        """Garante que a coordenada (row, col) pertence ao grid."""
        if not (0 <= row < self.rows and 0 <= col < self.cols):
            raise ValueError(f"Coordenada ({row}, {col}) fora do grid {self.rows}x{self.cols}.")

    def adquirir_celula(self, row: int, col: int) -> bool:
        """Tenta reservar a célula informada. Retorna True se reservada, False se ocupada."""
        self._verificar_limites(row, col)
        return self.locks[row][col].acquire(blocking=False)

    def liberar_celula(self, row: int, col: int) -> None:
        """Devolve o controle da célula para que outro veículo possa ocupar."""
        self._verificar_limites(row, col)
        self.locks[row][col].release()

    def adquirir_multiplas_celulas(self, posicoes: List[Tuple[int, int]]) -> bool:
        """
        Reserva múltiplas células sequencialmente. 
        Evita deadlocks exigindo que as aquisições sigam a ordem lexicográfica.
        Se uma célula falhar, desfaz todas as reservas feitas e retorna False.
        """
        # Remove duplicatas (set) para evitar que a mesma thread tente pegar um
        # Lock não-reentrante duas vezes e cause falha fantasma
        coordenadas_ordenadas = sorted(set(posicoes))
        coordenadas_reservadas: List[Tuple[int, int]] = []

        for row, col in coordenadas_ordenadas:
            if self.adquirir_celula(row, col):
                coordenadas_reservadas.append((row, col))
            else:
                # Rollback: libera tudo que já foi reservado nesta tentativa
                for row_reservada, col_reservada in coordenadas_reservadas:
                    self.liberar_celula(row_reservada, col_reservada)
                return False

        return True


class Relogio(threading.Thread):
    """Provedor do tempo global da simulação."""

    def __init__(self) -> None:
        super().__init__(daemon=True)
        self.em_execucao = False
        self.tick_event = threading.Event()

    def run(self) -> None:
        pass  # TODO: loop principal de emissão de ticks

    def esperar_tick(self) -> None:
        pass  # TODO: sincronia para as demais threads


class Semaforo:
    """
    Controla o acesso de veículos em um cruzamento específico.
    Utiliza uma Condition Variable para permitir que os veículos aguardem (durmam) 
    o sinal verde sem consumir processamento (busy-wait).
    """

    def __init__(self, cruzamento_id: int) -> None:
        self.cruzamento_id = cruzamento_id
        self.estado_aberto = False
        self.condicao = threading.Condition(threading.Lock())

    def abrir(self) -> None:
        """Sinaliza 'verde' e desperta imediatamente todos os veículos em espera."""
        with self.condicao:
            self.estado_aberto = True
            self.condicao.notify_all()

    def fechar(self) -> None:
        """Sinaliza 'vermelho'. Veículos que chegarem agora irão dormir."""
        with self.condicao:
            self.estado_aberto = False

    def forcar_verde(self) -> None:
        """Sinaliza 'verde' absoluto. Destinado apenas à passagem de ambulâncias."""
        with self.condicao:
            logger.debug("[EMERGÊNCIA] Semáforo %d forçado para verde.", self.cruzamento_id)
            self.estado_aberto = True
            self.condicao.notify_all()

    def esperar_verde(self) -> None:
        """
        Bloqueia a execução de um veículo enquanto o semáforo estiver vermelho.
        A thread é efetivamente pausada, poupando CPU.
        """
        with self.condicao:
            while not self.estado_aberto:
                self.condicao.wait()
