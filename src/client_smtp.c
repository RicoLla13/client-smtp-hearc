#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

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
} smtp_state;

int main(int argc, char **argv) {
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

