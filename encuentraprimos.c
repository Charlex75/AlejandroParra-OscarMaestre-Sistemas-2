#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#define LONGITUD_MSG 100           //Payload del mensaje
#define LONGITUD_MSG_ERR 200       //Mensajes de error por pantalla

//Códigos de exit por error
#define ERR_ENTRADA_ERRONEA 2
#define ERR_SEND 3
#define ERR_RECV 4
#define ERR_FSAL 5

#define NOMBRE_FICH "primos.txt"
#define NOMBRE_FICH_CUENTA "cuentaprimos.txt"
#define CADA_CUANTOS_ESCRIBO 5

//Rango de búsqueda, desde BASE a BASE+RANGO
#define BASE 800000000
#define RANGO 200

//Intervalo del temporizador para RAIZ
#define INTERVALO_TIMER 5

//Códigos de mensaje para el campo mesg_type del tipo T_MESG_BUFFER
#define COD_ESTOY_AQUI 5           //Un calculador indica al SERVER que está preparado
#define COD_LIMITES 4              //Mensaje del SERVER al calculador indicando los límites de operación
#define COD_RESULTADOS 6           //Localizado un primo
#define COD_FIN 7                  //Final del procesamiento de un calculador

//Mensaje que se intercambia
typedef struct {
    long mesg_type;
    char mesg_text[LONGITUD_MSG];
} T_MESG_BUFFER;
int Comprobarsiesprimo(long int numero);
void Informar(char *texto, int verboso);
void Imprimirjerarquiaproc(int pidraiz, int pidservidor, int *pidhijos, int numhijos);
int ContarLineas();
static void alarmHandler(int signo);


int main(int argc, char* argv[]){

        //Variables     
        int i, j;
        long int numero;
        long int numprimrec;
    long int nbase;
    int nrango;
    int nfin;
    time_t tstart, tend; 

        key_t key;
    int msgid;    
    int pid, pidservidor, pidraiz, parentpid, mypid, pidcalc;
    int *pidhijos;
    int intervalo, inicuenta;
    int verbosity;
    T_MESG_BUFFER message;
    char info[LONGITUD_MSG_ERR];
    FILE *fsal, *fc;
    int numhijos;
    int final = 0;
    int comprobar=0;
    // Control de entrada, después del nombre del script debe figurar el número de hijos y el parámetro verbosity
                //Abrimos los dos ficheros en w asegurandonos que no se produzcan errores y luego las volbemos a abrir pero esta vez en a.
                //Hacemos esto para asegurarnos que escriba ficheros en blanco sin el riesgo de que se junte con otro fichero.   
        fsal = fopen(NOMBRE_FICH, "w");
        fc = fopen(NOMBRE_FICH_CUENTA, "w+");

        if(fsal == NULL){
                printf("Error en la creación del fichero primo.txt\n");
                return -1;
        }
    
        if(fc == NULL){
                printf("Error en la creación del fichero cuentaprimos.txt\n");
                return -1;
        }

        fsal = fopen(NOMBRE_FICH, "a");
        fc = fopen(NOMBRE_FICH_CUENTA, "a");

        if(fsal == NULL){
                printf("Error en la creación del fichero primo.txt\n");
                return -1;
        }
    
        if(fc == NULL){
                printf("Error en la creación del fichero cuentaprimos.txt\n");
                return -1;
        }

        //Establezemos los valores que escribimos en sus respectivas variables.
        numhijos=strtol(argv[1], NULL, 10);
        verbosity=strtol(argv[2], NULL, 10);

    pid = fork();       //Creación del SERVER
    
    if(pid == 0){       //Rama del hijo de RAIZ (SERVER)
                pid = getpid();
                pidservidor = pid;
                mypid = pidservidor;       

                //Petición de clave para crear la cola
                if((key = ftok("/tmp", 'C')) == -1){
                        perror("Fallo al pedir ftok");
                        exit(1);
                }

                printf("Server: System V IPC key = %u\n", key);

        //Creación de la cola de mensajería
                if((msgid = msgget(key, IPC_CREAT | 0666)) == -1){
                        perror("Fallo al crear la cola de mensajes");
                        exit(2);
                }

                printf("Server: Message queue id = %u\n", msgid );

        i = 0;

        //Creación de los procesos CALCuladores
                while(i < numhijos){
                        if(pid > 0){ //Solo el SERVER creará hijos
                                pid = fork(); 
                                if(pid == 0){   //Rama hijo
                                        parentpid = getppid();
                                        mypid = getpid();
                                } 
                        }
                        i++;  //Número de hijos creados
                }
        // AQUI VA LA LOGICA DE NEGOCIO DE CADA CALCulador. 
                if(mypid != pidservidor){
                        //Los hijos envian el mensaje de COD_ESTOY_AQUI al padre y también reciven de este mensajes con sus limites y los numeros por lo que tienen que comenzar.
                        message.mesg_type = COD_ESTOY_AQUI;
                        sprintf(message.mesg_text, "%d", mypid);
                        comprobar=msgsnd( msgid, &message, sizeof(message), IPC_NOWAIT);
                        if(comprobar==-1){
                                perror("Fallo en el CALCulator en el msgsnd() en la parte de COD_ESTOY_AQUI");
                        }
                        comprobar=msgrcv(msgid, &message, sizeof(message), COD_LIMITES, 0);
                        if(comprobar==-1){
                                perror("Fallo en el CALCulator en el msgrcv() en la parte de COD_ESTOY_AQUI");
                        }
                        sscanf(message.mesg_text, "%ld %d", &numero, &nrango); 

                        //Este bucle se encarga de invocar a nuestra variable Comprobarsiesprimo para comprobar si el numero en cuestion es primo.
                        message.mesg_type = COD_RESULTADOS;
                        for(j=0; j<nrango; j++){ 
                                if(Comprobarsiesprimo(numero)==1){
                                        sprintf(message.mesg_text, "%d %ld", mypid, numero);
                                        comprobar=msgsnd(msgid, &message, sizeof(message), IPC_NOWAIT);
                                        if(comprobar==-1){
                                                perror("Fallo en el CALCulator en el msgsnd() en la parte de COD_RESULTADOS");
                                        }

                                }
                                numero++;
                        }
                        //Se envia el mensaje de que el programa CALCulador esta finalizando.
                        message.mesg_type = COD_FIN;
                        comprobar=msgsnd(msgid, &message, sizeof(message), IPC_NOWAIT);
                        if(comprobar==-1){
                                perror("Fallo en el CALCulator en el msgsnd() en la parte de COD_FIN");
                        }
                        //Termina de finalizar.
                        exit(0);
                }
                
                //SERVER

                else{

                        //Pide memoria dinámica para crear la lista de pids de los hijos CALCuladores
                        pidhijos = malloc(sizeof(int) * numhijos);

                        //Recepción de los mensajes COD_ESTOY_AQUI de los hijos
                        for(i=0; i<numhijos; i++){
                                comprobar=msgrcv(msgid, &message, sizeof(message), COD_ESTOY_AQUI, 0);
                                if(comprobar==-1){
                                        perror("Fallo en el SERVER en el msgrcv() en la parte de recopilacion de los mensajes COD_ESTOY_AQUI");
                                }
                                sscanf(message.mesg_text, "%d", &pid); // Tendrás que guardar esa pid 
                                pidhijos[i] = pid;
                        }
                        //Llamamos a la funcion Imprimirjerarquiaproc para imprimir la jerarquia de procesos por pantalla.
                        Imprimirjerarquiaproc(getppid(), pidservidor, pidhijos, numhijos);
                        //Aquí, el programa SERVER se encarga de enviar por mensaje a los procesos CALCulador en que numero en el que deben empezar y el rango que deven recorrer.
                        message.mesg_type = COD_LIMITES;
                        for(j=0; j<numhijos; j++){
                                sprintf(message.mesg_text, "%ld %d", (long)(j*RANGO/numhijos)+BASE, RANGO/numhijos); 
                                comprobar=msgsnd(msgid, &message, sizeof(message), IPC_NOWAIT);
                                if(comprobar==-1){
                                        perror("Fallo en el SERVER en el msgsnd() en la parte de COD_LIMITES");
                                }
                        }
                        //Aquí es donde comienza el temporizador.
                        tstart = time(NULL);
                        //Este bucle se encarga de recivir los numeros primos de los CALCulador y rellena los dos ficheros con su respectiva información.
                        while(final < numhijos){
                                comprobar=msgrcv(msgid, &message, sizeof(message), COD_LIMITES, MSG_EXCEPT);
                                if(comprobar==-1){
                                        perror("Fallo en el SERVER en el msgrcv() en la parte de recivir los numeros primos");
                                }
                                sscanf(message.mesg_text, "%d %ld", &pidcalc, &numprimrec);
                                if(message.mesg_type == COD_RESULTADOS){
                                        if(verbosity > 0){
                                                 printf("El hijo numero %d encontró el primo %ld\n", pidcalc, numprimrec);
                                        }
                                        fprintf(fsal, "%ld\n", numprimrec);
                                        fclose(fsal);
                                        fsal = fopen(NOMBRE_FICH, "a");
                                        intervalo++;
                                        if(intervalo % 5 == 0){
                                                fprintf(fc, "%d\n", intervalo);
                                                fclose(fc);
                                                fc = fopen(NOMBRE_FICH_CUENTA, "a");
                                        }
                                }
                                else if(message.mesg_type == COD_FIN){
                                         final++;
                                }
                                else{
                                        printf("El SERVER recibió un mensaje de tipo %ld\n", message.mesg_type);        
                                }
                        }
                        //Termina el temporizador y printa el resultado por pantalla.
                        tend = time(NULL);
                        printf("\n%.2f segundos es el tiempo que ha tardado\n", difftime(tend, tstart));

                        //Borrado de la cola de mensajeria, muy importante. No olvides cerrar los ficheros
                        comprobar=msgctl(msgid, IPC_RMID, NULL);
                        if(comprobar==-1){
                                perror("Fallo en el SERVER en el msgctl() en la parte de borrado de la cola");
                        }
                        else {
                                printf("\nSe borró la cola de mensajeria\n");
                        }
                        fclose(fsal);
                        fclose(fc);
                        //Borramos la memora dinámica y terminamos el programa SERVER
                        free(pidhijos);

                        exit(0);
                }
        }
        //Rama de RAIZ, proceso primigenio
        else{

                pidraiz = getpid();
                alarm(INTERVALO_TIMER);
                signal(SIGALRM, alarmHandler);

                //Espera al final de SERVER
                wait(NULL);
                printf("\nEl numero total de primos calculados es %d\n", ContarLineas());
        }

        fclose(fsal);
        fclose(fc);

        //El final de todo
        return 0;
}
