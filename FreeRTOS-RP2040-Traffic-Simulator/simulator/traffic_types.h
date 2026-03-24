/* === Traffic Types === */

#ifndef TRAFFIC_TYPES_H
#define TRAFFIC_TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { DIR_NONE = 0, DIR_NORTH = 1, DIR_SOUTH = 2, DIR_EAST = 4, DIR_WEST = 8 } Direction;

typedef enum { REGION_CRATO = 0, REGION_JUAZEIRO = 1, REGION_BARBALHA = 2 } Region;

typedef enum {
  /* PORT: grid.py linha 22 */
  /* IDs 0-5: Crato, 6-11: Juazeiro, 12-15: Barbalha */
  LIGHT_CRATO_PEDRO_FELICIO_DOM_QUINTINO = 0,
  LIGHT_CRATO_PEDRO_FELICIO_JOSE_HORACIO = 1,
  LIGHT_CRATO_HERMES_PARAHYBA_DOM_QUINTINO = 2,
  LIGHT_CRATO_HERMES_PARAHYBA_JOSE_HORACIO = 3,
  LIGHT_CRATO_IRINEU_PINHEIRO_DOM_QUINTINO = 4,
  LIGHT_CRATO_IRINEU_PINHEIRO_JOSE_HORACIO = 5,

  LIGHT_JUAZ_CASTELO_BRANCO_SAO_PEDRO = 6,
  LIGHT_JUAZ_CASTELO_BRANCO_LEAO_SAMPAIO = 7,
  LIGHT_JUAZ_PADRE_CICERO_SAO_PEDRO = 8,
  LIGHT_JUAZ_PADRE_CICERO_LEAO_SAMPAIO = 9,
  LIGHT_JUAZ_VIRGILIO_TAVORA_SAO_PEDRO = 10,
  LIGHT_JUAZ_VIRGILIO_TAVORA_LEAO_SAMPAIO = 11,

  LIGHT_BARB_SANTOS_DUMONT_CE060 = 12,
  LIGHT_BARB_SANTOS_DUMONT_DOM_QUINTINO = 13,
  LIGHT_BARB_LUIZ_TEIXEIRA_CE060 = 14,
  LIGHT_BARB_LUIZ_TEIXEIRA_DOM_QUINTINO = 15,

  LIGHT_COUNT = 16
} LightId;

typedef enum {
  CELL_WALL = 0,     /* "##" */
  CELL_ROAD_NS = 1,  /* "||" */
  CELL_ROAD_EW = 2,  /* "==" */
  CELL_ONE_EAST = 3, /* ">>" */
  CELL_ONE_WEST = 4, /* "<<" */
  CELL_BORDER_N = 5, /* "^^" */
  CELL_BORDER_S = 6, /* "vv" */
  CELL_CROSS = 7     /* "[]" */
} CellType;

typedef struct {
  CellType type;
  int8_t light_id; /* -1 = sem semaforo */
} Cell;

typedef struct {
  uint8_t region; /* 0,1,2 */
  uint8_t row;
  uint8_t col;
  Direction dir;
} SpawnPoint;

#define GRID_ROWS 5
#define GRID_COLS 5
#define LED_ROWS 5
#define LED_COLS 5
#define MAX_LIGHTS_PER_REGION 6
#define MAX_CARS 7       /* por placa */
#define AMBULANCE_ID 255 /* valor especial em car_grid */

/* DESIGN: compatibilidade com codigo legado, mantendo nomenclatura anterior. */
typedef Direction Direcao;
typedef Region Regiao;
typedef CellType TipoCelula;
typedef Cell Celula;
typedef LightId IdSemaforo;

#define DIRECAO_NORTE DIR_NORTH
#define DIRECAO_SUL DIR_SOUTH
#define DIRECAO_LESTE DIR_EAST
#define DIRECAO_OESTE DIR_WEST
#define DIRECAO_NONE DIR_NONE

#define REGIAO_CRATO REGION_CRATO
#define REGIAO_JUAZEIRO REGION_JUAZEIRO
#define REGIAO_BARBALHA REGION_BARBALHA

#define WALL CELL_WALL
#define VIA_NS CELL_ROAD_NS
#define VIA_EW CELL_ROAD_EW
#define MAO_LESTE CELL_ONE_EAST
#define MAO_OESTE CELL_ONE_WEST
#define BORDA_NORTE CELL_BORDER_N
#define BORDA_SUL CELL_BORDER_S
#define CRUZAMENTO CELL_CROSS

#define MAX_LIGHTS MAX_LIGHTS_PER_REGION
#define MAX_CARS_PER_REGION MAX_CARS

typedef struct {
  uint8_t id;
  uint8_t y;
  uint8_t x;
  Direcao dir;
  uint8_t velocidade;
  uint8_t tick_count;
  bool active;
} CarState;

typedef struct {
  CarState base;
  uint8_t radar_range;
  bool active;
} AmbulanceState;

#endif
