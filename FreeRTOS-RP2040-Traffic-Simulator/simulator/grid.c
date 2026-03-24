/* === Grid local por regiao === */

#include "simulator/grid.h"

#include "debug_log.h"

#include <stdio.h>

SemaphoreHandle_t cell_mutex[GRID_ROWS][GRID_COLS];

static Cell g_grid[GRID_ROWS][GRID_COLS];
static Region g_region = REGION_CRATO;

/*
 * Legenda do mapa 5x5:
 *   W  = parede
 *   NS = via norte/sul
 *   EW = via leste/oeste
 *   E  = mao unica leste
 *   O  = mao unica oeste
 *   BN = borda norte (entrada/saida)
 *   BS = borda sul (entrada/saida)
 *   X(id) = cruzamento com semaforo id
 *
 * Coordenadas:
 *   y: linhas 0..4 (cima para baixo)
 *   x: colunas 0..4 (esquerda para direita)
 */
#define W {CELL_WALL, -1}
#define NS {CELL_ROAD_NS, -1}
#define EW {CELL_ROAD_EW, -1}
#define E {CELL_ONE_EAST, -1}
#define O {CELL_ONE_WEST, -1}
#define BN {CELL_BORDER_N, -1}
#define BS {CELL_BORDER_S, -1}
#define X(id) {CELL_CROSS, (id)}

/* PORT: grid.py linha 77 */
static const Cell MAPA_CRATO[GRID_ROWS][GRID_COLS] = {
    /* y=0 */ {W, NS, W, NS, W},
    /* y=1 */
    {E, X(LIGHT_CRATO_PEDRO_FELICIO_DOM_QUINTINO), E, X(LIGHT_CRATO_PEDRO_FELICIO_JOSE_HORACIO), E},
    /* y=2 */
    {O, X(LIGHT_CRATO_HERMES_PARAHYBA_DOM_QUINTINO), O, X(LIGHT_CRATO_HERMES_PARAHYBA_JOSE_HORACIO),
     O},
    /* y=3 */
    {EW, X(LIGHT_CRATO_IRINEU_PINHEIRO_DOM_QUINTINO), EW,
     X(LIGHT_CRATO_IRINEU_PINHEIRO_JOSE_HORACIO), EW},
    /* y=4 */ {W, BS, W, BS, W},
};

/* PORT: grid.py linha 99 */
static const Cell MAPA_JUAZEIRO[GRID_ROWS][GRID_COLS] = {
    /* y=0 */ {W, BN, W, BN, W},
    /* y=1 */
    {E, X(LIGHT_JUAZ_CASTELO_BRANCO_SAO_PEDRO), E, X(LIGHT_JUAZ_CASTELO_BRANCO_LEAO_SAMPAIO), E},
    /* y=2 */
    {O, X(LIGHT_JUAZ_PADRE_CICERO_SAO_PEDRO), O, X(LIGHT_JUAZ_PADRE_CICERO_LEAO_SAMPAIO), O},
    /* y=3 */
    {EW, X(LIGHT_JUAZ_VIRGILIO_TAVORA_SAO_PEDRO), EW, X(LIGHT_JUAZ_VIRGILIO_TAVORA_LEAO_SAMPAIO),
     EW},
    /* y=4 */ {W, BS, W, BS, W},
};

/* PORT: grid.py linha 121 */
static const Cell MAPA_BARBALHA[GRID_ROWS][GRID_COLS] = {
    /* y=0 */ {W, BN, W, BN, W},
    /* y=1 */
    {E, X(LIGHT_BARB_SANTOS_DUMONT_CE060), E, X(LIGHT_BARB_SANTOS_DUMONT_DOM_QUINTINO), E},
    /* y=2 */
    {O, X(LIGHT_BARB_LUIZ_TEIXEIRA_CE060), O, X(LIGHT_BARB_LUIZ_TEIXEIRA_DOM_QUINTINO), O},
    /* y=3 */ {W, NS, W, NS, W},
    /* y=4 */ {W, BS, W, BS, W},
};

#undef W
#undef NS
#undef EW
#undef E
#undef O
#undef BN
#undef BS
#undef X

/* DESIGN: tabela fixa LED->semaforo por regiao, conforme mapeamento validado. */
static const int8_t LED_LIGHT_MAP[3][LED_ROWS][LED_COLS] = {
    /* NODE_ID 0: Crato */
    {
        {-1, -1, -1, -1, -1},
        {-1, 0, -1, 1, -1},
        {-1, 2, -1, 3, -1},
        {-1, 4, -1, 5, -1},
        {-1, -1, -1, -1, -1},
    },
    /* NODE_ID 1: Juazeiro */
    {
        {-1, -1, -1, -1, -1},
        {-1, 6, -1, 7, -1},
        {-1, 8, -1, 9, -1},
        {-1, 10, -1, 11, -1},
        {-1, -1, -1, -1, -1},
    },
    /* NODE_ID 2: Barbalha */
    {
        {-1, -1, -1, -1, -1},
        {-1, 12, -1, 13, -1},
        {-1, 14, -1, 15, -1},
        {-1, -1, -1, -1, -1},
        {-1, -1, -1, -1, -1},
    },
};

/* PORT: grid.py linha 156 */
static const SpawnPoint SPAWNS_CRATO[] = {
    {REGION_CRATO, 0, 1, DIR_SOUTH},
    {REGION_CRATO, 0, 3, DIR_SOUTH},
    {REGION_CRATO, 2, 0, DIR_EAST},
    {REGION_CRATO, 2, 4, DIR_WEST},
};

static const SpawnPoint SPAWNS_JUAZEIRO[] = {
    {REGION_JUAZEIRO, 2, 0, DIR_EAST},
    {REGION_JUAZEIRO, 2, 4, DIR_WEST},
};

static const SpawnPoint SPAWNS_BARBALHA[] = {
    {REGION_BARBALHA, 2, 0, DIR_EAST},
    {REGION_BARBALHA, 2, 4, DIR_WEST},
};

static const Cell CELL_INVALID = {CELL_WALL, -1};

/* DESIGN: lookup de direcoes evita switch repetido e reduz custo no loop. */
static const Direction CELL_DIR_LUT[] = {
    [CELL_WALL] = DIR_NONE,
    [CELL_ROAD_NS] = (Direction)(DIR_NORTH | DIR_SOUTH),
    [CELL_ROAD_EW] = (Direction)(DIR_EAST | DIR_WEST),
    [CELL_ONE_EAST] = DIR_EAST,
    [CELL_ONE_WEST] = DIR_WEST,
    [CELL_BORDER_N] = DIR_NORTH,
    [CELL_BORDER_S] = DIR_SOUTH,
    [CELL_CROSS] = (Direction)(DIR_NORTH | DIR_SOUTH | DIR_EAST | DIR_WEST),
};

static void grid_copy_region_map(Region region) {
  uint8_t y;
  uint8_t x;
  const Cell(*src)[GRID_COLS] = MAPA_CRATO;

  if (region == REGION_JUAZEIRO) {
    src = MAPA_JUAZEIRO;
  } else if (region == REGION_BARBALHA) {
    src = MAPA_BARBALHA;
  }

  for (y = 0; y < GRID_ROWS; y++) {
    for (x = 0; x < GRID_COLS; x++) {
      g_grid[y][x] = src[y][x];
    }
  }
}

void grid_init(uint8_t node_id) {
  uint8_t y;
  uint8_t x;

  configASSERT(node_id <= (uint8_t)REGION_BARBALHA);
  g_region = (Region)node_id;

  grid_copy_region_map(g_region);

  for (y = 0; y < GRID_ROWS; y++) {
    for (x = 0; x < GRID_COLS; x++) {
      cell_mutex[y][x] = xSemaphoreCreateMutex();
      configASSERT(cell_mutex[y][x] != NULL);
    }
  }

  LOG_NORMAL("[GRID]", "grid_init node=%u rows=%u cols=%u", (unsigned)node_id, (unsigned)GRID_ROWS,
             (unsigned)GRID_COLS);
}

bool grid_acquire_cell(uint8_t y, uint8_t x) {
  if (!grid_in_bounds(y, x)) {
    return false;
  }
  return xSemaphoreTake(cell_mutex[y][x], 0) == pdTRUE;
}

void grid_release_cell(uint8_t y, uint8_t x) {
  if (!grid_in_bounds(y, x)) {
    return;
  }
  (void)xSemaphoreGive(cell_mutex[y][x]);
}

const Cell grid_get_cell(uint8_t y, uint8_t x) {
  if (!grid_in_bounds(y, x)) {
    return CELL_INVALID;
  }
  return g_grid[y][x];
}

int8_t grid_get_light_id(uint8_t y, uint8_t x) {
  if (!grid_in_bounds(y, x)) {
    return -1;
  }
  return g_grid[y][x].light_id;
}

Direction grid_cell_directions(uint8_t y, uint8_t x) {
  if (!grid_in_bounds(y, x)) {
    return DIR_NONE;
  }
  return CELL_DIR_LUT[g_grid[y][x].type];
}

bool grid_can_enter(uint8_t y, uint8_t x, Direction dir) {
  Direction allowed;
  if (!grid_in_bounds(y, x)) {
    return false;
  }
  if (g_grid[y][x].type == CELL_WALL) {
    return false;
  }
  allowed = grid_cell_directions(y, x);
  return (allowed & dir) != 0;
}

const SpawnPoint *grid_get_spawns(uint8_t *out_count) {
  if (g_region == REGION_CRATO) {
    if (out_count != NULL) {
      *out_count = (uint8_t)(sizeof(SPAWNS_CRATO) / sizeof(SPAWNS_CRATO[0]));
    }
    return SPAWNS_CRATO;
  }
  if (g_region == REGION_JUAZEIRO) {
    if (out_count != NULL) {
      *out_count = (uint8_t)(sizeof(SPAWNS_JUAZEIRO) / sizeof(SPAWNS_JUAZEIRO[0]));
    }
    return SPAWNS_JUAZEIRO;
  }
  if (out_count != NULL) {
    *out_count = (uint8_t)(sizeof(SPAWNS_BARBALHA) / sizeof(SPAWNS_BARBALHA[0]));
  }
  return SPAWNS_BARBALHA;
}

int8_t grid_led_light_id(uint8_t led_x, uint8_t led_y) {
  if (led_x >= LED_COLS || led_y >= LED_ROWS) {
    return -1;
  }
  return LED_LIGHT_MAP[g_region][led_y][led_x];
}

bool grid_in_bounds(uint8_t y, uint8_t x) {
  return y < GRID_ROWS && x < GRID_COLS;
}

void grid_dump(void) {
  uint8_t y;
  uint8_t x;
  for (y = 0; y < GRID_ROWS; y++) {
    char line[80];
    int p = 0;
    for (x = 0; x < GRID_COLS; x++) {
      const Cell c = g_grid[y][x];
      const char *sym = "##";
      if (c.type == CELL_ROAD_NS) {
        sym = "||";
      } else if (c.type == CELL_ROAD_EW) {
        sym = "==";
      } else if (c.type == CELL_ONE_EAST) {
        sym = ">>";
      } else if (c.type == CELL_ONE_WEST) {
        sym = "<<";
      } else if (c.type == CELL_BORDER_N) {
        sym = "^^";
      } else if (c.type == CELL_BORDER_S) {
        sym = "vv";
      } else if (c.type == CELL_CROSS) {
        sym = "[]";
      }
      p += snprintf(&line[p], (size_t)(sizeof(line) - (size_t)p), "%s ", sym);
    }
    LOG_VERBOSE("[GRID]", "y=%u %s", (unsigned)y, line);
  }
}
