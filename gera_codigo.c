#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "gera_codigo.h"

void add_prologo(unsigned char code[]) {
    unsigned char prologo[8] = {0x55, 0x48, 0x89, 0xE5, 0x48, 0x83, 0xEC, 0x20};
    memcpy(code, prologo, sizeof(prologo)); 
}

void add_epilogo(unsigned char code[]) {
    unsigned char epilogo[2] = {0xC9, 0xC3};
    memcpy(code, epilogo, sizeof(epilogo)); 
}

void retorna_const(unsigned char code[],int i, int valor) {
    code[i++] = 0xB8;
    memcpy(&code[i], &valor, 4); 
    i += 4;
    code[i++] = 0xC3;
}

void gera_codigo (FILE *f, unsigned char code[], funcp *entry) {
    char linha[100];
    int valor = 0;
    int encontrou_ret = 0;
    int i = 0;

    while (fgets(linha, sizeof(linha), f)) {
        linha[strcspn(linha, "\n")] = 0;
        if (strstr(linha, "function\n") != NULL) {
            add_prologo(code);
            encontrou_ret = 1;
        }
        if (sscanf(linha, "ret $%d", &valor) == 1) {
            retorna_const(code,i, valor);
        }
        if (strstr(linha, "end\n") != NULL) {
            add_epilogo(code);
        }
    }



    *entry = (funcp)code;
}