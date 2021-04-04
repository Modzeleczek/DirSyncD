#include "Anna.h"
#include "Lukasz.h"
#include "Mariusz.h"

#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <signal.h>

static unsigned long long THRESHOLD; // static - zmienna globalna widoczna tylko w tym pliku; extern - zmienna globalna widoczna we wszystkich plikach

struct element
{
    element *next;
    struct dirent *entry; // element katalogu
};

struct list
{
    element *first, *last;
    unsigned int count;
};

int parseParameters(int argc, char **argv, char **source, char **destination, unsigned int *interval, char *recursive)
{
    // parsujemy dodatkowe opcje i argumenty programu
    if(argc <= 1)
        return -1;
    *interval = 5 * 60; // domyślny czas spania: 5*60 s = 5 min
    *recursive = 0; // domyślnie brak rekurencyjnego synchronizowania katalogów
    THRESHOLD = ULLONG_MAX; // domyślnie próg dużego pliku wynosi 2^64 - 1 bajtów; jeżeli podamy opcję -t 4096, to pliki o rozmiarze >= 4096 B będą czytane mmapem i zapisywane w folderze docelowym writem
    int option;
    // umieszczamy ':' na początku __shortopts, aby program mógł rozróżniać między '?' (nieznanym argumentem) i ':' (brakiem podania wartości dla opcji)
    while((option = getopt(argc, argv, ":Ri:t:")) != -1)
    {
        switch(option)
        {
            case 'R':
                *recursive = (char)1; // rekurencyjna synchronizacja katalogów
                break;
            case 'i':
                if(sscanf(optarg, "%u", interval) < 1) // ciąg znaków optarg jest czasem spania w sekundach; zamieniamy ciąg znaków na unsigned int; jeżeli sscanf nie wypełnił poprawnie zmiennej interval, to wartość przekazana do programu ma niepoprawny format 
                    return -2;
                break;
            case 't':
                if(sscanf(optarg, "%llu", &THRESHOLD) < 1) // ciąg znaków optarg jest progiem dużego pliku; zamieniamy ciąg znaków na unsigned long long; jeżeli sscanf nie wypełnił poprawnie zmiennej threshold, to wartość przekazana do programu ma niepoprawny format
                    return -3;
                break;
            case ':':
                printf("opcja wymaga podania wartosci\n");
                return -4;
                break;
            case '?':
                printf("nieznana opcja: %c\n", optopt);
                return -5;
                break;
            default:
                printf("blad");
                return -6;
                break;
        }
    }
    int remainingArguments = argc - optind; // wyznaczamy liczbę argumentów, które nie są opcjami
    if(remainingArguments != 2) // jeżeli nie mamy dokładnie dwóch argumentów (ściezki źródłowej i docelowej), to kończymy
        return -7;
    // optind - indeks pierwszego argumentu niesparsowanego przez getopt
    *source = argv[optind];
    *destination = argv[optind + 1];
    return 0;
}

int directoryValid(const char *path)
{
    DIR *d = opendir(path); // otwieramy katalog
    if(d == NULL) // jeżeli błąd podczas otwierania (m. in. kiedy katalog nie istnieje)
        return -1;
    if(closedir(d) == -1) // zamykamy katalog; jeżeli błąd podczas zamykania
        return -2;
    return 0; // katalog istnieje i operacje na nim nie powodują błędów
}

void handler(int signo) // funkcja obsługi sygnału SIGUSR1; nie musi nic robić, bo służy tylko do przerwania spania - powrót procesu demona ze stanu oczekiwania do stanu gotowego
{ }

// Love R. - "Linux. Programowanie systemowe." strona 177
// tworzymy proces potomny, kończymy proces rodzicielski (uruchamiacz demona), przekształcamy proces potomny w demona
void startDaemon(char *source, char *destination, unsigned int interval, char recursive)
{
    pid_t pid = fork(); // tworzymy proces potomny
    if(pid == -1) // błąd wywołania fork jeszcze w procesie rodzicielskim; nie powstał proces potomny
    {
        perror("fork");
        exit(-1); // zamykamy proces rodzicielski ze statusem -1 (błąd)
    }
    else if(pid > 0) // w procesie rozdzicielskim zmienna pid ma wartość równą ID (PID) utworzonego procesu potomnego
    {
        printf("PID procesu potomnego: %i\n", pid); // wypisujemy PID procesu potomnego - przyszłego demona
        exit(0); // zamykamy proces rodzicielski ze statusem 0 (brak błędów)
    }
    // poniższy kod wykonuje się w procesie potomnym, bo w nim pid == 0; przekształcamy proces potomny w demona
    int ret = 0;
    char *sourceAbsolutePath = NULL, *destinationAbsolutePath = NULL;
    if((sourceAbsolutePath = realpath(source, NULL)) == NULL) // wyznaczamy ścieżkę bezwzględną katalogu źródłowego
    {
        perror("realpath; source");
        ret = -1;
    }
    else if((destinationAbsolutePath = realpath(destination, NULL)) == NULL) // wyznaczamy ścieżkę bezwzględną katalogu docelowego
    {
        perror("realpath; destination");
        ret = -2;
    }
    else if(setsid() == -1) // tworzymy nową sesję i grupę procesów
        ret = -3; // nie wywołujemy perror, bo w procesie potomnym nie możemy wypisać błędu do terminala uruchamiającego proces rodzicielski
    else if(chdir("/") == -1) // ustawiamy katalog roboczy procesu na /
        ret = -4;
    else
    {
        int i;
        for(i = 0; i < 1023; ++i) // zamykamy stdin, stdout, stderr (deskryptory 0, 1, 2) i dalsze deskryptory - łącznie od 0 do 1023, bo domyślnie w Linuxie proces może mieć otwarte maksymalnie 1024 deskryptory
            if(close(i) == -1) // jeżeli nie uda się zamknąć któregoś deskryptora
            {
                ret = -5;
                break;
            }
    }
    if(ret >= 0)
    {
        // przeadresowujemy deskryptory 0, 1, 2 na /dev/null
        if(open("/dev/null", O_RDWR) == -1) // deskryptor 0 (stdin) wskazuje teraz na /dev/null
            ret = -6;
        else if(dup(0) == -1) // deskryptor 1 (stdout) wskazuje teraz na to samo co deskryptor 0 - na /dev/null
            ret = -7;
        else if(dup(0) == -1) // deskryptor 2 (stderr) wskazuje teraz na to samo co deskryptor 0 - na /dev/null
            ret = -8;
        // jeżeli nie wystąpił błąd, to w tym momencie proces potomny jest już demonem
        else if(signal(SIGUSR1, handler) == SIG_ERR) // błąd podczas rejestrowania funkcji obsługującej sygnał SIGUSR1
            ret = -9;
    }
    if(sourceAbsolutePath != NULL)
        free(sourceAbsolutePath);
    if(destinationAbsolutePath != NULL)
        free(destinationAbsolutePath);
    exit(ret); // zamykamy proces demona
}