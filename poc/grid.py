from typing import Optional, List, Dict, Tuple, NamedTuple
from dataclasses import dataclass
from enum import IntFlag, IntEnum

# ENUMS (Tipos Alto Nível)

class Direcao(IntFlag):
    NENHUMA = 0
    NORTE   = 1
    SUL     = 2
    LESTE   = 4
    OESTE   = 8

class Regiao(IntEnum):
    CRATO         = 0
    JUAZEIRO      = 1
    BARBALHA      = 2

# Dimensoes globais
ROWS_POR_PLACA: int = 8
COLS: int = 8
TOTAL_PLACAS: int = 3
TOTAL_ROWS: int = ROWS_POR_PLACA * TOTAL_PLACAS

class IdSemaforo(IntEnum):
    """Mapeamento semântico do Semáforo com os Nomes Físicos das Ruas (IDs 0 a 15)"""
    
    # === Crato (Av. Horizontal x R. Vertical) ===
    CRATO_AV_PEDRO_FELICIO_X_RUA_DOM_QUINTINO       = 0  
    CRATO_AV_PEDRO_FELICIO_X_AV_JOSE_HORACIO        = 1
    CRATO_AV_HERMES_PARAHYBA_X_RUA_DOM_QUINTINO     = 2
    CRATO_AV_HERMES_PARAHYBA_X_AV_JOSE_HORACIO      = 3
    CRATO_AV_IRINEU_PINHEIRO_X_RUA_DOM_QUINTINO     = 4
    CRATO_AV_IRINEU_PINHEIRO_X_AV_JOSE_HORACIO      = 5
    
    # === Juazeiro do Norte ===
    JUAZ_AV_CASTELO_BRANCO_X_RUA_SAO_PEDRO          = 6
    JUAZ_AV_CASTELO_BRANCO_X_AV_LEAO_SAMPAIO        = 7
    JUAZ_RUA_PADRE_CICERO_X_RUA_SAO_PEDRO           = 8
    JUAZ_RUA_PADRE_CICERO_X_AV_LEAO_SAMPAIO         = 9
    JUAZ_AV_VIRGILIO_TAVORA_X_RUA_SAO_PEDRO         = 10
    JUAZ_AV_VIRGILIO_TAVORA_X_AV_LEAO_SAMPAIO       = 11
    
    # === Barbalha ===
    BARB_RUA_SANTOS_DUMONT_X_RODOVIA_CE060          = 12
    BARB_RUA_SANTOS_DUMONT_X_RUA_DOM_QUINTINO       = 13
    BARB_RUA_LUIZ_TEIXEIRA_X_RODOVIA_CE060          = 14
    BARB_RUA_LUIZ_TEIXEIRA_X_RUA_DOM_QUINTINO       = 15



# CONFIGURAÇÕES DA MALHA

# Tradutor Visual -> Lógica de Direções
SIMBOLOS_MAPA = {
    "##": Direcao.NENHUMA,                                             # Parede / Área vazia
    "||": Direcao.NORTE | Direcao.SUL,                                 # Via Dupla Norte-Sul
    "==": Direcao.LESTE | Direcao.OESTE,                               # Via Dupla Leste-Oeste
    ">>": Direcao.LESTE,                                               # Mão Única Leste
    "<<": Direcao.OESTE,                                               # Mão Única Oeste
    "^^": Direcao.NORTE,                                               # Borda: Vindo do Norte
    "vv": Direcao.SUL,                                                 # Borda: Indo pro Sul
    "[]": Direcao.NORTE | Direcao.SUL | Direcao.LESTE | Direcao.OESTE  # Cruzamento
}

# 
# MAPA: CRATO
# 
MAPA_CRATO = [
    ["##", "##", "||", "##", "##", "||", "##", "##"],  # R0
    ["##", "##", "||", "##", "##", "||", "##", "##"],  # R1
    [">>", ">>", "[]", ">>", ">>", "[]", ">>", "##"],  # R2: Av. Pedro Felício →
    ["##", "##", "||", "##", "##", "||", "##", "##"],  # R3
    ["##", "<<", "[]", "<<", "<<", "[]", "<<", "##"],  # R4: Av. Hermes Parahyba ←
    ["##", "##", "||", "##", "##", "||", "##", "##"],  # R5
    ["##", "==", "[]", "==", "==", "[]", "==", "##"],  # R6: Av. Dr. Irineu Pinheiro ↔
    ["##", "##", "||", "##", "##", "||", "##", "##"],  # R7 (junção com Juazeiro)
]

SEMAFOROS_CRATO: Dict[Tuple[int, int], IdSemaforo] = {
    (2, 2): IdSemaforo.CRATO_AV_PEDRO_FELICIO_X_RUA_DOM_QUINTINO,
    (2, 5): IdSemaforo.CRATO_AV_PEDRO_FELICIO_X_AV_JOSE_HORACIO,
    
    (4, 2): IdSemaforo.CRATO_AV_HERMES_PARAHYBA_X_RUA_DOM_QUINTINO,
    (4, 5): IdSemaforo.CRATO_AV_HERMES_PARAHYBA_X_AV_JOSE_HORACIO,
    
    (6, 2): IdSemaforo.CRATO_AV_IRINEU_PINHEIRO_X_RUA_DOM_QUINTINO,
    (6, 5): IdSemaforo.CRATO_AV_IRINEU_PINHEIRO_X_AV_JOSE_HORACIO,
}

# MAPA: JUAZEIRO DO NORTE
MAPA_JUAZEIRO = [
    ["##", "##", "||", "##", "##", "||", "##", "##"],  # R0 (junção com Crato)
    ["##", "##", "||", "##", "##", "||", "##", "##"],  # R1
    [">>", ">>", "[]", ">>", ">>", "[]", ">>", "##"],  # R2: Av. Castelo Branco ->
    ["##", "##", "||", "##", "##", "||", "##", "##"],  # R3
    ["##", "<<", "[]", "<<", "<<", "[]", "<<", "##"],  # R4: Rua Padre Cícero <-
    ["##", "##", "||", "##", "##", "||", "##", "##"],  # R5
    ["##", "==", "[]", "==", "==", "[]", "==", "##"],  # R6: Av. Virgílio Távora <->
    ["##", "##", "||", "##", "##", "||", "##", "##"],  # R7 (junção com Barbalha)
]

SEMAFOROS_JUAZEIRO: Dict[Tuple[int, int], IdSemaforo] = {
    (2, 2): IdSemaforo.JUAZ_AV_CASTELO_BRANCO_X_RUA_SAO_PEDRO,
    (2, 5): IdSemaforo.JUAZ_AV_CASTELO_BRANCO_X_AV_LEAO_SAMPAIO,

    (4, 2): IdSemaforo.JUAZ_RUA_PADRE_CICERO_X_RUA_SAO_PEDRO,
    (4, 5): IdSemaforo.JUAZ_RUA_PADRE_CICERO_X_AV_LEAO_SAMPAIO,

    (6, 2): IdSemaforo.JUAZ_AV_VIRGILIO_TAVORA_X_RUA_SAO_PEDRO,
    (6, 5): IdSemaforo.JUAZ_AV_VIRGILIO_TAVORA_X_AV_LEAO_SAMPAIO,
}

# MAPA: BARBALHA
MAPA_BARBALHA = [
    ["##", "##", "||", "##", "##", "||", "##", "##"],  # R0 (junção com Juazeiro)
    ["##", "##", "||", "##", "##", "||", "##", "##"],  # R1
    ["##", ">>", "[]", ">>", ">>", "[]", ">>", "##"],  # R2: Rua Santos Dumont ->
    ["##", "##", "||", "##", "##", "||", "##", "##"],  # R3
    ["##", "<<", "[]", "<<", "<<", "[]", "<<", "##"],  # R4: Rua Cel. Luiz Teixeira <-
    ["##", "##", "||", "##", "##", "||", "##", "##"],  # R5
    ["##", "##", "##", "##", "##", "##", "##", "##"],  # R6 (Borda do mundo)
    ["##", "##", "##", "##", "##", "##", "##", "##"],  # R7
]

SEMAFOROS_BARBALHA: Dict[Tuple[int, int], IdSemaforo] = {
    (2, 2): IdSemaforo.BARB_RUA_SANTOS_DUMONT_X_RODOVIA_CE060,
    (2, 5): IdSemaforo.BARB_RUA_SANTOS_DUMONT_X_RUA_DOM_QUINTINO,
    
    (4, 2): IdSemaforo.BARB_RUA_LUIZ_TEIXEIRA_X_RODOVIA_CE060,
    (4, 5): IdSemaforo.BARB_RUA_LUIZ_TEIXEIRA_X_RUA_DOM_QUINTINO,
}

ESTRUTURAS_REGIONAIS = [
    (MAPA_CRATO,    SEMAFOROS_CRATO),
    (MAPA_JUAZEIRO, SEMAFOROS_JUAZEIRO),
    (MAPA_BARBALHA, SEMAFOROS_BARBALHA)
]
 
# NASCEDOUROS DE VEÍCULOS (Spawns) 
class Spawn(NamedTuple):
    regiao: Regiao
    linha: int
    coluna: int
    direcao: Direcao

SPAWN_POINTS: List[Spawn] = [
    # Entradas pelo Norte descendo ao Sul
    Spawn(Regiao.CRATO, 0, 2, Direcao.SUL),
    Spawn(Regiao.CRATO, 0, 5, Direcao.SUL),
    
    # Entradas pela lateral Oeste cruzando para o Leste
    Spawn(Regiao.CRATO,    2, 0, Direcao.LESTE),
    Spawn(Regiao.JUAZEIRO, 2, 0, Direcao.LESTE),
    Spawn(Regiao.BARBALHA, 2, 0, Direcao.LESTE),

    # Entradas pela lateral Leste cruzando para o Oeste
    Spawn(Regiao.CRATO,    4, 7, Direcao.OESTE),
    Spawn(Regiao.JUAZEIRO, 4, 7, Direcao.OESTE),
    Spawn(Regiao.BARBALHA, 4, 7, Direcao.OESTE),
]


# LÓGICA DO GRID
@dataclass
class Celula:
    visual: str
    light_id: int = -1

    @property
    def direcoes_permitidas(self) -> Direcao:
        return SIMBOLOS_MAPA.get(self.visual, Direcao.NENHUMA)

    @property
    def is_parede(self) -> bool:
        return self.direcoes_permitidas == Direcao.NENHUMA

    @property
    def is_cruzamento(self) -> bool:
        return self.visual == "[]"

    @property
    def has_semaforo(self) -> bool:
        return self.light_id >= 0

    def aceita_direcao(self, dir_desejada: Direcao) -> bool:
        """Checa se o carro pode entrar nesta célula apontando para esta direção."""
        return bool(self.direcoes_permitidas & dir_desejada)


class Grid:
    def __init__(self, linhas_por_regiao: int = 8, colunas: int = 8):
        self.rows: int = linhas_por_regiao * len(ESTRUTURAS_REGIONAIS)  # 24
        self.cols: int = colunas  # 8
        self.linhas_por_regiao: int = linhas_por_regiao
        
        self.matriz: List[List[Celula]] = []
        self.coordenadas_cruzamentos: List[Tuple[int, int]] = []
        self.semaforos_por_coordenada: Dict[Tuple[int, int], int] = {}

    def carregar_mapa_padrao(self) -> None:
        """Costura o mapa das 3 regiões em uma única matriz contínua (24x8)."""
        self.matriz = []

        for indice_regiao, (mapa_asci, semaforos_ids) in enumerate(ESTRUTURAS_REGIONAIS):
            for local_y, linha_ascii in enumerate(mapa_asci):
                global_y = (indice_regiao * self.linhas_por_regiao) + local_y
                linha_celulas: List[Celula] = []
                
                for x, simbolo in enumerate(linha_ascii):
                    # Converte o Enum devolta pro seu Integer primitivo caso seja um semaforo
                    enum_semaforo = semaforos_ids.get((local_y, x))
                    id_sem = int(enum_semaforo) if enum_semaforo is not None else -1
                    
                    celula = Celula(visual=simbolo, light_id=id_sem)
                    linha_celulas.append(celula)

                    if celula.is_cruzamento:
                        posicao = (global_y, x)
                        self.coordenadas_cruzamentos.append(posicao)
                        self.semaforos_por_coordenada[posicao] = id_sem
                
                self.matriz.append(linha_celulas)

    def obter_celula(self, y: int, x: int) -> Optional[Celula]:
        """Retorna o bloco físico em y (linha), x (coluna)."""
        if 0 <= y < self.rows and 0 <= x < self.cols:
            if self.matriz:
                return self.matriz[y][x]
        return None

    def obter_id_semaforo(self, y: int, x: int) -> int:
        celula = self.obter_celula(y, x)
        return celula.light_id if celula else -1

    def transpor_placa(self, carro: object) -> None:
        """Reposiciona o veículo para a primeira linha da malha na mesma coluna, se possível."""
        posicao = getattr(carro, "posicao_atual", None)
        direcao = getattr(carro, "direcao_atual", None)
        if not posicao or direcao is None:
            return

        y_atual, x_atual = posicao

        # Só transpõe ao sair do limite inferior (fim de Barbalha) andando para SUL
        if y_atual < self.rows - 1 or direcao != Direcao.SUL:
            return

        destino = self.obter_celula(0, x_atual)
        if destino and not destino.is_parede and destino.aceita_direcao(direcao):
            carro.posicao_atual = (0, x_atual)
