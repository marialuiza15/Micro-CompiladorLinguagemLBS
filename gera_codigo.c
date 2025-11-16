#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gera_codigo.h"

#define MAX_SIZE (1024 * 1024)  // 1MB

typedef enum { OPER_LOCAL, OPER_PARAM, OPER_CONST } OperTipo;

typedef struct {
    OperTipo tipo;
    int valor;   // índice da variável / parâmetro ou valor constante
} Operando;

int parse_varpc(const char *s, Operando *out) {
    if (s[0] == 'v') {          // variável local
        out->tipo  = OPER_LOCAL;
        out->valor = atoi(s + 1);
    } else if (s[0] == 'p') {   // parâmetro
        out->tipo  = OPER_PARAM;
        out->valor = atoi(s + 1);
    } else if (s[0] == '$') {   // constante
        out->tipo  = OPER_CONST;
        out->valor = atoi(s + 1);
    } else {
        return 0;               // formato inválido
    }
    return 1;
}

// calcula deslocamento da variável local vN em relação a %rbp
static int offset_local(int idx) {
    // v0 -> -4, v1 -> -8, ..., v4 -> -20
    return -4 * (idx + 1);
}

// grava %eax em v[idx]  => mov %eax, -offset(%rbp)
static void store_eax_in_local(unsigned char *code, int *total_size, int idx) {
    int off = offset_local(idx);
    unsigned char disp8 = (unsigned char)(off & 0xFF); // ex: -4 -> 0xFC

    code[(*total_size)++] = 0x89;        // mov r/m32, r32
    code[(*total_size)++] = 0x45;        // [rbp + disp8], reg = eax
    code[(*total_size)++] = disp8;
}

// carrega Operando em %eax ou %edx
// reg = 0 -> eax, reg = 1 -> edx
static void load_operand_to_reg(unsigned char *code, int *total_size,
                                Operando *op, int reg) {
    if (op->tipo == OPER_LOCAL) {
        int off = offset_local(op->valor);
        unsigned char disp8 = (unsigned char)(off & 0xFF);

        code[(*total_size)++] = 0x8B;        // mov r32, r/m32
        code[(*total_size)++] = (reg == 0) ? 0x45 : 0x55; // 45 -> eax, 55 -> edx
        code[(*total_size)++] = disp8;
    } else if (op->tipo == OPER_PARAM) {
        // só p0, que chega em %edi
        if (op->valor != 0) {
            fprintf(stderr, "So suportamos p0 por enquanto.\n");
            exit(1);
        }
        code[(*total_size)++] = 0x89;                // mov r/m32, r32
        code[(*total_size)++] = (reg == 0) ? 0xF8    // 89 F8 -> mov %eax, %edi (eax = edi)
                                           : 0xFA;   // 89 FA -> mov %edx, %edi (edx = edi)
    } else if (op->tipo == OPER_CONST) {
        int val = op->valor;
        // mov imm32 -> eax ou edx
        code[(*total_size)++] = (reg == 0) ? 0xB8    // mov eax, imm32
                                           : 0xBA;   // mov edx, imm32
        memcpy(code + *total_size, &val, 4);
        *total_size += 4;
    }
}

void gera_codigo(FILE *f, unsigned char *code, funcp *entry) {
    printf("Inicio\n");

    unsigned char *end_funcoes[10];
    int func_atual = -1;
    int total_size = 0;
    int num_calls = 0;
    int num_func = 0;
    char linha[100];
    int valor2 = 0;
    char op;

    char dst[16], op1[16], op2[16];
    Operando o_dst, o1, o2;

    // 
    typedef struct {
        unsigned char *pos_rel32; 
        int func_chamada; 
    } CallFixup;
    CallFixup calls[50];
    
    unsigned char prologo[] = {
        0x55,                   // push %rbp
        0x48, 0x89, 0xE5,       // mov %rsp, %rbp
        0x48, 0x83, 0xEC, 0x20  // sub $32, %rsp
    };


    unsigned char retseq[] = {
        0x8B, 0x45, 0xFC, 
        0xC9,             
        0xC3            
    };

    while (fgets(linha, sizeof(linha), f)) {
        linha[strcspn(linha, "\n")] = 0; 

        if (strcmp(linha, "function") == 0) {
            func_atual++;

            end_funcoes[func_atual] = code + total_size;

            if (total_size + (int)sizeof(prologo) > MAX_SIZE) {
                fprintf(stderr, "Memoria excedeu o limite de 1MB (prologo)\n");
                exit(1);
            }

            memcpy(code + total_size, prologo, sizeof(prologo));
            total_size += sizeof(prologo);
            continue;
        }

        if (strcmp(linha, "end") == 0) {
            if (total_size + (int)sizeof(retseq) > MAX_SIZE) {
                fprintf(stderr, "Memoria excedeu o limite de 1MB (ret)\n");
                exit(1);
            }

            memcpy(code + total_size, retseq, sizeof(retseq));
            total_size += sizeof(retseq);
            continue;
        }

        if (sscanf(linha, " %15s = %15s %c %15s", dst, op1, &op, op2) == 4) {
            if (!parse_varpc(dst, &o_dst) ||
                !parse_varpc(op1, &o1)   ||
                !parse_varpc(op2, &o2)) {
                fprintf(stderr, "Erro: varpc invalido na linha: %s\n", linha);
                exit(1);
            }

            if (o_dst.tipo != OPER_LOCAL) {
                fprintf(stderr, "Destino da atribuicao deve ser variavel local (v0..v4): %s\n", dst);
                exit(1);
            }

            // 1) carrega op1 em %eax
            load_operand_to_reg(code, &total_size, &o1, 0); // reg=0 -> eax

            // 2) aplica operacao com op2
            if (o2.tipo == OPER_CONST) {
                int imm = o2.valor;
                if (op == '+') {
                    // add imm32, %eax  -> 05 imm32
                    code[total_size++] = 0x05;
                    memcpy(code + total_size, &imm, 4);
                    total_size += 4;
                } else if (op == '-') {
                    // sub imm32, %eax  -> 2D imm32
                    code[total_size++] = 0x2D;
                    memcpy(code + total_size, &imm, 4);
                    total_size += 4;
                } else if (op == '*') {
                    // imul eax, imm32  -> 69 C0 imm32
                    code[total_size++] = 0x69;
                    code[total_size++] = 0xC0;
                    memcpy(code + total_size, &imm, 4);
                    total_size += 4;
                } else {
                    fprintf(stderr, "Operador desconhecido: %c\n", op);
                    exit(1);
                }
            } else {
                // op2 em %edx
                load_operand_to_reg(code, &total_size, &o2, 1); // reg=1 -> edx

                if (op == '+') {
                    // add %edx, %eax -> 01 D0
                    code[total_size++] = 0x01;
                    code[total_size++] = 0xD0;
                } else if (op == '-') {
                    // sub %edx, %eax -> 29 D0
                    code[total_size++] = 0x29;
                    code[total_size++] = 0xD0;
                } else if (op == '*') {
                    // imul %edx, %eax -> 0F AF C2
                    code[total_size++] = 0x0F;
                    code[total_size++] = 0xAF;
                    code[total_size++] = 0xC2;
                } else {
                    fprintf(stderr, "Operador desconhecido: %c\n", op);
                    exit(1);
                }
            }

            // 3) salva %eax em vN (dst)
            store_eax_in_local(code, &total_size, o_dst.valor);

            continue;
        }

        if (sscanf(linha, "call %d v0", &num_func) == 1) {
            if (num_func < 0 || num_func >= 10) {
                fprintf(stderr, "Indice de funcao invalido no call: %d\n", num_func);
                exit(1);
            }

            {
                unsigned char mov_v0_eax[] = {0x8B, 0x45, 0xFC};
                unsigned char mov_eax_edi[] = {0x89, 0xC7};

                if (total_size + (int)sizeof(mov_v0_eax) +
                    (int)sizeof(mov_eax_edi) + 5 +
                    3 > MAX_SIZE) { 
                    fprintf(stderr, "Memoria excedeu o limite (call)\n");
                    exit(1);
                }

                memcpy(code + total_size, mov_v0_eax, sizeof(mov_v0_eax));
                total_size += sizeof(mov_v0_eax);

                memcpy(code + total_size, mov_eax_edi, sizeof(mov_eax_edi));
                total_size += sizeof(mov_eax_edi);
            }

            unsigned char *call_pos = code + total_size;
            call_pos[0] = 0xE8;
            *(int *)(call_pos + 1) = 0; 

            if (num_calls >= 50) {
                fprintf(stderr, "Muitos calls\n");
                exit(1);
            }

            calls[num_calls].pos_rel32 = call_pos + 1;
            calls[num_calls].func_chamada = num_func;
            num_calls++;

            total_size += 5;

            {
                unsigned char salva_ret[] = {0x89, 0x45, 0xFC}; 
                memcpy(code + total_size, salva_ret, sizeof(salva_ret));
                total_size += sizeof(salva_ret);
            }

            continue;
        }

        if (strcmp(linha, "ret v0") == 0) {
            continue;
        }

        if (linha[0] != '\0') {
            fprintf(stderr, "Linha nao reconhecida: '%s'\n", linha);
        }
    }

    for (int i = 0; i < num_calls; i++) {
        unsigned char *pos_rel32 = calls[i].pos_rel32;
        int fidx = calls[i].func_chamada;

        unsigned char *addr_call_next = pos_rel32 + 4;   
        unsigned char *addr_dest = end_funcoes[fidx];   

        int desloc = (int)(addr_dest - addr_call_next);
        memcpy(pos_rel32, &desloc, 4);
    }

    *entry = (funcp)end_funcoes[0];
}