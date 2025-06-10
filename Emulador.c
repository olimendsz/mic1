#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Constantes
#define TAMANHO_MEMORIA 100000000
#define TAMANHO_ARMAZENAMENTO 512
#define BITS_MICROINSTRUCAO 36
#define BYTES_PALAVRA 4
#define ENDERECO_INICIO_PROGRAMA 0x0401
#define BYTES_INICIALIZACAO 20

// Tipos
typedef unsigned char byte;         // 8 bits
typedef unsigned int palavra;       // 32 bits
typedef unsigned long long microinstrucao;  // 64 bits (usa apenas 36 bits)

// Enumerações para facilitar leitura e manutenção
typedef enum {
    REG_MAR, REG_MDR, REG_PC, REG_MBR, REG_SP, REG_LV, REG_CPP, REG_TOS, REG_OPC, REG_H, REG_COUNT
} Registrador;

typedef enum {
    BUSB_MDR = 0, BUSB_PC, BUSB_MBR_SINAL, BUSB_MBR, BUSB_SP, BUSB_LV, BUSB_CPP, BUSB_TOS, BUSB_OPC
} FonteBarramentoB;

typedef enum {
    DESLOC_NENHUM = 0, DESLOC_ESQ8 = 1, DESLOC_DIR1 = 2
} TipoDeslocador;

// Registradores
palavra registradores[REG_COUNT] = {0};
microinstrucao MIR = 0;     // Microinstrução atual
palavra MPC = 0;            // Próximo endereço de microinstrução

// Barramentos
palavra barramentoB = 0, barramentoC = 0;

// Flags
bool flagN = false, flagZ = false;

// Decodificação da Microinstrução
byte MIR_B, MIR_Operacao, MIR_Deslocador, MIR_MEM, MIR_Pulo;
palavra MIR_C;

// Armazenamento de Controle e Memória Principal
microinstrucao ArmazenamentoControle[TAMANHO_ARMAZENAMENTO] = {0};
byte memoria[TAMANHO_MEMORIA] = {0};

// Protótipos
void carregar_armazenamento_controle(const char *arquivo);
void carregar_programa(const char *arquivo);
void exibir_estado(void);
void decodificar_microinstrucao(void);
void atribuir_barramentoB(void);
void operar_ULA(void);
void atribuir_barramentoC(void);
void operar_memoria(void);
void logica_pulo(void);
void imprimir_binario(const void* valor, int tipo);

// Função principal
int main(int argc, const char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <arquivo_programa>\n", argv[0]);
        return EXIT_FAILURE;
    }

    carregar_armazenamento_controle("microprog.rom");
    carregar_programa(argv[1]);

    while (true) {
        exibir_estado();
        MIR = ArmazenamentoControle[MPC];

        decodificar_microinstrucao();
        atribuir_barramentoB();
        operar_ULA();
        atribuir_barramentoC();
        operar_memoria();
        logica_pulo();
    }

    return EXIT_SUCCESS;
}

// Implementação das funções

void carregar_armazenamento_controle(const char *arquivo) {
    FILE *fp = fopen(arquivo, "rb");
    if (!fp) {
        perror("Não foi possível abrir o arquivo de microprograma");
        exit(EXIT_FAILURE);
    }
    fread(ArmazenamentoControle, sizeof(microinstrucao), TAMANHO_ARMAZENAMENTO, fp);
    fclose(fp);
}

void carregar_programa(const char *arquivo) {
    FILE *fp = fopen(arquivo, "rb");
    if (!fp) {
        perror("Não foi possível abrir o arquivo do programa");
        exit(EXIT_FAILURE);
    }
    palavra tamanho = 0;
    byte buf_tamanho[BYTES_PALAVRA];
    fread(buf_tamanho, sizeof(byte), BYTES_PALAVRA, fp);
    memcpy(&tamanho, buf_tamanho, BYTES_PALAVRA);
    fread(memoria, sizeof(byte), BYTES_INICIALIZACAO, fp);
    fread(&memoria[ENDERECO_INICIO_PROGRAMA], sizeof(byte), tamanho - BYTES_INICIALIZACAO, fp);
    fclose(fp);
}

void decodificar_microinstrucao(void) {
    MIR_B        = (MIR) & 0xF;
    MIR_MEM      = (MIR >> 4) & 0x7;
    MIR_C        = (MIR >> 7) & 0x1FF;
    MIR_Operacao = (MIR >> 16) & 0x3F;
    MIR_Deslocador = (MIR >> 22) & 0x3;
    MIR_Pulo     = (MIR >> 24) & 0x7;
    MPC          = (MIR >> 27) & 0x1FF;
}

void atribuir_barramentoB(void) {
    switch (MIR_B) {
        case BUSB_MDR:       barramentoB = registradores[REG_MDR]; break;
        case BUSB_PC:        barramentoB = registradores[REG_PC]; break;
        case BUSB_MBR_SINAL:
            barramentoB = registradores[REG_MBR];
            if (registradores[REG_MBR] & 0x80)
                barramentoB |= 0xFFFFFF00;
            break;
        case BUSB_MBR:       barramentoB = registradores[REG_MBR]; break;
        case BUSB_SP:        barramentoB = registradores[REG_SP]; break;
        case BUSB_LV:        barramentoB = registradores[REG_LV]; break;
        case BUSB_CPP:       barramentoB = registradores[REG_CPP]; break;
        case BUSB_TOS:       barramentoB = registradores[REG_TOS]; break;
        case BUSB_OPC:       barramentoB = registradores[REG_OPC]; break;
        default:             barramentoB = 0; break;
    }
}

void operar_ULA(void) {
    switch (MIR_Operacao) {
        case 12: barramentoC = registradores[REG_H] & barramentoB; break;
        case 17: barramentoC = 1; break;
        case 18: barramentoC = -1; break;
        case 20: barramentoC = barramentoB; break;
        case 24: barramentoC = registradores[REG_H]; break;
        case 26: barramentoC = ~registradores[REG_H]; break;
        case 28: barramentoC = registradores[REG_H] | barramentoB; break;
        case 44: barramentoC = ~barramentoB; break;
        case 53: barramentoC = barramentoB + 1; break;
        case 54: barramentoC = barramentoB - 1; break;
        case 57: barramentoC = registradores[REG_H] + 1; break;
        case 59: barramentoC = -registradores[REG_H]; break;
        case 60: barramentoC = registradores[REG_H] + barramentoB; break;
        case 61: barramentoC = registradores[REG_H] + barramentoB + 1; break;
        case 63: barramentoC = barramentoB - registradores[REG_H]; break;
        default: barramentoC = 0; break;
    }

    // Flags: N = negativo, Z = zero
    flagN = (barramentoC == 0);
    flagZ = (barramentoC != 0);

    // Deslocador
    switch (MIR_Deslocador) {
        case DESLOC_ESQ8: barramentoC <<= 8; break;
        case DESLOC_DIR1: barramentoC >>= 1; break;
        default: break;
    }
}

void atribuir_barramentoC(void) {
    if (MIR_C & 0x001) registradores[REG_MAR] = barramentoC;
    if (MIR_C & 0x002) registradores[REG_MDR] = barramentoC;
    if (MIR_C & 0x004) registradores[REG_PC]  = barramentoC;
    if (MIR_C & 0x008) registradores[REG_SP]  = barramentoC;
    if (MIR_C & 0x010) registradores[REG_LV]  = barramentoC;
    if (MIR_C & 0x020) registradores[REG_CPP] = barramentoC;
    if (MIR_C & 0x040) registradores[REG_TOS] = barramentoC;
    if (MIR_C & 0x080) registradores[REG_OPC] = barramentoC;
    if (MIR_C & 0x100) registradores[REG_H]   = barramentoC;
}

void operar_memoria(void) {
    palavra mar = registradores[REG_MAR];
    if (MIR_MEM & 0x1) registradores[REG_MBR] = memoria[registradores[REG_PC]];
    if (MIR_MEM & 0x2) memcpy(&registradores[REG_MDR], &memoria[mar * BYTES_PALAVRA], BYTES_PALAVRA);
    if (MIR_MEM & 0x4) memcpy(&memoria[mar * BYTES_PALAVRA], &registradores[REG_MDR], BYTES_PALAVRA);
}

void logica_pulo(void) {
    if (MIR_Pulo & 0x1) MPC |= (flagN << 8);
    if (MIR_Pulo & 0x2) MPC |= (flagZ << 8);
    if (MIR_Pulo & 0x4) MPC |= registradores[REG_MBR];
}

void exibir_estado(void) {
    printf("\n=== ESTADO ATUAL ===\n");
    printf("PC: 0x%08X | SP: 0x%08X | LV: 0x%08X | TOS: 0x%08X\n",
           registradores[REG_PC], registradores[REG_SP], registradores[REG_LV], registradores[REG_TOS]);
    printf("MAR: 0x%08X | MDR: 0x%08X | MBR: 0x%02X | CPP: 0x%08X | OPC: 0x%08X | H: 0x%08X\n",
           registradores[REG_MAR], registradores[REG_MDR], registradores[REG_MBR],
           registradores[REG_CPP], registradores[REG_OPC], registradores[REG_H]);
    printf("MPC: 0x%03X | MIR: 0x%09llX\n", MPC, MIR);

    // Você pode detalhar a visualização da pilha, memória, etc.

    getchar();
}

// Função utilitária para imprimir binário (adapte conforme necessidade)
void imprimir_binario(const void *valor, int tipo) {
    // Implemente conforme sua necessidade de visualização.
}
