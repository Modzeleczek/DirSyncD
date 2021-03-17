#include "Anna.h"
#include "Lukasz.h"
#include "Mariusz.h"

#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>

int parseParameters(int argc, char **argv, char **source, char **destination, unsigned int *interval, char *recursive)
{
    // parsujemy dodatkowe opcje i argumenty programu
    if(argc <= 1)
        return -1;
    *interval = 5 * 60; // domyślny czas spania: 5*60 s = 5 min
    *recursive = 0; // domyślnie brak rekurencyjnego synchronizowania katalogów

    int option;
    // umieszczamy ':' na początku __shortopts, aby program mógł rozróżniać między '?' (nieznanym argumentem) i ':' (brakiem podania wartości dla opcji)
    while((option = getopt(argc, argv, ":Ri:")) != -1)
    {
        switch(option)
        {
            case 'R':
                *recursive = (char)1; // rekurencyjna synchronizacja katalogów
                break;
            case 'i':
                sscanf(optarg, "%u", interval); // ciąg znaków optarg jest czasem spania w sekundach; zamieniamy ciąg znaków na unsigned int
                break;
            case ':':
                printf("opcja wymaga podania wartosci\n");
                return -2;
                break;
            case '?':
                printf("nieznana opcja: %c\n", optopt);
                return -3;
                break;
            default:
                printf("blad");
                return -4;
                break;
        }
    }
    
    int remainingArguments = argc - optind; // wyznaczamy liczbę argumentów, które nie są opcjami
    if(remainingArguments != 2) // jeżeli nie mamy dokładnie dwóch argumentów (ściezki źródłowej i docelowej), to kończymy
        return -5;
    // optind - indeks pierwszego argumentu niesparsowanego przez getopt
    *source = argv[optind];
    *destination = argv[optind + 1];
    return 0;
}

void printUsage()
{
    printf("sposob uzycia: DirSyncD [-i <czas_spania>] [-R] sciezka_zrodlowa sciezka_docelowa\n");
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