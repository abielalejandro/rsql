#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

Storage* storage_open(const char* filepath) {
    // Abrir lectura/escritura, crear si no existe, permisos 0644
    int fd = open(filepath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        perror("Error al abrir archivo de BD");
        exit(EXIT_FAILURE);
    }

    off_t file_len = lseek(fd, 0, SEEK_END);

    Storage* st = (Storage*)malloc(sizeof(Storage));
    st->fd = fd;
    st->file_len = file_len;

    for (uint32_t i = 0; i < MAX_BLOCKS; i++) {
        st->blocks[i] = NULL;
    }

    return st;
}

void* storage_get_block(Storage* st, uint32_t block_idx) {
    if (block_idx >= MAX_BLOCKS) {
        fprintf(stderr, "Índice de bloque fuera de rango: %u\n", block_idx);
        exit(EXIT_FAILURE);
    }

    // Si ya está en RAM, lo devolvemos directo
    if (st->blocks[block_idx] == NULL) {
        void* block = malloc(BLOCK_SIZE);

        lseek(st->fd, block_idx * BLOCK_SIZE, SEEK_SET);
        ssize_t bytes_read = read(st->fd, block, BLOCK_SIZE);
        if (bytes_read < 0) {
            perror("Error al leer bloque de disco");
            exit(EXIT_FAILURE);
        }

        st->blocks[block_idx] = block;
    }

    return st->blocks[block_idx];
}

void storage_close(Storage* st, uint32_t total_rows, uint32_t row_size, uint32_t rows_per_block) {
    uint32_t full_blocks = total_rows / rows_per_block;

    for (uint32_t i = 0; i < MAX_BLOCKS; i++) {
        if (st->blocks[i] == NULL) {
            continue;
        }

        lseek(st->fd, i * BLOCK_SIZE, SEEK_SET);

        // Determinamos cuántos bytes escribir en disco
        size_t bytes_to_write = BLOCK_SIZE;
        if (i == full_blocks) {
            uint32_t leftover_rows = total_rows % rows_per_block;
            bytes_to_write = leftover_rows * row_size;
        }

        if (bytes_to_write > 0) {
            ssize_t written = write(st->fd, st->blocks[i], bytes_to_write);
            if (written < 0) {
                perror("Error al sincronizar datos a disco");
                exit(EXIT_FAILURE);
            }
        }

        free(st->blocks[i]);
        st->blocks[i] = NULL;
    }

    close(st->fd);
    free(st);
}