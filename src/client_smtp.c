#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sysexits.h>

#define DEFAULT_PORT "25"
#define MAX_ATTEMPTS 5
#define WAIT_TIME 5

static FILE *tcp_connect(const char *hostname, const char *port);

// State machine for the SMTP client
typedef enum {
    CONNECTION,
    SMTP_HELO,
    SMTP_MAIL,
    SMTP_RCPT,
    SMTP_DATA,
    SMTP_SEND,
    SMTP_QUIT,
    CLOSE,
} smtp_state_t;

int main(int argc, char **argv) {
    /* 
     * Command line arguments structure:
     * 1: sender email address
     * 2: mail subject
     * 3: message body file name
     * 4: DNS domain or IP address of the mail server
     * 5: reciever mail address
     * 6: *optional* port number
     */

    // Check for correct number of arguments
    if(argc < 6 || argc > 7) {
        fprintf(stderr, "[!] Usage: %s <sender email> <subject> <message file> <mail server> <reciever email> [<port>]\n", argv[0]);
        exit(EX_USAGE);
    }

    // Copy each argument to a variable for readability
    char *sender = argv[1];
    char *subject = argv[2];
    char *message_file = argv[3];
    char *mail_server = argv[4];
    char *reciever = argv[5];
    char *port = (argc == 7) ? argv[6] : DEFAULT_PORT;

    // Check if the arguments respect the RFC 3696 and RFC 1035 norms
    // Check if the sender is a valid email address
    if(strlen(sender) < 7 || strlen(sender) > 320) {
        fprintf(stderr, "[!] Invalid sender email address: %s\n", sender);
        exit(EX_DATAERR);
    }
    // Check if the subject is a certain length
    if(strlen(subject) > 90) {
        fprintf(stderr, "[!] Subject is too long\n");
        exit(EX_DATAERR);
    } else if(strlen(subject) == 0) {
        fprintf(stderr, "[!] Subject is empty\n");
        exit(EX_DATAERR);
    }
    // Check file name length
    if(strlen(message_file) > 256) {
        fprintf(stderr, "[!] Message file name is too long\n");
        exit(EX_DATAERR);
    } else if(strlen(message_file) == 0) {
        fprintf(stderr, "[!] Message file name is empty\n");
        exit(EX_DATAERR);
    }
    // Check if the mail server is a valid domain name or IP address
    if(strlen(mail_server) < 7 || strlen(mail_server) > 255) {
        fprintf(stderr, "[!] Invalid mail server: %s\n", mail_server);
        exit(EX_DATAERR);
    }
    // Check if the reciever is a valid email address
    if(strlen(reciever) < 7 || strlen(reciever) > 320) {
        fprintf(stderr, "[!] Invalid reciever email address: %s\n", reciever);
        exit(EX_DATAERR);
    }
    // Check if the port number is valid
    if(strlen(port) > 4) {
        fprintf(stderr, "[!] Invalid port number: %s\n", port);
        exit(EX_DATAERR);
    } 

    FILE* mail_body = fopen(message_file, "r");
    if(mail_body == NULL) {
        fprintf(stderr, "[!] Could not open message file: %s\n", message_file);
        exit(EX_NOINPUT);
    }

    FILE* tcp_socket;
    char buffer[200];
    smtp_state_t state = CONNECTION;

    int attempt = 0;
    while(1) {
        printf("[*] State: %d\n", state);

        switch(state) {
            case CONNECTION:
                tcp_socket = tcp_connect(mail_server, port);
                if(tcp_socket == NULL) {
                    fprintf(stderr, "[!] Could not connect to %s:%s\n", mail_server, port);
                    state = CLOSE;
                    continue;
                }
                fgets(buffer, sizeof(buffer), tcp_socket);
                break;
            case SMTP_HELO:
                printf("[*] Sending HELO\n");
                fprintf(tcp_socket, "HELO %s\n", mail_server);
                fgets(buffer, sizeof(buffer), tcp_socket);
                break;
            case SMTP_MAIL:
                printf("[*] Sending MAIL FROM\n");
                fprintf(tcp_socket, "MAIL FROM: <%s>\r\n", sender);
                fgets(buffer, sizeof(buffer), tcp_socket);
                break;
            case SMTP_RCPT:
                printf("[*] Sending RCPT TO\n");
                fprintf(tcp_socket, "RCPT TO: <%s>\r\n", reciever);
                fgets(buffer, sizeof(buffer), tcp_socket);
                break;
            case SMTP_DATA:
                printf("[*] Sending DATA\n");
                fprintf(tcp_socket, "DATA\n");
                fgets(buffer, sizeof(buffer), tcp_socket);
                break;
            case SMTP_SEND:
                printf("[*] Sending email\n");
                fprintf(tcp_socket, "From: %s\n", sender);
                fprintf(tcp_socket, "To: %s\n", reciever);
                fprintf(tcp_socket, "Subject: %s\n", subject);
                fprintf(tcp_socket, "\n");
                while(fgets(buffer, sizeof(buffer), mail_body) != NULL)
                    fprintf(tcp_socket, "%s", buffer);
                fprintf(tcp_socket, ".\n");
                fgets(buffer, sizeof(buffer), tcp_socket);
                break;
            case SMTP_QUIT:
                printf("[*] Sending QUIT\n");
                fprintf(tcp_socket, "QUIT\n");
                fgets(buffer, sizeof(buffer), tcp_socket);
                break;
            case CLOSE:
                printf("[*] Closing connection\n");
                shutdown(fileno(tcp_socket), SHUT_RDWR);
                printf("[*] Connection closed\n");
                if(attempt <= MAX_ATTEMPTS && buffer[0] == '4') {
                    printf("[*] Retrying in %d seconds\n", WAIT_TIME);
                    sleep(WAIT_TIME);
                    attempt++;
                    state = CONNECTION;
                    continue;
                }
                return EXIT_SUCCESS;
            default:
                fprintf(stderr, "[!] Invalid state\n");
                return EX_SOFTWARE;
        }

        printf("[*] Server response: %s\n", buffer);
        if(buffer[0] != '2' && state != SMTP_DATA) {
            state = CLOSE;
            continue;
        }
        if(buffer[0] != '3' && state == SMTP_DATA) {
            state = CLOSE;
            continue;
        }

        state++;
    }

    return EXIT_SUCCESS;
}

// #define LOW_LEVEL_DEBUG
static FILE *tcp_connect(const char *hostname, const char *port) {
   FILE *f = NULL;

   /* STUDENT_START */
   int s;
   struct addrinfo hints;
   struct addrinfo *result, *rp;

   hints.ai_family = AF_UNSPEC; /* IPv4 or v6 */
   hints.ai_socktype = SOCK_STREAM; /* TCP */
   hints.ai_flags = 0;
   hints.ai_protocol = 0; /* any protocol */

   if ((s = getaddrinfo(hostname, port, &hints, &result))) {
      fprintf(stderr, "getaddrinfo(): failed: %s.\n", gai_strerror(s));
   }
   else {
      /*  getaddrinfo() retourne une liste de structures d'adresses.
       *  Nous essayerons chaque adresse jusqu'a connect(2)ion
       */
      for (rp = result; rp != NULL; rp = rp->ai_next) {
         char ipname[INET6_ADDRSTRLEN]; /* len(addrv6) > len(addrv4) */
         char servicename[6]; /* "65535\0" */

         /* creation du socket */
         if ((s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol))
             == -1) {
            perror("socket()");
            continue; /* boucle suivante */
         }

#ifdef LOW_LEVEL_DEBUG
         /* ceci est une des raisons pourquoi l'acces aux structures bas
          * niveau est decourage: suivant la plateforme (p.ex. little-endian)
          * ces donnees seront deja convertie avec htons(3); c'est aussi une
          * raison pourquoi gethostbyname(3) ne devrait plus etre utilise
          * (aussi: securite et multithread)
          */
         if (rp->ai_family == AF_INET) {
	    printf("HACK: valeur du port brut: %d\n",
		   ((struct sockaddr_in *) rp->ai_addr)->sin_port);
         }
#endif /* LOW_LEVEL_DEBUG */

         if (!getnameinfo(rp->ai_addr,
                          rp->ai_addrlen,
                          ipname,
                          sizeof(ipname),
                          servicename,
                          sizeof(servicename),
                          NI_NUMERICHOST|NI_NUMERICSERV)) {
	    printf("Essai de connection vers host %s:%s ...\n",
                   ipname,
                   servicename);
         }

         if (connect(s, rp->ai_addr, rp->ai_addrlen) != -1) {
	   /* depuis maintenant, on peut utiliser les appels systemes
            * read(2), write(2), shutdown(2) and close(2)
	    * sur ce socket (descripteur de fichier UNIX) -- attention,
	    * c'est un peripherique caractere, donc write(2) peut ne pas
            * tout ecrire et read(2) peut retourner moins que demande:
	    * il faudrait boucler (voir slides), mais c'est plus simple
            * en particulier pour les protocoles NVT de promouvoir le
	    * socket en un FILE * de la libc!
	    * donc, associons le descripteur a un FILE * de la bibliotheque
            * C standard, cela simplifiera les choses!
	    */
	    if ((f = fdopen(s, "r+"))) {
               break; /* fin de la boucle */
            }
            else {
	       perror("fdopen()");
	    }
         }
         else {
            perror("connect()");
         }

         close(s); /* err. ign. */
      }

      /* rendre la structure de donnees allouee par getaddrinfo() */
      freeaddrinfo(result);

      if (f == NULL) {
         fprintf(stderr, "Could not connect.\n");
      }
   }

   /* STUDENT_END */

   return f;
}

