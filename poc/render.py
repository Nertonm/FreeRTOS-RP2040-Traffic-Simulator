import os
import curses
from abc import ABC, abstractmethod
from typing import TYPE_CHECKING, Dict, List, Union

from lock_manager import TICK_DURATION

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
    "^^": "^^",
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

        # Evita race condition: leia `posicao_atual` apenas uma vez por veículo
        posicoes_veiculos: Dict[tuple, str] = {}
        for v in self.veiculos:
            if v.em_execucao:
                pos = v.posicao_atual
                posicoes_veiculos[pos] = "A " if isinstance(v, Ambulance) else "C "

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
                    # Mostrar verde se qualquer fase (H ou V) estiver aberta
                    sem_h = self.semaforos.get(celula.light_id)
                    sem_v = self.semaforos.get(celula.light_id + 16)
                    aberto = (sem_h and sem_h.estado_aberto) or (sem_v and sem_v.estado_aberto)
                    linha += ("G " if aberto else "R ") if (sem_h or sem_v) else "+ "
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


def _par_via(visual: str) -> int:
    """Mapeia o símbolo interno de uma célula ao color pair curses correspondente."""
    if visual in (">>", "<<", "^^", "vv"):
        return 3  # branco — mão única
    if visual in ("||", "=="):
        return 4  # amarelo — mão dupla
    if visual == "[]":
        return 5  # cruzamento sem semáforo
    return 0      # default (paredes / célula vazia)


# ---------------------------------------------------------------------------

class CursesRenderer(Renderer):
    """
    Renderer de terminal sem flickering usando curses.
    Gerencia o ciclo de vida do curses internamente — main.py não precisa
    saber que curses existe.

    Pairs de cor:
        1  — semáforo aberto (verde)
        2  — semáforo fechado (vermelho)
        3  — via mão única (branco)
        4  — via mão dupla (amarelo)
        5  — cruzamento sem semáforo (branco)
        6  — ambulância (preto sobre vermelho)
        7–13 — veículos, ciclo de 7 cores de fundo
    """

    _PALETA_VEICULOS = [
        curses.COLOR_CYAN,
        curses.COLOR_BLUE,
        curses.COLOR_MAGENTA,
        curses.COLOR_GREEN,
        curses.COLOR_YELLOW,
        curses.COLOR_WHITE,
        curses.COLOR_RED,
    ]

    def loop(self) -> None:
        curses.wrapper(self._loop_interno)

    def _loop_interno(self, stdscr) -> None:
        self._stdscr = stdscr
        self.em_execucao = True
        curses.curs_set(0)
        self._inicializar_cores()
        while self.em_execucao:
            self.relogio.esperar_tick()
            self.desenhar_frame()

    def _inicializar_cores(self) -> None:
        """Registra os color pairs e constrói o mapeamento id_carro → pair."""
        curses.start_color()
        curses.use_default_colors()

        curses.init_pair(1, curses.COLOR_GREEN,  curses.COLOR_BLACK)  # semáforo aberto
        curses.init_pair(2, curses.COLOR_RED,    curses.COLOR_BLACK)  # semáforo fechado
        curses.init_pair(3, curses.COLOR_WHITE,  curses.COLOR_BLACK)  # mão única
        curses.init_pair(4, curses.COLOR_YELLOW, curses.COLOR_BLACK)  # mão dupla
        curses.init_pair(5, curses.COLOR_WHITE,  curses.COLOR_BLACK)  # cruzamento s/ semáforo
        curses.init_pair(6, curses.COLOR_BLACK,  curses.COLOR_RED)    # ambulância

        for i, cor in enumerate(self._PALETA_VEICULOS):
            curses.init_pair(7 + i, curses.COLOR_BLACK, cor)

        curses.init_pair(14, curses.COLOR_CYAN, curses.COLOR_BLACK)  # cabeçalho do painel

        from ambulance import Ambulance
        self._cor_veiculo: Dict[int, int] = {
            v.id_carro: (6 if isinstance(v, Ambulance) else 7 + (v.id_carro % 7))
            for v in self.veiculos
        }

    def desenhar_frame(self) -> None:
        from ambulance import Ambulance
        self._stdscr.erase()

        posicoes: Dict[tuple, int] = {
            v.posicao_atual: v.id_carro for v in self.veiculos
            if v.em_execucao
        }

        for y in range(self.grid.rows):
            for x in range(self.grid.cols):
                celula = self.grid.obter_celula(y, x)
                if celula is None:
                    continue

                if (y, x) in posicoes:
                    id_c = posicoes[(y, x)]
                    pair = self._cor_veiculo[id_c]
                    texto = "A " if pair == 6 else "C "
                    attr = curses.color_pair(pair) | curses.A_BOLD
                elif celula.is_cruzamento and celula.has_semaforo:
                    # Mostra H (fase horizontal) ou V (fase vertical) quando possível;
                    # caso contrário, mostra R se ambos fechados.
                    sem_h = self.semaforos.get(celula.light_id)
                    sem_v = self.semaforos.get(celula.light_id + 16)
                    if sem_h and sem_h.estado_aberto:
                        texto = "H "
                        attr = curses.color_pair(1) | curses.A_BOLD
                    elif sem_v and sem_v.estado_aberto:
                        texto = "V "
                        attr = curses.color_pair(1) | curses.A_BOLD
                    else:
                        texto = "R "
                        attr = curses.color_pair(2) | curses.A_BOLD
                else:
                    texto = _MAPA_VISUAL.get(celula.visual, "  ")
                    attr = curses.color_pair(_par_via(celula.visual))

                try:
                    self._stdscr.addstr(y, x * 2, texto, attr)
                except curses.error:
                    pass

        self._desenhar_painel()
        self._stdscr.refresh()

    def _desenhar_painel(self) -> None:
        """Escreve o painel de informações à direita do mapa."""
        from ambulance import Ambulance

        col = self.grid.cols * 2 + 3
        tick = self.relogio._tick_numero
        tempo = tick * TICK_DURATION

        ativos = sum(1 for v in self.veiculos if v.em_execucao)
        total = len(self.veiculos)
        amb = next((v for v in self.veiculos if isinstance(v, Ambulance)), None)
        semaforos_abertos = sum(1 for s in self.semaforos.values() if s.estado_aberto)

        BOLD  = curses.A_BOLD
        NORM  = curses.A_NORMAL
        CYAN  = curses.color_pair(14) | BOLD
        VERDE = curses.color_pair(1)  | BOLD
        VERM  = curses.color_pair(2)  | BOLD

        # (texto, attr) ou ("__semaforos__", [ids])
        linhas: List[tuple] = [
            ("SIMULADOR DE TRÁFEGO", CYAN),
            ("─" * 22, NORM),
            (f"Tick  : {tick}", NORM),
            (f"Tempo : {tempo:.1f}s", NORM),
            ("", NORM),
            ("VEÍCULOS", BOLD),
            (f"  Ativos : {ativos} / {total}", NORM),
        ]
        if amb:
            linhas.append((f"  Amb    : {amb.posicao_atual}", NORM))
        linhas += [
            ("", NORM),
            (f"SEMÁFOROS  [{semaforos_abertos}/{len(self.semaforos)} abertos]", BOLD),
        ]

        ids_ordenados = sorted(self.semaforos)
        for i in range(0, len(ids_ordenados), 4):
            linhas.append(("__semaforos__", ids_ordenados[i:i + 4]))

        linhas += [
            ("", NORM),
            ("LEGENDA", BOLD),
            ("  C   Carro", NORM),
            ("  A   Ambulância", NORM),
            ("  G   Sinal verde", VERDE),
            ("  R   Sinal vermelho", VERM),
            ("  --  Mão dupla (amarelo)", curses.color_pair(4)),
            ("  >>  Mão única (branco)", curses.color_pair(3)),
        ]

        for row, (texto, attr_ou_grupo) in enumerate(linhas):
            if texto == "__semaforos__":
                x_offset = col
                for sid in attr_ou_grupo:
                    s = self.semaforos[sid]
                    attr = VERDE if s.estado_aberto else VERM
                    trecho = f"{sid:2d}:{'G' if s.estado_aberto else 'R'}  "
                    try:
                        self._stdscr.addstr(row, x_offset, trecho, attr)
                    except curses.error:
                        pass
                    x_offset += len(trecho)
            else:
                try:
                    self._stdscr.addstr(row, col, texto, attr_ou_grupo)
                except curses.error:
                    pass

