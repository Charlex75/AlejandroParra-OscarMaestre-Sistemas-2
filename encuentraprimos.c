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
 

