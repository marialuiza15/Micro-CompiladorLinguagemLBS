#include <stdio.h>
#include <stdlib.h>
#include "gera_codigo.h"
#include <sys/mman.h>  // Para mmap

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    FILE *fp = fopen("teste.lbs", "r");
    if (!fp) {
    
        fp = tmpfile();
        if (!fp) { perror("tmpfile"); return 1; }
    }

    // Aloca 1MB de memória para armazenar as funções
    unsigned char *code = (unsigned char*)mmap(NULL, 1024 * 1024, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (code == MAP_FAILED) {
        perror("Falha na alocação de memória");
        exit(1);
    }

    funcp funcLBS = NULL;

    gera_codigo(fp, code, &funcLBS);

    int res = (*funcLBS)(10);
    printf("Resultado = %d\n", res);

    fclose(fp);
    return 0;
}
