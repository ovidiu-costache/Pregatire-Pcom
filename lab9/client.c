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

#define IP_SERVER "3.248.203.233"
#define PORT_SERVER 8080

int main(int argc, char *argv[])
{
    char *message;
    char *response;
    int sockfd;

    // TODO: Take a look into requests.c (there are some TODOs also)
    sockfd = open_connection(IP_SERVER, PORT_SERVER, AF_INET, SOCK_STREAM, 0);

    // TODO Ex 1.1: GET dummy from main server
    message = compute_get_request(IP_SERVER, "/api/v1/dummy", NULL, NULL, 0);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);

    printf("--- RASPUNS DUMMY GET ---\n%s\n\n", response);
    
    free(message); 
    free(response);
    close_connection(sockfd);

    // TODO Ex 2: Login into main server
    sockfd = open_connection(IP_SERVER, PORT_SERVER, AF_INET, SOCK_STREAM, 0);
    
    // Pregatim datele de login
    char *login_data[2];
    login_data[0] = "username=student";
    login_data[1] = "password=student";

    message = compute_post_request(IP_SERVER, "/api/v1/auth/login", "application/x-www-form-urlencoded", login_data, 2, NULL, 0);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);
    
    printf("--- RASPUNS LOGIN POST ---\n%s\n\n", response);

    // -------------------------------------------------------
    // MAGIA DE PARSARE: Cum furăm Cookie-ul din răspuns
    // -------------------------------------------------------
    char *login_cookie = NULL;
    
    // Cautam in textul raspunsului cuvantul "Set-Cookie: "
    char *cookie_start = strstr(response, "Set-Cookie: ");
    if (cookie_start != NULL) {
        // Sarim peste lungimea cuvantului "Set-Cookie: " (12 caractere)
        cookie_start += 12; 
        
        // Cautam primul punct si virgula care marcheaza finalul cookie-ului
        char *cookie_end = strchr(cookie_start, ';'); 
        
        if (cookie_end != NULL) {
            // Alocam memorie exacta si il decupam
            login_cookie = calloc(cookie_end - cookie_start + 1, sizeof(char));
            strncpy(login_cookie, cookie_start, cookie_end - cookie_start);
        }
    }
    
    printf("--- AM EXTRAS COOKIE-UL: %s ---\n\n", login_cookie);

    free(message); 
    free(response);
    close_connection(sockfd);

    // =======================================================
    // Ex 3: GET weather key from main server
    // =======================================================
    sockfd = open_connection(IP_SERVER, PORT_SERVER, AF_INET, SOCK_STREAM, 0);

    char *cookies[1];
    cookies[0] = login_cookie;

    message = compute_get_request(IP_SERVER, "/api/v1/weather/key", NULL, cookies, 1);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);

    printf("--- RASPUNS WEATHER KEY ---\n%s\n\n", response);

    // Extragem cheia (INAINTE sa dam free la response!)
    char *weather_key = NULL;
    char *key_start = strstr(response, "{\"key\":\"");
    if (key_start != NULL) {
        key_start += 8; 
        char *key_end = strchr(key_start, '\"');
        if (key_end != NULL) {
            weather_key = calloc(key_end - key_start + 1, sizeof(char));
            strncpy(weather_key, key_start, key_end - key_start);
        }
    }
    printf("--- AM EXTRAS CHEIA DE VREME: %s ---\n\n", weather_key);

    // ACUM putem elibera curat memoria pentru Task 3
    free(message);
    free(response);
    close_connection(sockfd);

    // =======================================================
    // Ex 4: GET weather data from OpenWeather API
    // =======================================================
    struct hostent *hw = gethostbyname("api.openweathermap.org");
    char *ip_weather = inet_ntoa(*(struct in_addr *)hw->h_addr);
    
    int sockfd_weather = open_connection(ip_weather, 80, AF_INET, SOCK_STREAM, 0);

    char query_params[200];
    sprintf(query_params, "lat=44.4323&lon=26.1063&appid=%s", weather_key);

    message = compute_get_request("api.openweathermap.org", "/data/2.5/weather", query_params, NULL, 0);
    send_to_server(sockfd_weather, message);
    response = receive_from_server(sockfd_weather);

    printf("--- RASPUNS VREMEA (OPENWEATHER) ---\n%s\n\n", response);

    char *weather_data_json = strchr(response, '{');

    free(message);
    close_connection(sockfd_weather);

    // =======================================================
    // Ex 5: POST weather data for verification to main server
    // =======================================================
    sockfd = open_connection(IP_SERVER, PORT_SERVER, AF_INET, SOCK_STREAM, 0);

    // Pregatim payload-ul: JSON-ul salvat la Ex 4
    char *weather_payload[1];
    weather_payload[0] = weather_data_json; 

    char verify_url[100];
    // Folosim aceleasi coordonate pe care le-am cerut la Task 4
    sprintf(verify_url, "/api/v1/weather/44.4323/26.1063");

    // Facem POST cu "application/json" si trimitem si cookie-ul de logare!
    cookies[0] = login_cookie; // Refolosim vectorul deja declarat la Ex 3

    message = compute_post_request(IP_SERVER, verify_url, "application/json", weather_payload, 1, cookies, 1);
    send_to_server(sockfd, message);
    
    char *response_ex5 = receive_from_server(sockfd);
    printf("--- RASPUNS VERIFICARE VREME ---\n%s\n\n", response_ex5);

    free(message);
    free(response_ex5);
    close_connection(sockfd);

    // =======================================================
    // Ex 6: Logout from main server
    // =======================================================
    sockfd = open_connection(IP_SERVER, PORT_SERVER, AF_INET, SOCK_STREAM, 0);

    // Facem un simplu GET la ruta de logout, demonstrand cu Cookie-ul cine suntem
    message = compute_get_request(IP_SERVER, "/api/v1/auth/logout", NULL, cookies, 1);
    send_to_server(sockfd, message);
    
    char *response_ex6 = receive_from_server(sockfd);
    printf("--- RASPUNS LOGOUT ---\n%s\n\n", response_ex6);

    free(message);
    free(response_ex6);
    close_connection(sockfd);

    return 0;
}
