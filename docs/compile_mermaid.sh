#!/bin/bash

###############################################################################
# Script de Compilação de Diagramas Mermaid para PNG
# Projeto: AgroSat-IoT - Fase 2
# Autor: Equipe Orbitalis
# Data: 2025-01-15
###############################################################################

set -e  # Exit on error

# Cores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configurações
DOCS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FASE2_DIR="$DOCS_DIR/fase 2"
IMAGES_DIR="$DOCS_DIR/docs/images"
DOCUMENTO="$FASE2_DIR/Orbitalis_N3_Fase2_001.md"
TEMP_DIR="/tmp/agrosat_mermaid_$$"

# Criar diretórios necessários
mkdir -p "$IMAGES_DIR"
mkdir -p "$TEMP_DIR"

echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║  Compilador de Diagramas Mermaid - AgroSat-IoT           ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Verificar se o documento existe
if [ ! -f "$DOCUMENTO" ]; then
    echo -e "${RED}✗ Erro: Documento não encontrado em $DOCUMENTO${NC}"
    exit 1
fi

# Verificar se mmdc está instalado
if ! command -v mmdc &> /dev/null; then
    echo -e "${YELLOW}⚠ mermaid-cli (mmdc) não encontrado!${NC}"
    echo ""
    echo "Instalando mermaid-cli via npm..."
    echo ""
    
    # Verificar se npm está instalado
    if ! command -v npm &> /dev/null; then
        echo -e "${RED}✗ npm não encontrado. Instale Node.js primeiro:${NC}"
        echo "  sudo pacman -S nodejs npm"
        echo "  ou"
        echo "  sudo apt install nodejs npm"
        exit 1
    fi
    
    # Instalar mermaid-cli globalmente
    npm install @mermaid-js/mermaid-cli
    
    if [ $? -ne 0 ]; then
        echo -e "${RED}✗ Falha ao instalar mermaid-cli${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}✓ mermaid-cli instalado com sucesso!${NC}"
    echo ""
fi

# Configuração Mermaid para PNG de alta qualidade
cat > "$TEMP_DIR/mermaid-config.json" << 'EOF'
{
  "theme": "base",
  "themeVariables": {
    "primaryColor": "#e3f2fd",
    "primaryTextColor": "#1a1a1a",
    "primaryBorderColor": "#1976d2",
    "lineColor": "#546e7a",
    "secondaryColor": "#fff8e1",
    "tertiaryColor": "#e8f5e9",
    "fontSize": "16px",
    "fontFamily": "Arial, sans-serif"
  },
  "flowchart": {
    "curve": "basis",
    "padding": 20
  },
  "sequence": {
    "diagramMarginX": 50,
    "diagramMarginY": 10,
    "actorMargin": 50,
    "width": 150,
    "height": 65,
    "boxMargin": 10,
    "boxTextMargin": 5,
    "noteMargin": 10,
    "messageMargin": 35
  },
  "stateDiagram": {
    "padding": 20
  }
}
EOF

echo -e "${BLUE}[1/4] Extraindo diagramas Mermaid do documento...${NC}"

# Função para extrair diagrama Mermaid
extract_mermaid() {
    local anexo=$1
    local output_file=$2
    local start_line=$3
    
    echo -e "  → Extraindo Anexo $anexo..."
    
    # Encontrar o bloco Mermaid após a linha especificada
    awk -v start="$start_line" '
        NR >= start && /^```mermaid/ { in_block=1; next }
        in_block && /^```$/ { exit }
        in_block { print }
    ' "$DOCUMENTO" > "$output_file"
    
    if [ -s "$output_file" ]; then
        echo -e "    ${GREEN}✓${NC} Extraído $(wc -l < "$output_file") linhas"
        return 0
    else
        echo -e "    ${RED}✗${NC} Falha na extração"
        return 1
    fi
}

# Extrair cada diagrama
extract_mermaid "D" "$TEMP_DIR/anexo_d.mmd" 368
extract_mermaid "H" "$TEMP_DIR/anexo_h.mmd" 419
extract_mermaid "N" "$TEMP_DIR/anexo_n.mmd" 494

echo ""
echo -e "${BLUE}[2/4] Compilando diagramas para PNG...${NC}"

# Função para compilar Mermaid para PNG
compile_mermaid() {
    local input_file=$1
    local output_file=$2
    local anexo=$3
    
    echo -e "  → Compilando Anexo $anexo..."
    
    if [ ! -f "$input_file" ]; then
        echo -e "    ${RED}✗${NC} Arquivo fonte não encontrado"
        return 1
    fi
    
    # Compilar com mmdc
    npx -y @mermaid-js/mermaid-cli -i "$input_file" \
         -o "$output_file" \
         -c "$TEMP_DIR/mermaid-config.json" \
         -b transparent \
         -w 1920 \
         -H 1080 \
         -s 2 \
         2>&1 | grep -v "Generating single mermaid chart" || true
    
    if [ -f "$output_file" ]; then
        local size=$(du -h "$output_file" | cut -f1)
        echo -e "    ${GREEN}✓${NC} Gerado: $output_file ($size)"
        return 0
    else
        echo -e "    ${RED}✗${NC} Falha na compilação"
        return 1
    fi
}

# Compilar cada diagrama
compile_mermaid "$TEMP_DIR/anexo_d.mmd" "$IMAGES_DIR/anexo_d.png" "D"
compile_mermaid "$TEMP_DIR/anexo_h.mmd" "$IMAGES_DIR/anexo_h.png" "H"
compile_mermaid "$TEMP_DIR/anexo_n.mmd" "$IMAGES_DIR/anexo_n.png" "N"

echo ""
echo -e "${BLUE}[3/4] Otimizando imagens...${NC}"

# Verificar se optipng está disponível para otimização
if command -v optipng &> /dev/null; then
    echo -e "  → Otimizando PNGs com optipng..."
    for png in "$IMAGES_DIR"/anexo_*.png; do
        if [ -f "$png" ]; then
            optipng -quiet -o2 "$png"
            echo -e "    ${GREEN}✓${NC} Otimizado: $(basename "$png")"
        fi
    done
else
    echo -e "  ${YELLOW}⚠${NC} optipng não encontrado, pulando otimização"
    echo -e "    Instale com: sudo pacman -S optipng"
fi

echo ""
echo -e "${BLUE}[4/4] Limpando arquivos temporários...${NC}"
rm -rf "$TEMP_DIR"
echo -e "  ${GREEN}✓${NC} Limpeza concluída"

echo ""
echo -e "${GREEN}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  ✓ Compilação concluída com sucesso!                      ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "Diagramas gerados em: ${BLUE}$IMAGES_DIR${NC}"
echo ""
ls -lh "$IMAGES_DIR"/anexo_*.png 2>/dev/null || echo "Nenhum arquivo gerado"
echo ""
echo -e "${YELLOW}Próximos passos:${NC}"
echo "  1. Verifique os PNGs gerados"
echo "  2. Insira as imagens no documento substituindo os placeholders"
echo "  3. Compile o documento final para PDF"
echo ""