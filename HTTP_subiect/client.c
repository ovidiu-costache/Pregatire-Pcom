#include <stdio.h>      /* printf, sprintf */
#include <stdlib.h>     /* exit, atoi, malloc, free */
#include <unistd.h>     /* read, write, close */
#include <string.h>     /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h>      /* struct hostent, gethostbyname */
#include <arpa/inet.h>
#include "helpers.h"
#include "requests.h"

int main(int argc, char *argv[])
{
    int sockfd;
    char *message;
    char *response;

    // Pasul 0: Deschidem conexiunea TCP catre serverul cerut in enunt
   sockfd = open_connection("3.248.203.233", 8080, AF_INET, SOCK_STREAM, 0);

    // ========================================================
    // TODO 1: Construiți și trimiteți un request GET la /api/v1/dummy
    // ========================================================
    // Apelam functia lor ca sa ne genereze textul. Nu avem parametri (?) si nici cookies, deci punem NULL.
    message = compute_get_request("3.248.203.233", "/api/v1/dummy", NULL, NULL, 0);
    
    // Trimitem textul prin socket
    send_to_server(sockfd, message);


    // ========================================================
    // TODO 2: Citiți răspunsul primit de la server
    // ========================================================
    // Asteptam sa ne raspunda serverul si salvam tot textul primit
    response = receive_from_server(sockfd);


    // ========================================================
    // TODO 3: Afișați pe ecran valoarea câmpului 'message'
    // ========================================================
    /* Serverul ne va raspunde la final cu un JSON de forma: 
       {"message":"Salut, ai reusit!"} 
       Noi trebuie sa extragem doar: Salut, ai reusit!
    */

    char *valoare_extrasa = NULL;
    
    // Cautam in tot raspunsul unde incepe textul "message":"
    char *start = strstr(response, "\"message\":\"");
    
    if (start != NULL) {
        // start pointeaza acum fix pe prima ghilimea din "message":"
        // Noi vrem sa sarim peste aceste 11 caractere ca sa ajungem la valoarea efectiva
        start += 11; 
        
        // Acum cautam unde se termina valoarea (prima ghilimea de dupa text)
        char *end = strchr(start, '\"');
        
        if (end != NULL) {
            // Calculam cate litere are valoarea noastra
            int lungime = end - start;
            
            // Alocam memorie exacta pentru ea (+1 pentru terminatorul de sir \0)
            valoare_extrasa = calloc(lungime + 1, sizeof(char));
            
            // Copiem fix textul gasit
            strncpy(valoare_extrasa, start, lungime);
        }
    }

    // Printam rezultatul cerut
    if (valoare_extrasa != NULL) {
        printf("Valoarea ceruta este: %s\n", valoare_extrasa);
        free(valoare_extrasa); // facem curat
    } else {
        printf("Nu am gasit campul message in raspuns.\n");
    }

    // Eliberam restul memoriei si inchidem usa
    free(message);
    free(response);
    close_connection(sockfd);

    return 0;
}
