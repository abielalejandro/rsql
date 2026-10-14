#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include <sys/types.h>

#define BLOCK_SIZE 4096
#define MAX_BLOCKS 100

/* Gestor de archivos binarios y caché de memoria */
typedef struct {
    int fd;
    off_t file_len;
    void* blocks[MAX_BLOCKS];
} Storage;

/* Abre o crea el archivo en disco y devuelve el Storage */
Storage* storage_open(const char* filepath);

/* Obtiene un bloque de memoria (lo carga de disco si no estaba en RAM) */
void* storage_get_block(Storage* st, uint32_t block_idx);

/* Sincroniza bloques modificados y cierra el descriptor */
void storage_close(Storage* st, uint32_t total_rows, uint32_t row_size, uint32_t rows_per_block);

#endif