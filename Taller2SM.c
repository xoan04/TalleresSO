#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>

// Funciones dadas
char **read_lines(char *filename, int *rows){
    char **matrix = NULL;
    char *line = NULL;
    int i = 0;
    FILE *fp = fopen(filename, "r");
    if (fp == NULL){perror("Error al abrir archivo");exit(EXIT_FAILURE);}
    int max_line_lenght = 1024;
    line = (char *)malloc(max_line_lenght * sizeof(char));
    matrix = (char **)malloc(max_line_lenght * sizeof(char *));
    while ( fgets(line, max_line_lenght, fp) != NULL){
        matrix[i] = line;
        line = (char *)malloc(max_line_lenght * sizeof(char));
        i++;
    }
    fclose(fp);
    *rows = i;
    return matrix;
}
int exec_system(char *cmd){
    int tub [2];
    pid_t child;
    if (pipe(tub)==-1){return -1;}
    child=fork();
    switch(child){
        case -1: return -1;
        case 0://Hijo
            close(tub[0]);
            dup2(tub[1], STDOUT_FILENO);
            execl("/bin/sh","sh","-c",cmd,NULL);
            return -1;
        default:
            close(tub[1]);
            return tub[0];
    }
}
void manejador(int sig){
    printf("senal capturada [%d]\n", sig);
}

int main() {
    int i, j, rows;
    char **matrix = read_lines("config.txt", &rows);

    // Tamaño de la región de memoria compartida
    size_t size = 1024;

    // Crea n objetos de memoria compartida, donde n es el número de líneas en el archivo
    int *fd = malloc(rows * sizeof(int));
    for (i = 0; i < rows; i++) {
        fd[i] = shm_open("/myshm", O_CREAT | O_RDWR, 0666);
        if (fd[i] == -1) {
            perror("shm_open");
            exit(EXIT_FAILURE);
        }

        // Configura el tamaño del objeto de memoria compartida
        if (ftruncate(fd[i], size) == -1) {
            perror("ftruncate");
            exit(EXIT_FAILURE);
        }
    }

    // Crea n hijos, donde n es el número de líneas en el archivo
    for (i = 0; i < rows; i++) {
        pid_t pid;
        int status;

        // Crea una región de memoria compartida para comunicar el hijo con el padre
        void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd[i], 0);
        if (ptr == MAP_FAILED) {
            perror("mmap");
            exit(EXIT_FAILURE);
        }

        // Crea un hijo
        pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) { // Hijo
            // Ejecuta el comando correspondiente
            int pipe_fd = exec_system(matrix[i]);

            // Escribe el resultado en la memoria compartida
            char *buffer = (char *)ptr;
            int nbytes;
            while ((nbytes = read(pipe_fd, buffer, size)) > 0) {
                buffer += nbytes;
                size -= nbytes;
            }

            // Cierra el descriptor de archivo y sale
            close(pipe_fd);
            exit(EXIT_SUCCESS);
        } else { // Padre
            // Espera a que el hijo termine
            waitpid(pid, &status, 0);

            // Muestra el resultado de la memoria compartida por pantalla
            char *buffer = (char *)ptr;
            write(STDOUT_FILENO, buffer, size);

            // Desvincula la región de memoria compartida y cierra el descriptor de archivo
            if (munmap(ptr, size) == -1) {
                perror("munmap");
                exit(EXIT_FAILURE);
            }
            if (shm_unlink("/myshm") == -1) {
                perror("shm_unlink");
                exit(EXIT_FAILURE);
            }
            close(fd[i]);
        }
    }

    return 0;
}