#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255

// Tabla de usuarios
typedef struct {
  uint32_t id;
  char username[COLUMN_USERNAME_SIZE + 1];
  char email[COLUMN_EMAIL_SIZE + 1];
} Row;
// Fin de tabla de usuarios

/* Cálculo de offsets y tamaños compactos */
#define size_of_attribute(Struct, Attribute) sizeof(((Struct *)0)->Attribute)

static const uint32_t ID_SIZE = size_of_attribute(Row, id);
static const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
static const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);

#define ID_OFFSET 0
#define USERNAME_OFFSET (ID_OFFSET + ID_SIZE)
#define EMAIL_OFFSET (USERNAME_OFFSET + USERNAME_SIZE)
#define ROW_SIZE (ID_SIZE + USERNAME_SIZE + EMAIL_SIZE)

/* Configuración de páginas y tabla */
#define PAGE_SIZE 4096
#define TABLE_MAX_PAGES 100
#define ROWS_PER_PAGE (PAGE_SIZE / ROW_SIZE)
#define TABLE_MAX_ROWS (ROWS_PER_PAGE * TABLE_MAX_PAGES)

typedef struct {
  uint32_t num_rows;
  void *pages[TABLE_MAX_PAGES];
} Table;

/* Estructura para almacenar el buffer dinamico */

typedef struct {
  char *buffer;
  size_t buffer_length;
  ssize_t input_length;
} InputBuffer;

/* Inicializa el búfer en NULL para que getline() asigne memoria automáticamente
 */
/* Resultados de los meta-comandos (.) */
typedef enum {
  META_COMMAND_SUCCESS,
  META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

/* Resultados del analizador sintáctico (Parser) */
typedef enum {
  PREPARE_SUCCESS,
  PREPARE_SYNTAX_ERROR,
  PREPARE_UNRECOGNIZED_STATEMENT
} PrepareResult;

typedef enum { EXECUTE_SUCCESS, EXECUTE_TABLE_FULL } ExecuteResult;

/* Tipos de comandos SQL admitidos */
typedef enum { STATEMENT_INSERT, STATEMENT_SELECT } StatementType;

/* Representación interna del comando procesado */
typedef struct {
  StatementType type;
  Row row_to_insert; // Para utilizar solo en el insert
} Statement;

/* --- Serialización y Deserialización --- */

void serialize_row(Row *row, void *destination) {
  memcpy((char *)destination + ID_OFFSET, &(row)->id, ID_SIZE);
  memcpy((char *)destination + USERNAME_OFFSET, &(row)->username,
         USERNAME_SIZE);
  memcpy((char *)destination + EMAIL_OFFSET, &(row)->email, EMAIL_SIZE);
}

void deserialize_row(void *source, Row *destination) {
  memcpy(&(destination->id), (char *)source + ID_OFFSET, ID_SIZE);
  memcpy(&(destination->username), (char *)source + USERNAME_OFFSET,
         USERNAME_SIZE);
  memcpy(&(destination->email), (char *)source + EMAIL_OFFSET, EMAIL_SIZE);
}

/* Localiza el puntero de memoria exacto para una fila (Row) */
void *row_slot(Table *table, uint32_t row_num) {
  uint32_t page_num = row_num / ROWS_PER_PAGE;
  void *page = table->pages[page_num];

  if (page == NULL) {
    page = table->pages[page_num] = malloc(PAGE_SIZE);
  }

  uint32_t row_offset = row_num % ROWS_PER_PAGE;
  uint32_t bytes_offset = row_offset * ROW_SIZE;

  return (char *)page + bytes_offset;
}

Table *new_table() {
  Table *table = (Table *)malloc(sizeof(Table));
  table->num_rows = 0;
  for (int i = 0; i < TABLE_MAX_PAGES; i++) {
    table->pages[i] = NULL;
  }
  return table;
}

void free_table(Table *table) {
  for (int i = 0; i < TABLE_MAX_PAGES; i++) {
    if (table->pages[i] != NULL)
      free(table->pages[i]);
  }
  free(table);
}

InputBuffer *new_input_bugger() {
  InputBuffer *buffer = (InputBuffer *)malloc(sizeof(InputBuffer));
  buffer->buffer = NULL;
  buffer->input_length = 0;
  buffer->buffer_length = 0;

  return buffer;
}

/* Imprime el prompt en la consola */
void print_prompt() {
  printf("db > ");
  fflush(stdout);
}

/* Lee una línea completa de la entrada estándar */
void read_input(InputBuffer *buffer) {
  ssize_t bytes_read =
      getline(&(buffer->buffer), &(buffer->buffer_length), stdin);

  if (bytes_read <= 0) {
    printf("Error al leer la entrada o es el fin del archivo. \n");
    exit(EXIT_FAILURE);
  }

  buffer->input_length = bytes_read - 1;
  buffer->buffer[bytes_read - 1] = '\0';
}

/* Libera la memoria reservada por el InputBuffer */
void close_input_buffer(InputBuffer *buffer) {
  free(buffer->buffer);
  free(buffer);
}

void print_row(Row *row) {
  printf("(%" PRIu32 ", %s, %s)\n", row->id, row->username, row->email);
}

MetaCommandResult do_meta_command(InputBuffer *buffer) {
  if (strcmp(buffer->buffer, ".exit") == 0) {
    close_input_buffer(buffer);
    exit(EXIT_SUCCESS);
  }

  return META_COMMAND_UNRECOGNIZED_COMMAND;
}

PrepareResult prepare_statement(InputBuffer *buffer, Statement *statement) {
  if (strncmp(buffer->buffer, "insert", 6) == 0) {
    statement->type = STATEMENT_INSERT;
    int args_assigned = sscanf(buffer->buffer, "insert %" SCNu32 " %32s %255s",
                               &(statement->row_to_insert.id),
                               statement->row_to_insert.username,
                               statement->row_to_insert.email);

    if (args_assigned < 3) {
      return PREPARE_SYNTAX_ERROR;
    }

    return PREPARE_SUCCESS;
  }

  if (strncmp(buffer->buffer, "select", 6) == 0) {
    statement->type = STATEMENT_SELECT;
    return PREPARE_SUCCESS;
  }

  return PREPARE_UNRECOGNIZED_STATEMENT;
}

ExecuteResult execute_insert(Statement *statement, Table *table) {
  if (table->num_rows == TABLE_MAX_ROWS) {
    return EXECUTE_TABLE_FULL;
  }

  Row *row = &(statement)->row_to_insert;
  void *slot = row_slot(table, table->num_rows);
  serialize_row(row, slot);
  table->num_rows += 1;
  return EXECUTE_SUCCESS;
}

ExecuteResult execute_select(Statement *statement, Table *table) {
  (void)statement;
  Row row;
  for (uint32_t i = 0; i < table->num_rows; i++) {
    deserialize_row(row_slot(table, i), &row);
    print_row(&row);
  }

  return EXECUTE_SUCCESS;
}

ExecuteResult execute_statement(Statement *statement, Table *table) {
  switch (statement->type) {
  case STATEMENT_INSERT:
    return execute_insert(statement, table);
  case STATEMENT_SELECT:
    return execute_select(statement, table);
  }

  return EXECUTE_SUCCESS;
}
int main() {
  printf("Welcome to RSQL\n");
  Table *table = new_table();
  InputBuffer *buffer = new_input_bugger();

  while (true) {
    print_prompt();
    read_input(buffer);
    if (buffer->buffer[0] == '.') {
      switch (do_meta_command(buffer)) {
      case META_COMMAND_SUCCESS:
        continue;
      case META_COMMAND_UNRECOGNIZED_COMMAND:
        printf("Unknown command!\n");
        continue;
      }
    }

    Statement statement;
    switch (prepare_statement(buffer, &statement)) {
    case PREPARE_SUCCESS:
      break;
    case PREPARE_UNRECOGNIZED_STATEMENT:
      printf("No recognized sintax.\n");
      continue;
    case PREPARE_SYNTAX_ERROR:
      printf("Sintax error.\n");
      continue;
    }
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
