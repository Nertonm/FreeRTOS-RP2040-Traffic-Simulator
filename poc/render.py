import os
import curses
from abc import ABC, abstractmethod
from typing import TYPE_CHECKING, Dict, List, Union

if TYPE_CHECKING:
    from grid import Grid
    from car import Car
    from ambulance import Ambulance
    from lock_manager import Semaforo, Relogio


# Tradução do símbolo interno da célula para o caractere exibido no terminal
_MAPA_VISUAL: Dict[str, str] = {
    "##": "  ",
    "||": "| ",
    "==": "--",
    ">>": ">>",
    "<<": "<<",
    "^^": "^^ ",
    "vv": "vv",
    "[]": "+ ",  # cruzamento sem semáforo
}


class Renderer(ABC):
    """
    Classe base abstrata para todos os renderers da simulação.

    Interface pública uniforme — trocar de renderer em main.py exige mudar
    apenas o nome da classe importada:

        renderer = PrintRenderer(grid, veiculos, semaforos, relogio)
        renderer.iniciar()          # setup opcional (abre janela, etc.)
        iniciar_simulacao(...)      # sobe threads de veículos e relógio
        renderer.loop()             # bloqueia a thread principal até encerrar
    """

    def __init__(
        self,
        grid: "Grid",
        veiculos: List[Union["Car", "Ambulance"]],
        semaforos: Dict[int, "Semaforo"],
        relogio: "Relogio",
    ) -> None:
        self.grid = grid
        self.veiculos = veiculos
        self.semaforos = semaforos
        self.relogio = relogio
        self.em_execucao: bool = False

    def iniciar(self) -> None:
        """Setup do renderer. No-op por padrão — subclasses sobrescrevem se necessário."""
        pass

    @abstractmethod
    def loop(self) -> None:
        """Bloqueia a thread principal executando o renderer até o encerramento."""
        ...

    @abstractmethod
    def desenhar_frame(self) -> None:
        """Desenha o estado instantâneo da simulação."""
        ...

    def _montar_frame(self) -> List[str]:
        """
        Constrói o frame atual como lista de strings a partir do estado do grid,
        veículos e semáforos.
        """
        from ambulance import Ambulance

        posicoes_veiculos: Dict[tuple, str] = {
            v.posicao_atual: "A " if isinstance(v, Ambulance) else "C "
            for v in self.veiculos
        }

        linhas: List[str] = []
        for y in range(self.grid.rows):
            linha = ""
            for x in range(self.grid.cols):
                celula = self.grid.obter_celula(y, x)
                if celula is None:
                    linha += "  "
                    continue

                if (y, x) in posicoes_veiculos:
                    linha += posicoes_veiculos[(y, x)]
                elif celula.is_cruzamento and celula.has_semaforo:
                    semaforo = self.semaforos.get(celula.light_id)
                    linha += ("G " if semaforo.estado_aberto else "R ") if semaforo else "+ "
                else:
                    linha += _MAPA_VISUAL.get(celula.visual, "  ")

            linhas.append(linha)

        return linhas


# ---------------------------------------------------------------------------

class PrintRenderer(Renderer):
    """
    Renderer de terminal simples: limpa a tela e imprime o frame a cada tick.
    Pode apresentar flickering, mas não exige dependências externas.
    """

    def loop(self) -> None:
        self.em_execucao = True
        while self.em_execucao:
            self.relogio.esperar_tick()
            self.desenhar_frame()

    def desenhar_frame(self) -> None:
        os.system("clear")
        print("\n".join(self._montar_frame()))


# ---------------------------------------------------------------------------

class CursesRenderer(Renderer):
    """
    Renderer de terminal sem flickering usando curses.
    Gerencia o ciclo de vida do curses internamente — main.py não precisa
    saber que curses existe.
    """

    def loop(self) -> None:
        curses.wrapper(self._loop_interno)

    def _loop_interno(self, stdscr) -> None:
        self._stdscr = stdscr
        self.em_execucao = True
        curses.curs_set(0)
        while self.em_execucao:
            self.relogio.esperar_tick()
            self.desenhar_frame()

    def desenhar_frame(self) -> None:
        self._stdscr.clear()
        for y, linha in enumerate(self._montar_frame()):
            try:
                self._stdscr.addstr(y, 0, linha)
            except curses.error:
                pass
        self._stdscr.refresh()

