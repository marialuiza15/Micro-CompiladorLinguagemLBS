#include <stdio.h>
#include <stdlib.h>
#include "gera_codigo.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    FILE* fp = fopen("teste.lbs", "r");


    unsigned char code[300];
    funcp funcLBS = NULL;

    gera_codigo(fp, code, &funcLBS);

    int res = (*funcLBS)(10);
    printf("Resultado = %d\n", res);

    fclose(fp);
    return 0;
}

