#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_PATH 1024
#define MAX_FILES 1000
// Sin límite de tamaño de archivo - ¡Gloria a Jehová!

// Estructura para archivos
typedef struct {
    char path[MAX_PATH];
    char var_name[MAX_PATH];
    int is_binary;
    long size;
    char* content;
} FileInfo;

FileInfo files[MAX_FILES];
int file_count = 0;
char app_name[256];

// Función para obtener nombre de la aplicación desde el directorio
void get_app_name() {
    char* cwd = getcwd(NULL, 0);
    char* last_slash = strrchr(cwd, '/');
    if (last_slash) {
        strcpy(app_name, last_slash + 1);
    } else {
        strcpy(app_name, cwd);
    }
    free(cwd);
}

// Convertir path a nombre de variable C válido
void path_to_var_name(const char* path, char* var_name) {
    strcpy(var_name, "file_");
    for (int i = 0; path[i]; i++) {
        if (path[i] == '/' || path[i] == '.' || path[i] == '-' || path[i] == ' ') {
            strcat(var_name, "_");
        } else {
            char c[2] = {path[i], 0};
            strcat(var_name, c);
        }
    }
}

// Verificar si un archivo es binario
int is_binary_file(const char* filepath) {
    FILE* f = fopen(filepath, "rb");
    if (!f) return 0;
    
    unsigned char buffer[1024];
    size_t bytes = fread(buffer, 1, sizeof(buffer), f);
    fclose(f);
    
    for (size_t i = 0; i < bytes; i++) {
        if (buffer[i] == 0) return 1;  // Contiene byte nulo = binario
    }
    return 0;
}

// Leer archivo completo
char* read_file(const char* filepath, long* size) {
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* content = malloc(*size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }
    
    fread(content, 1, *size, f);
    content[*size] = 0;
    fclose(f);
    
    return content;
}

// Escanear directorio recursivamente
void scan_directory(const char* base_path, const char* current_path) {
    char full_path[MAX_PATH];
    if (strlen(current_path) > 0) {
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, current_path);
    } else {
        strcpy(full_path, base_path);
    }
    
    DIR* dir = opendir(full_path);
    if (!dir) {
        return;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char item_path[MAX_PATH];
        snprintf(item_path, sizeof(item_path), "%s/%s", full_path, entry->d_name);
        
        char relative_path[MAX_PATH];
        if (strlen(current_path) > 0) {
            snprintf(relative_path, sizeof(relative_path), "%s/%s", current_path, entry->d_name);
        } else {
            // Determinar el prefijo basado en base_path
            if (strstr(base_path, "src/main")) {
                snprintf(relative_path, sizeof(relative_path), "main/%s", entry->d_name);
            } else if (strstr(base_path, "src/glue")) {
                snprintf(relative_path, sizeof(relative_path), "glue/%s", entry->d_name);
            } else {
                strcpy(relative_path, entry->d_name);
            }
        }
        
        struct stat st;
        if (stat(item_path, &st) != 0) {
            continue;
        }
        
        if (S_ISDIR(st.st_mode)) {
            // Es directorio, escanear recursivamente
            char new_current_path[MAX_PATH];
            if (strlen(current_path) > 0) {
                snprintf(new_current_path, sizeof(new_current_path), "%s/%s", current_path, entry->d_name);
            } else {
                if (strstr(base_path, "src/main")) {
                    snprintf(new_current_path, sizeof(new_current_path), "main/%s", entry->d_name);
                } else if (strstr(base_path, "src/glue")) {
                    snprintf(new_current_path, sizeof(new_current_path), "glue/%s", entry->d_name);
                } else {
                    strcpy(new_current_path, entry->d_name);
                }
            }
            scan_directory(base_path, new_current_path);
        } else {
            // Es archivo, agregarlo
            if (file_count >= MAX_FILES) {
                break;
            }
            
            FileInfo* file = &files[file_count];
            strcpy(file->path, relative_path);
            path_to_var_name(relative_path, file->var_name);
            file->is_binary = is_binary_file(item_path);
            file->content = read_file(item_path, &file->size);
            
            if (file->content) {
                file_count++;
            }
        }
    }
    closedir(dir);
}

// Ejecutar build.sh si existe
void execute_build_script() {
    if (access("src/glue/build.sh", F_OK) == 0) {
        // Hacer ejecutable
        system("chmod +x src/glue/build.sh");
        
        // Crear script temporal que incluye la función build
        FILE* temp_build = fopen("temp_build.sh", "w");
        fprintf(temp_build, "#!/bin/bash\n\n");
        fprintf(temp_build, "# Función build inyectada por Glue\n");
        fprintf(temp_build, "build() {\n");
        fprintf(temp_build, "    return 0\n");
        fprintf(temp_build, "}\n\n");
        fprintf(temp_build, "# Contenido original de build.sh\n");
        
        // Copiar contenido de build.sh (sin shebang si existe)
        FILE* original_build = fopen("src/glue/build.sh", "r");
        char line[1024];
        int first_line = 1;
        while (fgets(line, sizeof(line), original_build)) {
            if (first_line && strncmp(line, "#!/", 3) == 0) {
                first_line = 0;
                continue; // Saltar shebang
            }
            first_line = 0;
            fputs(line, temp_build);
        }
        fclose(original_build);
        fclose(temp_build);
        
        system("chmod +x temp_build.sh");
        system("./temp_build.sh");
        system("rm -f temp_build.sh");
    }
}

// Generar main.c
void generate_main_c() {
    FILE* f = fopen("main.c", "w");
    
    // Headers
    fprintf(f, "#include <stdio.h>\n");
    fprintf(f, "#include <stdlib.h>\n");
    fprintf(f, "#include <unistd.h>\n");
    fprintf(f, "#include <sys/stat.h>\n");
    fprintf(f, "#include <string.h>\n\n");
    
    fprintf(f, "// ========================================\n");
    fprintf(f, "// GLUE APPLICATION: %s\n", app_name);
    fprintf(f, "// Generado automáticamente - Con fe en Jehová\n");
    fprintf(f, "// ========================================\n\n");
    
    // Variables globales
    fprintf(f, "char temp_dir[256];\n\n");
    
    // Declarar archivos embebidos
    for (int i = 0; i < file_count; i++) {
        FileInfo* file = &files[i];
        if (file->is_binary) {
            fprintf(f, "const unsigned char %s[] = {", file->var_name);
            for (long j = 0; j < file->size; j++) {
                if (j % 16 == 0) fprintf(f, "\n    ");
                fprintf(f, "0x%02x", (unsigned char)file->content[j]);
                if (j < file->size - 1) fprintf(f, ",");
            }
            fprintf(f, "\n};\n");
            fprintf(f, "const size_t %s_size = %ld;\n\n", file->var_name, file->size);
        } else {
            fprintf(f, "const char* %s = ", file->var_name);
            fprintf(f, "\"");
            for (long j = 0; j < file->size; j++) {
                char c = file->content[j];
                switch (c) {
                    case '\n': fprintf(f, "\\n"); break;
                    case '\r': fprintf(f, "\\r"); break;
                    case '\t': fprintf(f, "\\t"); break;
                    case '\"': fprintf(f, "\\\""); break;
                    case '\\': fprintf(f, "\\\\"); break;
                    default:
                        if (c >= 32 && c <= 126) {
                            fprintf(f, "%c", c);
                        } else {
                            fprintf(f, "\\x%02x", (unsigned char)c);
                        }
                        break;
                }
            }
            fprintf(f, "\";\n\n");
        }
    }
    
    // Función para crear directorio temporal
    fprintf(f, "void create_temp_dir() {\n");
    fprintf(f, "    snprintf(temp_dir, sizeof(temp_dir), \"/tmp/glue_%%d\", getpid());\n");
    fprintf(f, "    mkdir(temp_dir, 0755);\n");
    fprintf(f, "    \n");
    fprintf(f, "    char main_dir[300], glue_dir[300];\n");
    fprintf(f, "    snprintf(main_dir, sizeof(main_dir), \"%%s/main\", temp_dir);\n");
    fprintf(f, "    snprintf(glue_dir, sizeof(glue_dir), \"%%s/glue\", temp_dir);\n");
    fprintf(f, "    mkdir(main_dir, 0755);\n");
    fprintf(f, "    mkdir(glue_dir, 0755);\n");
    fprintf(f, "}\n\n");
    
    // Función para extraer archivos
    fprintf(f, "void extract_files() {\n");
    fprintf(f, "    char filepath[400];\n");
    fprintf(f, "    char parent_dir[400];\n");
    fprintf(f, "    char mkdir_cmd[500];\n");
    fprintf(f, "    FILE* file;\n\n");
    
    for (int i = 0; i < file_count; i++) {
        FileInfo* file = &files[i];
        fprintf(f, "    // Extraer %s\n", file->path);
        fprintf(f, "    snprintf(filepath, sizeof(filepath), \"%%s/%%s\", temp_dir, \"%s\");\n", file->path);
        
        // Crear directorios padre si es necesario
        char* last_slash = strrchr(file->path, '/');
        if (last_slash) {
            char parent_path[MAX_PATH];
            strncpy(parent_path, file->path, last_slash - file->path);
            parent_path[last_slash - file->path] = 0;
            
            fprintf(f, "    snprintf(parent_dir, sizeof(parent_dir), \"%%s/%%s\", temp_dir, \"%s\");\n", parent_path);
            fprintf(f, "    snprintf(mkdir_cmd, sizeof(mkdir_cmd), \"mkdir -p %%s\", parent_dir);\n");
            fprintf(f, "    system(mkdir_cmd);\n");
        }
        
        if (file->is_binary) {
            fprintf(f, "    file = fopen(filepath, \"wb\");\n");
            fprintf(f, "    fwrite(%s, 1, %s_size, file);\n", file->var_name, file->var_name);
        } else {
            fprintf(f, "    file = fopen(filepath, \"w\");\n");
            fprintf(f, "    fputs(%s, file);\n", file->var_name);
        }
        fprintf(f, "    fclose(file);\n");
        
        // Hacer ejecutables los .sh
        if (strstr(file->path, ".sh")) {
            fprintf(f, "    chmod(filepath, 0755);\n");
        }
        fprintf(f, "\n");
    }
    fprintf(f, "}\n\n");
    
    // Función para ejecutar glue.sh
    fprintf(f, "void run_glue() {\n");
    fprintf(f, "    char command[500];\n");
    fprintf(f, "    snprintf(command, sizeof(command), \"cd %%s/main && ../glue/glue.sh\", temp_dir);\n");
    fprintf(f, "    system(command);\n");
    fprintf(f, "}\n\n");
    
    // Función de limpieza
    fprintf(f, "void cleanup() {\n");
    fprintf(f, "    char command[300];\n");
    fprintf(f, "    snprintf(command, sizeof(command), \"rm -rf %%s\", temp_dir);\n");
    fprintf(f, "    system(command);\n");
    fprintf(f, "}\n\n");
    
    // Función main
    fprintf(f, "int main() {\n");
    fprintf(f, "    create_temp_dir();\n");
    fprintf(f, "    extract_files();\n");
    fprintf(f, "    run_glue();\n");
    fprintf(f, "    cleanup();\n");
    fprintf(f, "    return 0;\n");
    fprintf(f, "}\n");
    
    fclose(f);
}

// Compilar y limpiar
void compile_and_cleanup() {
    // Crear directorio build/out si no existe
    system("mkdir -p build/out");
    
    char compile_cmd[512];
    snprintf(compile_cmd, sizeof(compile_cmd), "gcc -o build/out/%s main.c 2>/dev/null", app_name);
    
    if (system(compile_cmd) == 0) {
        // Eliminar main.c
        system("rm -f main.c");
    } else {
        exit(1);
    }
}

int main() {
    // Verificar estructura del proyecto
    if (access("src", F_OK) != 0 || access("src/main", F_OK) != 0 || access("src/glue", F_OK) != 0) {
        return 1;
    }
    
    get_app_name();
    
    // Ejecutar build.sh si existe
    execute_build_script();
    
    // Escanear archivos
    scan_directory("src/main", "");
    scan_directory("src/glue", "");
    
    if (file_count == 0) {
        return 1;
    }
    
    // Generar main.c
    generate_main_c();
    
    // Compilar y limpiar
    compile_and_cleanup();
    
    return 0;
}
