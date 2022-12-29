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
