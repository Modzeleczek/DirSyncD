#include "Anna.h"
#include "Lukasz.h"
#include "Mariusz.h"

#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>

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
        return -3;
    // optind - indeks pierwszego argumentu niesparsowanego przez getopt
    int i;
    for(i = 0; i < remainingArguments; ++i) // iterujemy po ścieżkach, aby sprawdzić, czy katalogi istnieją; są tylko 2 ścieżki: źródłowa i docelowa
    {
        DIR *d = opendir(argv[optind + i]); // otwieramy katalog; i = 0 - katalog źródłowy; i = 1 - katalog docelowy
        if(d == NULL) // błąd podczas otwierania
        {
            perror("opendir");
            return -(5 + i);
        }
        if(closedir(d) == -1) // zamykamy katalog źródłowy
        {
            perror("closedir");
            return -(6 + i);
        }
    }
    *source = argv[optind];
    *destination = argv[optind + 1];
    return 0;
}