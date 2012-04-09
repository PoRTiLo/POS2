#define _POSIX_C_SOURCE 199506/*199500L*/
#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED 1 /* XPG 4.2 - needed for WCOREDUMP() */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/types.h>
       #include <unistd.h>



#define ASCII0 48   /* v ASCII tabulce hodnota cisla '0' */

volatile int ticket;
volatile int ticketAct;
struct Spthread *threads;                                   /* pole vlaken */
int countSynchr = 0;                                        /* Pocet pruchodu kritickou sekci */
int countThreads = 0;                                       /* Pocet vlaken */
struct Spthread {                                           /* struktura obsahuji info o vlaknech */
   int volatile ticket;                                     /* prideleny vstupni listekj do kriticke sekce */
   pthread_t id;                                            /* id vlakna */
};
pthread_mutex_t var_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int isNumber(char argv[]);
int randomWait(int threadID);
int waitMoment();
int checkArg(int argc, char *argv[]);
void *threadFunc(void *thread_id);
int getticket(void);
void await(int aenter);
void advance(void);


int main(int argc, char *argv[]) {

   intptr_t i;
   int result;                                              /* Promenna obsahujici vysledky funkci */
   ticket = -1;
   ticketAct = 0;
   if(checkArg(argc, argv) == -1 ) {                        /* Kontrola vstupnich argumentu */
      return -1;                                            /* Nastala chyba */
   }

   /* Tvorba vlaken */
   if( (threads = (struct Spthread*) malloc(sizeof(struct Spthread) * countThreads)) == NULL) { /* Dynamicka alokace pole vlaken*/
      printf("CHYBA: Nepovedla se dynamicka alokace pole sdruzujici informace o vlaknech.\n");
      return -1;
   }

   for(i = 0; i < countThreads; i++) {  /* Vytvoreni vlaken */
      threads[i].ticket = -1;                                                                   /* Inicializace listku */
      result = pthread_create(&threads[i].id, NULL, threadFunc, (void *) i);
      if(result == EAGAIN) {                                                                    /* Tvorba vlaken docasne nedostupna, zkusim znovu */
         waitMoment();                                                                          /* Pockam chvili */
         i--;
      }
      else if(result) {
         printf("CHYBA: Nepovedlo se vytvorit vlakno. Kod chyby: %d\n", result);
         free(threads);
         return -1;
      }
     /* else {
          hlavni vlakno skonci posledni 
         if((result = pthread_join(threads[i].id, NULL)) != 0 ) {                                   cekani na dokonceni vlakna 
            printf("CHYBA: Nepovedlo se nasatvit cekani na konec tohoto vlakna. Kod chyby: %d\n", result);
            free(threads);
            return -1;
         }
      }*/
   }

   for(i = 0; i < countThreads; i++) {  /* Vytvoreni vlaken */
      if((result = pthread_join(threads[i].id, NULL)) != 0 ) {                                  /* cekani na dokonceni vlakna */
         printf("CHYBA: Nepovedlo se nasatvit cekani na konec tohoto vlakna. Kod chyby: %d\n", result);
         free(threads);
         return -1;
      }
   }
   printf("KONEC\n");
   free(threads);                                     /* uvolneni pameti */
   /*pthread_exit(NULL); */                               /* ukonceni vlakna */
   return 0;
}

void *threadFunc(void *threadId) {
   int localTic;
   while ((localTic = getticket()) < countSynchr) { /* P¿id¿lení lístku */
      threads[(intptr_t)threadId].ticket = localTic;
      randomWait((intptr_t)threadId);             /* Náhodné ¿ekání v intervalu <0,0 s, 0,5 s> */
      await(threads[(intptr_t)threadId].ticket);                              /* Vstup do KS */
      printf("%d\t(%ld)\n", threads[(intptr_t)threadId].ticket, (intptr_t)threadId+1); /* fflush(stdout); */
      advance();                                  /* Výstup z KS */
      randomWait((intptr_t)threadId);             /* Náhodné ¿ekání v intervalu <0,0 s, 0,5 s> */
   }

   return (NULL);
}

int getticket(void) {
   pthread_mutex_lock(&var_mutex);           /* získání výhradního p¿ístupu */
   ticket++;                                 /* operace se sdílenou prom¿nnou */
   pthread_mutex_unlock(&var_mutex);         /* uvoln¿ní p¿ístupu */
   return ticket;
}

void await(int aenter) {
   pthread_mutex_lock(&var_mutex);           /* získání výhradního p¿ístupu */
   while(aenter <= countSynchr) {
 /*     printf("jsme ve wjile:%d:%d\n", aenter, ticketAct);*/
      if(aenter == ticketAct) {
/*         printf("mame stejne hodnoty\n");*/
         return;
      }
/*      printf("spime");*/
      pthread_cond_wait(&cond, &var_mutex);
   }

}

void advance(void) {
   pthread_mutex_unlock(&var_mutex);
   ticketAct++;
/*   printf("ukoncuji kritickou sekci:%d\n", ticketAct);*/
   if(pthread_cond_broadcast(&cond) != 0) {
      printf("CHYBA: u Brodcastu.\n");
   }
}
int isNumber(char argv[]) {
   int i;
   int num = 0;
   int m = 1;
   for(i = 0; argv[i] != '\0'; i++) {
      ;
   }
   for(i--; i >= 0; i--) {
      if(argv[i] - ASCII0 >= 0 && argv[i] - ASCII0 <= 9) {
         num = num + ((argv[i] - ASCII0) *  m);
         m = m * 10;
      }
      else {
         return -1;
      }
   }
   return num;
}

int waitMoment() {
   struct timespec rem;
   struct timespec req;
   req.tv_sec = 0;
   req.tv_nsec = 5000000;
   if(nanosleep(&req, &rem) != 0) {
      /*printf("VAROVANI: Cekani se nezdarilo!\n");*/
      return -1;
   }
   return 1;
}
int randomWait(int threadId) {
   struct timespec rem;
   struct timespec req;
   unsigned int seed = (threadId+1) * getpid();                   /* Inicializace SEED - podle id vlakna i PID procesu */
   req.tv_sec = 0;
   req.tv_nsec = rand_r(&seed) % 500000000;                       /* Pseudogenerator, nastaveni podle man stranky */
/*   printf("%ld\n", req.tv_nsec);*/
   if(nanosleep(&req, &rem) != 0) {
      /*printf("VAROVANI: Cekani se nezdarilo");*/
      return -1;
   }
   return 1;
}
int checkArg(int argc, char *argv[]) {
   if(argc == 1 || argc == 2) {
      /* Tisk napovedy */
      /* TODO: HELP*/
      printf("0 nebo 1\n");
      return -1;
   }
   else if(argc == 3) {
      if( (countThreads = isNumber(argv[1])) == -1) {  /* Neni cislo, ukoncim */
         printf("CHYBA: Pocet vlaken neni zadan jako cislo.\n");
         return -1;
      }
      if( (countSynchr = isNumber(argv[2])) == -1) {  /* Neni cislo, ukoncim */
         printf("CHYBA: Pocet pruchodu kritickou sekci neni zadan jako cislo.\n");
         return -1;
      }
   }
   else {/* Chyba spatny pocet argumentu*/
      printf("CHYBA: Spatny pocet argumentu.\n");
      return -1;
   }
   return 0;
}
