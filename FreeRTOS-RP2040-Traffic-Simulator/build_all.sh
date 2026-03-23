#!/usr/bin/env bash
# build_all.sh — Compila os 3 binários do Traffic Simulator
#
# Uso:
#   WIFI_SSID="MinhaRede" WIFI_PASSWORD="senha123" \
#   FALLBACK_SERVER_IP="192.168.0.10" ./build_all.sh
#
# Saída em dist/:
#   norte.uf2  → NODE_ID=0, Crato        (flash no Pico do norte)
#   centro.uf2 → NODE_ID=1, Juazeiro     (servidor TCP, flash no Pico central)
#   sul.uf2    → NODE_ID=2, Barbalha     (flash no Pico do sul)
#
# Pré-requisitos:
#   - PICO_SDK_PATH apontando para o diretório do Pico SDK instalado
#   - cmake >= 3.13
#   - arm-none-eabi-gcc

set -euo pipefail

# ---------------------------------------------------------------------------
# Validação de variáveis obrigatórias
# ---------------------------------------------------------------------------
missing=""
for var in WIFI_SSID WIFI_PASSWORD FALLBACK_SERVER_IP; do
    if [ -z "${!var:-}" ]; then
        missing="${missing} ${var}"
    fi
done
if [ -n "$missing" ]; then
    echo "[build_all] ERRO: Variáveis de ambiente ausentes:${missing}" >&2
    echo "[build_all] Uso: WIFI_SSID=... WIFI_PASSWORD=... FALLBACK_SERVER_IP=... $0" >&2
    exit 1
fi

if [ -z "${PICO_SDK_PATH:-}" ]; then
    echo "[build_all] ERRO: PICO_SDK_PATH não definido." >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Diretórios de trabalho (relativos ao script)
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
DIST_DIR="${SCRIPT_DIR}/dist"

# Mapeamento: nome do nó → arquivo de saída esperado
declare -A NODES=(
    [norte]="norte.uf2"
    [centro]="centro.uf2"
    [sul]="sul.uf2"
)

echo "[build_all] Limpando ${BUILD_DIR} e ${DIST_DIR}..."
rm -rf "${BUILD_DIR}" "${DIST_DIR}"
mkdir -p "${BUILD_DIR}" "${DIST_DIR}"

# ---------------------------------------------------------------------------
# Trap para sinalizar falha com número de linha
# ---------------------------------------------------------------------------
trap 'echo "[build_all] Falha na linha $LINENO. Saindo." >&2' ERR

# ---------------------------------------------------------------------------
# CMake configure
# ---------------------------------------------------------------------------
echo "[build_all] Configurando CMake (PICO_SDK_PATH=${PICO_SDK_PATH})..."
cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
    -DPICO_BOARD=pico_w \
    -DWIFI_SSID="${WIFI_SSID}" \
    -DWIFI_PASSWORD="${WIFI_PASSWORD}" \
    -DFALLBACK_SERVER_IP="${FALLBACK_SERVER_IP}"

# ---------------------------------------------------------------------------
# Build dos 3 targets em paralelo
# ---------------------------------------------------------------------------
echo "[build_all] Compilando all_nodes (norte / centro / sul)..."
cmake --build "${BUILD_DIR}" --target all_nodes \
    --parallel "$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)"

# ---------------------------------------------------------------------------
# Copia .uf2 para dist/ com nomes semânticos
# ---------------------------------------------------------------------------
echo "[build_all] Copiando binários para ${DIST_DIR}..."
for node in norte centro sul; do
    src="${BUILD_DIR}/${node}.uf2"
    dst="${DIST_DIR}/${node}.uf2"
    if [ ! -f "${src}" ]; then
        echo "[build_all] ERRO: Arquivo esperado não encontrado: ${src}" >&2
        exit 1
    fi
    cp "${src}" "${dst}"
done

# ---------------------------------------------------------------------------
# Relatório final com SHA256
# ---------------------------------------------------------------------------
echo ""
echo "================================================================"
echo " Build concluído — binários em ${DIST_DIR}"
echo "================================================================"

declare -A NODE_ROLES=(
    [norte]="NODE_ID=0, Crato"
    [centro]="NODE_ID=1, Juazeiro (servidor TCP)"
    [sul]="NODE_ID=2, Barbalha"
)

for node in norte centro sul; do
    file="${DIST_DIR}/${node}.uf2"
    sha=$(sha256sum "${file}" | awk '{print $1}')
    echo "  ${node}.uf2  [${NODE_ROLES[$node]}]"
    echo "    Path:   ${file}"
    echo "    SHA256: ${sha}"
    echo ""
done

echo "================================================================"
echo " Grava cada .uf2 na placa correspondente em modo BOOTSEL."
echo "  norte.uf2  → Pico do cruzamento Norte  (Crato)"
echo "  centro.uf2 → Pico central / servidor    (Juazeiro)"
echo "  sul.uf2    → Pico do cruzamento Sul     (Barbalha)"
echo "================================================================"
