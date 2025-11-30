#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>  
#include "gera_codigo.h"

#define CODE_SIZE 300

typedef enum { OP_CONST, OP_VAR, OP_PARAM } OpType;
typedef struct { OpType type; int value; } Operand;

static int add_prologo(unsigned char code[], int i) {
    unsigned char prologo[] = {
        0x55,                        
        0x48, 0x89, 0xE5,             
        0x48, 0x83, 0xEC, 0x20,       
        0x89, 0x7D, 0xFC              
    };
    memcpy(&code[i], prologo, sizeof(prologo));
    return i + sizeof(prologo);
}

static int add_epilogo(unsigned char code[], int i) {
    unsigned char epilogo[2] = { 0xC9, 0xC3 };
    memcpy(&code[i], epilogo, sizeof(epilogo));
    return i + sizeof(epilogo);
}

static int obter_deslocamento_para_vn(int vn, unsigned char* out_disp) {
    if (out_disp == NULL) return 0;
    switch (vn) {
    case 0: *out_disp = (unsigned char)0xF8; return 1; // -8(%rbp)   v0
    case 1: *out_disp = (unsigned char)0xF4; return 1; // -12(%rbp)  v1
    case 2: *out_disp = (unsigned char)0xF0; return 1; // -16(%rbp)  v2
    case 3: *out_disp = (unsigned char)0xEC; return 1; // -20(%rbp)  v3
    case 4: *out_disp = (unsigned char)0xE8; return 1; // -24(%rbp)  v4
    default:
        return 0;
    }
}

static int retorna_const(unsigned char code[], int i, int valor) {
    code[i++] = 0xB8;  
    memcpy(&code[i], &valor, 4);
    i += 4;
    return i;
}

static int efetua_op(unsigned char code[], int vn, int i) {
    unsigned char disp;
    if (!obter_deslocamento_para_vn(vn, &disp)) {
        fprintf(stderr, "Erro: índice de v inválido: %d\n", vn);
        return i;
    }
    code[i++] = 0x89; 
    code[i++] = 0x7D; 
    code[i++] = disp;
    return i;
}

static int movvar_eax(unsigned char code[], int i, int vn) {
    unsigned char disp;
    if (!obter_deslocamento_para_vn(vn, &disp)) {
        fprintf(stderr, "Erro: ret v%d não suportado\n", vn);
        return i;
    }
    code[i++] = 0x8B; 
    code[i++] = 0x45; 
    code[i++] = disp;
    return i;
}

static int movpar_eax(unsigned char code[], int i) {
    code[i++] = 0x89;
    code[i++] = 0xF8;
    return i;
}

static int carrega_token_edi(unsigned char code[], int i, const char* tok) {
    unsigned char disp;
    if (tok == NULL) return i;

    if (tok[0] == '$') {
        int val = (int)strtol(tok + 1, NULL, 0);
        code[i++] = 0xBF;       
        memcpy(&code[i], &val, 4);
        i += 4;
    }
    else if (tok[0] == 'v') {
        int vn = atoi(tok + 1);
        if (!obter_deslocamento_para_vn(vn, &disp)) {
            fprintf(stderr, "Erro: vn inválido no load arg: v%d\n", vn);
            return i;
        }
        code[i++] = 0x8B;             
        code[i++] = 0x7D;
        code[i++] = disp;
    }
    else if (tok[0] == 'p' && tok[1] == '0' && tok[2] == '\0') {
        code[i++] = 0x8B;            
        code[i++] = 0x7D;
        code[i++] = 0xFC;
    }
    else {
        fprintf(stderr, "Erro: token de argumento desconhecido: %s\n", tok);
    }
    return i;
}


static int parsear_token_para_operando(const char* tok, Operand* out) {
    if (tok == NULL || out == NULL) return 0;
    if (tok[0] == '$') {
        long val = strtol(tok + 1, NULL, 0);
        out->type = OP_CONST;
        out->value = (int)val;
        return 1;
    }
    else if (tok[0] == 'v') {
        int vn = atoi(tok + 1);
        out->type = OP_VAR;
        out->value = vn;
        return 1;
    }
    else if (tok[0] == 'p' && tok[1] == '0') {
        out->type = OP_PARAM;
        out->value = 0;
        return 1;
    }
    return 0;
}

static int gerar_codigo_load_eax(unsigned char code[], int i, Operand opnd) {
    // corrigido
    unsigned char disp;
    switch (opnd.type) {
    case OP_CONST:
        code[i++] = 0xB8;
        memcpy(&code[i], &opnd.value, 4);
        i += 4;
        break;
    case OP_VAR:
        if (!obter_deslocamento_para_vn(opnd.value, &disp)) {
            fprintf(stderr, "Erro: vn inválido no load: v%d\n", opnd.value);
            return i;
        }
        code[i++] = 0x8B; 
        code[i++] = 0x45; 
        code[i++] = disp;
        break;
    case OP_PARAM:
        code[i++] = 0x8B;  
        code[i++] = 0x45;
        code[i++] = 0xFC;
        break;
    }
    return i;
}

static int gerar_codigo_load_ecx(unsigned char code[], int i, Operand opnd) {
    unsigned char disp;
    switch (opnd.type) {
    case OP_CONST:
        code[i++] = 0xB9;
        memcpy(&code[i], &opnd.value, 4);
        i += 4;
        break;
    case OP_VAR:
        if (!obter_deslocamento_para_vn(opnd.value, &disp)) {
            fprintf(stderr, "Erro: vn inválido no load ecx: v%d\n", opnd.value);
            return i;
        }
        code[i++] = 0x8B; 
        code[i++] = 0x4D;
        code[i++] = disp;
        break;
    case OP_PARAM:
        code[i++] = 0x8B;  
        code[i++] = 0x4D;
        code[i++] = 0xFC; 
        break;
    }
    return i;
}

static int emitir_operacao_com_operando_direito(unsigned char code[], int i, char op, Operand rhs) {
    if (rhs.type == OP_CONST) {
        int imm = rhs.value;
        switch (op) {
        case '+':
            code[i++] = 0x05; 
            memcpy(&code[i], &imm, 4);
            i += 4;
            break;
        case '-':
            code[i++] = 0x2D; 
            memcpy(&code[i], &imm, 4);
            i += 4;
            break;
        case '*':
            code[i++] = 0x69; 
            code[i++] = 0xC0;
            memcpy(&code[i], &imm, 4);
            i += 4;
            break;
        }
    }
    else {
        i = gerar_codigo_load_ecx(code, i, rhs);
        switch (op) {
        case '+':
            code[i++] = 0x01; code[i++] = 0xC8; 
            break;
        case '-':
            code[i++] = 0x29; code[i++] = 0xC8; 
            break;
        case '*':
            code[i++] = 0x0F; code[i++] = 0xAF; code[i++] = 0xC1; 
            break;
        }
    }
    return i;
}


static int gerar_codigo_operacao_binaria(unsigned char code[], int i, int dest_v, Operand lhs, char op, Operand rhs) {
    i = gerar_codigo_load_eax(code, i, lhs);
    i = emitir_operacao_com_operando_direito(code, i, op, rhs);
    unsigned char disp;
    if (!obter_deslocamento_para_vn(dest_v, &disp)) {
        fprintf(stderr, "Erro: dest vn inválido: v%d\n", dest_v);
        return i;
    }
    code[i++] = 0x89; 
    code[i++] = 0x45; 
    code[i++] = disp; 
    return i;
}

static int atribui_var(unsigned char code[], int v1, int v2, int i) {
    unsigned char disp1, disp2;
    if (!obter_deslocamento_para_vn(v1, &disp1) || !obter_deslocamento_para_vn(v2, &disp2)) {
        fprintf(stderr, "Erro: índice de v inválido em atribuição v%d = v%d\n", v1, v2);
        return i;
    }
    code[i++] = 0x8B; code[i++] = 0x45; code[i++] = disp2; 
    code[i++] = 0x89; code[i++] = 0x45; code[i++] = disp1; 
    return i;
}

static int retorno_condicional(unsigned char code[], int i,
                               Operand cond, Operand retv) {
    i = gerar_codigo_load_eax(code, i, cond);

    code[i++] = 0x85;
    code[i++] = 0xC0;

    code[i++] = 0x75;         
    int jump_pos = i;
    code[i++] = 0x00;        

    i = gerar_codigo_load_eax(code, i, retv);

    i = add_epilogo(code, i);

    int jump_offset = i - (jump_pos + 1);
    code[jump_pos] = (unsigned char)jump_offset;

    return i;
}


static int retorno_incondicional_var(unsigned char code[], int i, int ret_var) {
    unsigned char disp;
    
    if (!obter_deslocamento_para_vn(ret_var, &disp)) {
        fprintf(stderr, "Erro: vn inválido no ret: v%d\n", ret_var);
        return i;
    }
    code[i++] = 0x8B; code[i++] = 0x45; code[i++] = disp;
    
    i = add_epilogo(code, i);
    
    return i;
}

static void dump_bytes(const unsigned char* buf, int len) {
    fprintf(stderr, "%d bytes:\n", len);
    for (int j = 0; j < len; ++j) {
        fprintf(stderr, "%02X ", buf[j]);
        if ((j + 1) % 16 == 0) fprintf(stderr, "\n");
    }
    if (len % 16) fprintf(stderr, "\n");
}

void gera_codigo(FILE* f, unsigned char code[], funcp* entry) {
    char linha[200];
    int valor = 0;
    int v = 0, v1 = 0, v2 = 0;
    int i = 0;
    char op;
    int started = 0;

    int* func_offsets = NULL;
    int func_count = 0;
    int func_cap = 0;

    typedef struct { int rel_pos; int target_index; } CallPatch;
    CallPatch* patches = NULL;
    int patch_count = 0;
    int patch_cap = 0;

    if (f == NULL) {
        fprintf(stderr, "Arquivo de entrada NULL\n");
        *entry = NULL;
        return;
    }

    while (fgets(linha, sizeof(linha), f)) {
        linha[strcspn(linha, "\r\n")] = 0; 

        if (strcmp(linha, "function") == 0) {
            if (func_count == func_cap) {
                func_cap = func_cap ? func_cap * 2 : 8;
                func_offsets = (int*)realloc(func_offsets, func_cap * sizeof(int));
            }
            func_offsets[func_count++] = i;
            i = add_prologo(code, i);
            started = 1;
            continue;
        }

        char zt1[64], zt2[64];
        if (sscanf(linha, "zret %63s %63s", zt1, zt2) == 2) {
            Operand cond, retv;
            if (!parsear_token_para_operando(zt1, &cond) ||
                !parsear_token_para_operando(zt2, &retv)) {
                fprintf(stderr, "Erro: tokens inválidos em zret: %s %s\n", zt1, zt2);
            } else {
                i = retorno_condicional(code, i, cond, retv);
            }
            continue;
        }

        if (sscanf(linha, "ret v%d", &v) == 1) {
            i = retorno_incondicional_var(code, i, v);
            continue;
        }

        char t1[64], t2[64];
        if (sscanf(linha, "v%d = %63s %c %63s", &v, t1, &op, t2) == 4) {
            Operand lhs, rhs;
            if (parsear_token_para_operando(t1, &lhs) && parsear_token_para_operando(t2, &rhs)) {
                i = gerar_codigo_operacao_binaria(code, i, v, lhs, op, rhs);
                continue;
            }
        }

        int target_idx;
        char argtok[64];
        if (sscanf(linha, "v%d = call %d %63s", &v, &target_idx, argtok) == 3) {

            i = carrega_token_edi(code, i, argtok);

            code[i++] = 0xE8;
            int rel_pos = i;

            code[i++] = 0; code[i++] = 0; code[i++] = 0; code[i++] = 0;

            if (patch_count == patch_cap) {
                patch_cap = patch_cap ? patch_cap * 2 : 8;
                patches = (CallPatch*)realloc(patches, patch_cap * sizeof(CallPatch));
            }
            patches[patch_count].rel_pos = rel_pos;
            patches[patch_count].target_index = target_idx;
            patch_count++;

            unsigned char disp;
            if (!obter_deslocamento_para_vn(v, &disp)) {
                fprintf(stderr, "Erro: dest vn inválido no call: v%d\n", v);
            }
            else {
                code[i++] = 0x89; code[i++] = 0x45; code[i++] = disp; 
            }
            continue;
        }

        if (sscanf(linha, "v%d = v%d", &v1, &v2) == 2) {
            i = atribui_var(code, v1, v2, i);
            continue;
        }

        if (sscanf(linha, "v%d = p0", &v) == 1) {
            i = efetua_op(code, v, i);
            continue;
        }

        if (sscanf(linha, "ret $%d", &valor) == 1) {
            i = retorna_const(code, i, valor);
            continue;
        }

        if (sscanf(linha, "ret v%d", &v) == 1) {
            i = movvar_eax(code, i, v);
            continue;
        }

        if (strcmp(linha, "ret p0") == 0) {
            i = movpar_eax(code, i);
            continue;
        }

        if (strcmp(linha, "end") == 0) {
            i = add_epilogo(code, i);
            continue;
        }
    }

    if (!started || i == 0) {
        fprintf(stderr, "Nenhum código gerado\n");
        *entry = NULL;
        free(func_offsets);
        free(patches);
        return;
    }

    for (int p = 0; p < patch_count; ++p) {
        int rel_pos = patches[p].rel_pos;
        int tidx = patches[p].target_index;
        if (tidx < 0 || tidx >= func_count) {
            fprintf(stderr, "Erro: índice de função alvo inválido no call: %d\n", tidx);
            continue;
        }
        int target_offset = func_offsets[tidx];
        int rel = target_offset - (rel_pos + 4);
        int32_t rel32 = (int32_t)rel;
        memcpy(&code[rel_pos], &rel32, 4);
    }

    //dump_bytes(code, i);

    size_t allocSize = CODE_SIZE;
    unsigned char* mem = (unsigned char*)mmap(NULL, allocSize, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        perror("mmap failed");
        free(func_offsets);
        free(patches);
        return;
    }

    memset(mem, 0x90, allocSize);
    memcpy(mem, code, i);

    if (mprotect(mem, allocSize, PROT_READ | PROT_EXEC) == -1) {
        perror("mprotect failed");
    }

    if (func_count > 0) {
        *entry = (funcp)(mem + func_offsets[func_count - 1]);
    } else {
        *entry = (funcp)mem;
    }

    free(func_offsets);
    free(patches);
}