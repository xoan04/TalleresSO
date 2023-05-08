#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

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
    signal(SIGINT, manejador);
    int i, j, rows;
    char **matrix = read_lines("config.txt", &rows);

    // Crea n hijos, donde n es el número de líneas en el archivo
    for (i = 0; i < rows; i++) {
        pid_t pid;
        int status;
        int fd[2];
        // Crea una tubería para comunicar el hijo con el padre
        if (pipe(fd) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
        // Crea un hijo
        pid = fork();
        if (pid == 0) { // Hijo
            // Cierra el extremo de lectura de la tubería
            close(fd[0]);
            // Ejecuta el comando correspondiente
            int pipe_fd = exec_system(matrix[i]);
            // Escribe el resultado en la tubería
            char buffer[1024];
            int nbytes;
            while ((nbytes = read(pipe_fd, buffer, sizeof(buffer))) > 0) {write(fd[1], buffer, nbytes);}
            // Cierra el extremo de escritura de la tubería y sale
            close(fd[1]);
            exit(EXIT_SUCCESS);
        } else { // Padre
            // Cierra el extremo de escritura de la tubería
            close(fd[1]);

            // Lee el resultado de la tubería y lo muestra por pantalla
            char buffer[1024];
            int nbytes;
            while ((nbytes = read(fd[0], buffer, sizeof(buffer))) > 0) {
                write(STDOUT_FILENO, buffer, nbytes);                
            }
            // Espera a que el hijo termine
            waitpid(pid, &status, 0);
            // Cierra el extremo de lectura de la tubería
            close(fd[0]);
        }
    }
    return 0;
}