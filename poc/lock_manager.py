import logging
import threading
import time
from typing import Dict, List, Tuple, TYPE_CHECKING

if TYPE_CHECKING:
    from grid import Grid

logger = logging.getLogger(__name__)


class LockManager:
    """Gerencia a concorrência espacial no grid, evitando colisões de veículos."""

    def __init__(self, rows: int, cols: int) -> None:
        self.rows: int = rows
        self.cols: int = cols
        
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


TICK_DURATION: float = 0.2  # segundos por tick

class Relogio(threading.Thread):
    """Provedor do tempo global da simulação."""

    def __init__(self) -> None:
        super().__init__(daemon=True)
        self.em_execucao: bool = False
        self._condicao: threading.Condition = threading.Condition(threading.Lock())
        self._tick_numero: int = 0

    def run(self) -> None:
        self.em_execucao = True
        while self.em_execucao:
            time.sleep(TICK_DURATION)
            with self._condicao:
                self._tick_numero += 1
                self._condicao.notify_all()

    def esperar_tick(self) -> None:
        """Bloqueia a thread chamadora até o próximo tick do relógio global."""
        with self._condicao:
            tick_atual = self._tick_numero
            while self._tick_numero == tick_atual:
                self._condicao.wait()


class Semaforo:
    """
    Controla o acesso de veículos em um cruzamento específico.
    Utiliza uma Condition Variable para permitir que os veículos aguardem (durmam) 
    o sinal verde sem consumir processamento (busy-wait).
    """

    def __init__(self, cruzamento_id: int) -> None:
        self.cruzamento_id: int = cruzamento_id
        self.estado_aberto: bool = False
        self.condicao: threading.Condition = threading.Condition(threading.Lock())

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


class ControladorSemaforo(threading.Thread):
    """
    Thread que alterna os semáforos entre fase horizontal e fase vertical
    a cada ticks_fase ticks do relógio global.

    IDs 0-15  → semáforos do fluxo horizontal (LESTE/OESTE)
    IDs 16-31 → semáforos do fluxo vertical   (NORTE/SUL)

    A simulação começa com a fase H ativa (H=verde, V=vermelho).
    """

    def __init__(
        self,
        semaforos: Dict[int, "Semaforo"],
        relogio: "Relogio",
        ticks_fase: int = 8,
    ) -> None:
        super().__init__(daemon=True)
        self.relogio = relogio
        self.ticks_fase = ticks_fase
        self.em_execucao = False

        self._fase_h: List["Semaforo"] = [s for sid, s in semaforos.items() if sid < 16]
        self._fase_v: List["Semaforo"] = [s for sid, s in semaforos.items() if sid >= 16]

    def run(self) -> None:
        self.em_execucao = True
        contador = 0
        fase_h_ativa = True  # começa com H aberto, consistente com criar_semaforos
        while self.em_execucao:
            self.relogio.esperar_tick()
            contador += 1
            if contador >= self.ticks_fase:
                contador = 0
                fase_h_ativa = not fase_h_ativa
                if fase_h_ativa:
                    for s in self._fase_v:
                        s.fechar()
                    for s in self._fase_h:
                        s.abrir()
                else:
                    for s in self._fase_h:
                        s.fechar()
                    for s in self._fase_v:
                        s.abrir()
