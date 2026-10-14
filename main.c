#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>

#include "storage.h"

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255

/* Estructura del registro */
typedef struct {
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE + 1];
    char email[COLUMN_EMAIL_SIZE + 1];
} Row;

/* Cálculo de offsets y dimensiones en bytes */
#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)

static const uint32_t ID_SIZE = size_of_attribute(Row, id);
static const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
static const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);

#define ID_OFFSET 0
#define USERNAME_OFFSET (ID_OFFSET + ID_SIZE)
#define EMAIL_OFFSET (USERNAME_OFFSET + USERNAME_SIZE)
#define ROW_SIZE (ID_SIZE + USERNAME_SIZE + EMAIL_SIZE)

/* Configuración de páginas y límites */
#define PAGE_SIZE BLOCK_SIZE
#define TABLE_MAX_PAGES MAX_BLOCKS
#define ROWS_PER_PAGE (PAGE_SIZE / ROW_SIZE)
#define TABLE_MAX_ROWS (ROWS_PER_PAGE * TABLE_MAX_PAGES)

/* Estructura de la base de datos vinculada al almacenamiento persistente */
typedef struct {
    uint32_t num_rows;
    Storage* storage;
} Table;

/* Búfer dinámico de entrada por teclado */
typedef struct {
    char* buffer;
    size_t buffer_length;
    ssize_t input_length;
} InputBuffer;

/* Códigos de resultado para el flujo de ejecución */
typedef enum {
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

typedef enum {
    PREPARE_SUCCESS,
    PREPARE_SYNTAX_ERROR,
    PREPARE_UNRECOGNIZED_STATEMENT
} PrepareResult;

typedef enum {
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL
} ExecuteResult;

typedef enum {
    STATEMENT_INSERT,
    STATEMENT_SELECT
} StatementType;

typedef struct {
    StatementType type;
    Row row_to_insert;
} Statement;

/* --- Serialización binaria --- */

void serialize_row(Row* source, void* destination) {
    memcpy((char*)destination + ID_OFFSET, &(source->id), ID_SIZE);
    memcpy((char*)destination + USERNAME_OFFSET, &(source->username), USERNAME_SIZE);
    memcpy((char*)destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
}

void deserialize_row(void* source, Row* destination) {
    memcpy(&(destination->id), (char*)source + ID_OFFSET, ID_SIZE);
    memcpy(&(destination->username), (char*)source + USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(&(destination->email), (char*)source + EMAIL_OFFSET, EMAIL_SIZE);
}

/* Localiza el puntero de memoria del registro usando el Storage */
void* row_slot(Table* table, uint32_t row_num) {
    uint32_t page_num = row_num / ROWS_PER_PAGE;
    void* page = storage_get_block(table->storage, page_num);

    uint32_t row_offset = row_num % ROWS_PER_PAGE;
    uint32_t byte_offset = row_offset * ROW_SIZE;
    return (char*)page + byte_offset;
}

/* Abre la BD, inicializa el Storage y deduce registros existentes */
Table* db_open(const char* filename) {
    Storage* storage = storage_open(filename);
    Table* table = (Table*)malloc(sizeof(Table));
    table->storage = storage;

    uint32_t full_blocks = storage->file_len / BLOCK_SIZE;
    uint32_t leftover_bytes = storage->file_len % BLOCK_SIZE;
    table->num_rows = (full_blocks * ROWS_PER_PAGE) + (leftover_bytes / ROW_SIZE);

    return table;
}

/* Cierra el Storage (escribe bloques a disco y libera memoria) */
void db_close(Table* table) {
    storage_close(table->storage, table->num_rows, ROW_SIZE, ROWS_PER_PAGE);
    free(table);
}

/* --- Gestión de Entrada de Usuario --- */

InputBuffer* new_input_buffer() {
    InputBuffer* input_buffer = (InputBuffer*)malloc(sizeof(InputBuffer));
    input_buffer->buffer = NULL;
    input_buffer->buffer_length = 0;
    input_buffer->input_length = 0;
    return input_buffer;
}

void print_prompt() {
    printf("db > ");
    fflush(stdout);
}

void read_input(InputBuffer* input_buffer) {
    ssize_t bytes_read = getline(&(input_buffer->buffer), &(input_buffer->buffer_length), stdin);

    if (bytes_read <= 0) {
        printf("Error al leer entrada o EOF.\n");
        exit(EXIT_FAILURE);
    }

    input_buffer->input_length = bytes_read - 1;
    input_buffer->buffer[bytes_read - 1] = '\0';
}

void close_input_buffer(InputBuffer* input_buffer) {
    free(input_buffer->buffer);
    free(input_buffer);
}

void print_row(Row* row) {
    printf("(%" PRIu32 ", %s, %s)\n", row->id, row->username, row->email);
}

/* --- Parser y Ejecutor --- */

PrepareResult prepare_statement(InputBuffer* input_buffer, Statement* statement) {
    if (strncmp(input_buffer->buffer, "insert", 6) == 0) {
        statement->type = STATEMENT_INSERT;
        int args_assigned = sscanf(
            input_buffer->buffer,
            "insert %" SCNu32 " %32s %255s",
            &(statement->row_to_insert.id),
            statement->row_to_insert.username,
            statement->row_to_insert.email
        );

        if (args_assigned < 3) {
            return PREPARE_SYNTAX_ERROR;
        }

        return PREPARE_SUCCESS;
    }

    if (strcmp(input_buffer->buffer, "select") == 0) {
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }

    return PREPARE_UNRECOGNIZED_STATEMENT;
}

ExecuteResult execute_insert(Statement* statement, Table* table) {
    if (table->num_rows >= TABLE_MAX_ROWS) {
        return EXECUTE_TABLE_FULL;
    }

    Row* row_to_insert = &(statement->row_to_insert);
    void* slot = row_slot(table, table->num_rows);

    serialize_row(row_to_insert, slot);
    table->num_rows += 1;

    return EXECUTE_SUCCESS;
}

ExecuteResult execute_select(Statement* statement, Table* table) {
    (void)statement;
    Row row;
    for (uint32_t i = 0; i < table->num_rows; i++) {
        deserialize_row(row_slot(table, i), &row);
        print_row(&row);
    }
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_statement(Statement* statement, Table* table) {
    switch (statement->type) {
        case STATEMENT_INSERT:
            return execute_insert(statement, table);
        case STATEMENT_SELECT:
            return execute_select(statement, table);
    }
    return EXECUTE_SUCCESS;
}

/* --- Ciclo Principal (REPL) --- */

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Uso: %s <nombre_archivo_bd>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char* filename = argv[1];
    Table* table = db_open(filename);
    InputBuffer* input_buffer = new_input_buffer();

    while (true) {
        print_prompt();
        read_input(input_buffer);

        // 1. Meta-comandos
        if (input_buffer->buffer[0] == '.') {
            if (strcmp(input_buffer->buffer, ".exit") == 0) {
                close_input_buffer(input_buffer);
                db_close(table);
                exit(EXIT_SUCCESS);
            } else {
                printf("Comando no reconocido: '%s'\n", input_buffer->buffer);
                continue;
            }
        }

        // 2. Preparación (Parser)
        Statement statement;
        switch (prepare_statement(input_buffer, &statement)) {
            case PREPARE_SUCCESS:
                break;
            case PREPARE_SYNTAX_ERROR:
                printf("Error de sintaxis. Uso esperado: insert <id> <usuario> <email>\n");
                continue;
            case PREPARE_UNRECOGNIZED_STATEMENT:
                printf("Palabra clave no reconocida al inicio de: '%s'\n", input_buffer->buffer);
                continue;
        }

        // 3. Ejecución
        switch (execute_statement(&statement, table)) {
            case EXECUTE_SUCCESS:
                printf("Ejecutado.\n");
                break;
            case EXECUTE_TABLE_FULL:
                printf("Error: La tabla está llena.\n");
                break;
        }
    }

    return 0;
}