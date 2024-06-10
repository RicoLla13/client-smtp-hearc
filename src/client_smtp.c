#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sysexits.h>

#define DEFAULT_PORT "25"

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
    } else if(strlen(port) == 0) {
        strncpy(port, DEFAULT_PORT, 4);
    }

    return EXIT_SUCCESS;
}

#define LOW_LEVEL_DEBUG
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

