# SimpleDB (C Database from Scratch)

Un motor de base de datos relacional ligero escrito desde cero en **C (C99)** para Linux, inspirado en la arquitectura interna de **SQLite**.

El objetivo de este proyecto es construir progresivamente un sistema de gestión de base de datos funcional, implementando conceptos clave de ingeniería de software de sistemas: serialización binaria, gestión de memoria por páginas, persistencia en disco y árboles B+.

---

## 🚀 Estado Actual (Fase 1 y 2)

Actualmente el proyecto cuenta con:
- **REPL interactivo**: Búfer dinámico de entrada mediante `getline()` para evitar *buffer overflows*.
- **Parser de comandos**: Separación entre meta-comandos administrativos (`.exit`) y sentencias SQL (`insert`, `select`).
- **Esquema de datos fijo**: Estructura `Row` con identificador numérico de tamaño exacto (`uint32_t`) y cadenas limitadas.
- **Serialización en memoria**: Conversión directa de structs a bytes planos empaquetados (`memcpy`) evitando problemas de alineamiento (*padding*).
- **Almacenamiento por páginas**: Asignación de páginas en memoria virtual de **4 KB** bajo demanda, con capacidad inicial de hasta 1300 registros.

---

## 📋 Esquema de Datos

Cada registro (`Row`) consta de los siguientes campos:

| Campo | Tipo C | Tamaño | Descripción |
| :--- | :--- | :--- | :--- |
| `id` | `uint32_t` | 4 bytes | Clave primaria numérica |
| `username` | `char[33]` | 33 bytes | Nombre de usuario (máx. 32 caracteres + `\0`) |
| `email` | `char[256]` | 256 bytes | Correo electrónico (máx. 255 caracteres + `\0`) |
| **Total** | | **293 bytes** | Tamaño serializado por fila |

- **Tamaño de página:** 4096 bytes (4 KB).
- **Filas por página:** 13 filas ($4096 \div 293$).
- **Límite actual:** 100 páginas (1300 filas en memoria RAM).

---

## 🛠️ Requisitos y Compilación

### Requisitos
- Linux (probado en Debian / Ubuntu)
- GCC (`build-essential`)
- Make

### Comandos de Compilación

Compilar el binario:
```bash
make
