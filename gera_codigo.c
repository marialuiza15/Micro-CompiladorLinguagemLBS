#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "gera_codigo.h"

void gera_codigo(FILE *f, unsigned char code[], funcp *entry) {
    char linha[100];
    int i = 0;
    int valor = 0;

    unsigned char prologo[] = {0x55, 0x48, 0x89, 0xE5, 0x48, 0x83, 0xEC, 0x08};
    memcpy(code + i, prologo, sizeof(prologo));
    i += sizeof(prologo);

    while (fgets(linha, sizeof(linha), f)) {
        if (sscanf(linha, "v0 = p0 + $%d", &valor) == 1) {
            code[i++] = 0x89; code[i++] = 0xF8;
            code[i++] = 0x81; code[i++] = 0xC0;
            memcpy(code + i, &valor, 4); i += 4;
            unsigned char salva[] = {0x89, 0x45, 0xFC};
            memcpy(code + i, salva, sizeof(salva));
            i += sizeof(salva);
        }
    }

    unsigned char retseq[] = {
        0x8B, 0x45, 0xFC,
        0xC9,            
        0xC3           
    };
    memcpy(code + i, retseq, sizeof(retseq));
    i += sizeof(retseq);

    *entry = (funcp)code;
}
