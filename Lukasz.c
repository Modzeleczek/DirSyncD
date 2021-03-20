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

int parseParameters(int argc, char **argv, char **source, char **destination, unsigned int *interval, char *recursive, unsigned long long *threshold)
{
    // parsujemy dodatkowe opcje i argumenty programu
    if(argc <= 1)
        return -1;
    *interval = 5 * 60; // domyślny czas spania: 5*60 s = 5 min
    *recursive = 0; // domyślnie brak rekurencyjnego synchronizowania katalogów
    *threshold = ULLONG_MAX; // domyślnie próg dużego pliku wynosi 2^64 - 1 bajtów; jeżeli podamy opcję -t 4096, to pliki o rozmiarze >= 4096 B będą czytane mmapem i zapisywane w folderze docelowym writem

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
                if(sscanf(optarg, "%llu", threshold) < 1) // ciąg znaków optarg jest progiem dużego pliku; zamieniamy ciąg znaków na unsigned long long; jeżeli sscanf nie wypełnił poprawnie zmiennej threshold, to wartość przekazana do programu ma niepoprawny format
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

void printUsage()
{
    printf("sposob uzycia: DirSyncD [-i <czas_spania>] [-R] [-t <prog_duzego_pliku>] sciezka_zrodlowa sciezka_docelowa\n");
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

// Love R. - "Linux. Programowanie systemowe." strona 177
// tworzymy proces potomny, kończymy proces rodzicielski (uruchamiacz demona), przekształcamy proces potomny w demona
void startDaemon(char *source, char *destination, unsigned int interval, char recursive, unsigned long long threshold)
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
    if(setsid() == -1) // tworzymy nową sesję i grupę procesów
        exit(-2); // zamykamy proces potomny ze statusem -2 (błąd); nie wywołujemy perror, bo w procesie potomnym nie możemy wypisać błędu do terminala uruchamiającego proces rodzicielski
    if(chdir("/") == -1) // ustawiamy katalog roboczy na /
        exit(-3); // zamykamy proces potomny ze statusem -3 (błąd)
    int i;
    for(i = 0; i < 1023; ++i) // zamykamy stdin, stdout, stderr (deskryptory 0, 1, 2) i dalsze deskryptory - łącznie od 0 do 1023, bo domyślnie w Linuxie proces może mieć otwarte maksymalnie 1024 deskryptory
        close(i);
    // przeadresowujemy deskryptory 0, 1, 2 na /dev/null
    open("/dev/null", O_RDWR); // deskryptor 0 (stdin) wskazuje teraz na /dev/null
    dup(0); // deskryptor 1 (stdout) wskazuje teraz na to samo co deskryptor 0 - na /dev/null
    dup(0); // deskryptor 2 (stderr) wskazuje teraz na to samo co deskryptor 0 - na /dev/null

    // w tym momencie proces potomny jest już demonem
}