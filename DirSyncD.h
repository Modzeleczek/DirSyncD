#ifndef DIRSYNCD_H
#define DIRSYNCD_H

#include <dirent.h>
#include <sys/stat.h>

typedef struct element element;
/*
Element listy jednokierunkowej przechowujący wskaźnik na element katalogu i na następny element listy.
*/
struct element;
/*
Porównuje elementy listy jednokierunkowej.
odczytuje:
a - pierwszy element
b - drugi element
zwraca:
< 0, jeżeli a jest przed b w porządku leksykograficznym według nazwy elementu katalogu
0, jeżeli a i b mają równe nazwy elementu katalogu
> 0, jeżeli a jest po b w porządku leksykograficznym według nazwy elementu katalogu
*/
int cmp(element *a, element *b);

typedef struct list list;
/*
Lista jednokierunkowa. W funkcjach do niej zakładamy, że podano prawidłowy wskaźnik do listy.
*/
struct list;
/*
Inicjuje listę jednokierunkową.
zapisuje:
l - pusta lista jednokierunkowa przeznaczona do pierwszego użycia
*/
void initialize(list *l);
/*
Dodaje element katalogu na koniec listy.
odczytuje:
newEntry - element katalogu, który dodajemy na koniec listy
zapisuje:
l - lista jednokierunkowa ze wstawionym na końcu elementem zawierającym element katalogu newEntry
*/
int pushBack(list *l, struct dirent *newEntry);
/*
Czyści listę jednokierunkową.
zapisuje:
l - pusta lista jednokierunkowa przeznaczona do ponownego użycia
*/
void clear(list *l);
/* https://www.chiark.greenend.org.uk/~sgtatham/algorithms/listsort.html
 * This file is copyright 2001 Simon Tatham.
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL SIMON TATHAM BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/*
Sortuje przez scalanie listę jednokierunkową. Funkcja zawiera oryginalne komentarze autora.
zapisuje:
l - lista jednokierunkowa posortowana z wykorzystaniem funkcji cmp porównującej elementy
*/
void listMergeSort(list *l);

/*
Dopisuje napis src do napisu dst, począwszy od pozycji o numerze offset.
odczytuje:
offset - numer bajtu w ciągu dst, od którego zaczynamy wstawiać ciąg src
src - ciąg wstawiany do ciągu dst
zapisuje:
dst - ciąg, do którego wstawiamy ciąg src
*/
void stringAppend(char *dst, const size_t offset, const char *src);
/*
odczytuje:
path - ścieżka katalogu, w którym znajduje się podkatalog
pathLength - długość w bajtach ścieżki katalogu
subName - nazwa podkatalogu
zapisuje:
path - ścieżka podkatalogu
zwraca:
długość w bajtach ścieżki podkatalogu ze znakiem '/' na końcu
*/
size_t appendSubdirectoryName(char *path, const size_t pathLength, const char *subName);

/*
Analizuje opcje i argumenty przekazane do programu i zapisuje w swoich parametrach wartości gotowe do użycia.
odczytuje:
argc - liczba parametrów programu (opcji i argumentów razem)
argv - parametry programu
zapisuje:
source - ścieżka katalogu źródłowego
destination - ścieżka katalogu docelowego
interval - czas spania w sekundach
recursive - rekurecyjna synchronizacja katalogów
threshold - minimalna wielkość pliku, żeby był traktowany jako duży (zapisuje w zmiennej globalnej)
zwraca:
< 0, jeżeli wystąpił błąd
0, jeżeli nie wystąpił błąd
*/
int parseParameters(int argc, char **argv, char **source, char **destination, unsigned int *interval, char *recursive);

/*
Sprawdza, czy katalog istnieje i jest gotowy do użycia.
odczytuje:
path - ścieżka katalogu
zwraca:
-1, jeżeli wystąpił błąd podczas otwierania katalogu
-2, jeżeli wystąpił błąd podczas zamykania katalogu
0, jeżeli nie wystąpił błąd
*/
int directoryValid(const char *path);

/*
Obsługuje sygnał SIGUSR1.
odczytuje:
signo - numer obsługiwanego sygnału - zawsze SIGUSR1
*/
void sigusr1Handler(int signo);
/*
Obsługuje sygnał SIGTERM.
odczytuje:
signo - numer obsługiwanego sygnału - zawsze SIGTERM
*/
void sigtermHandler(int signo);

/*
Z procesu rodzicielskiego uruchamia proces potomny. Kończy proces rodzicielski. Przekształca proces potomny w demona. Wykonuje spanie i synchronizuje katalogi. Obsługuje sygnały.
odczytuje:
source - ścieżka katalogu źródłowego
destination - ścieżka katalogu docelowego
interval - czas spania w sekundach
recursive - rekurecyjna synchronizacja katalogów
*/
void runDaemon(char *source, char *destination, unsigned int interval, char recursive);

/*
Wypełnia listę plików katalogu dir.
odczytuje:
dir - strumień katalogu otwarty za pomocą opendir
zapisuje:
files - lista zwykłych plików znajdujących się w katalogu
zwraca:
< 0, jeżeli wystąpił błąd
0, jeżeli nie wystąpił błąd
*/
int listFiles(DIR *dir, list *files);
/*
Wypełnia listy plików i podkatalogów katalogu dir.
odczytuje:
dir - strumień katalogu otwarty za pomocą opendir
zapisuje:
files - lista zwykłych plików znajdujących się w katalogu
subdirs - lista podkatalogów znajdujących się w katalogu
zwraca:
< 0, jeżeli wystąpił błąd
0, jeżeli nie wystąpił błąd
*/
int listFilesAndDirectories(DIR *dir, list *files, list *subdirs);

/*
Tworzy pusty katalog.
odczytuje:
path - ścieżka katalogu bezwzględna lub względem aktualnego katalogu roboczego (cwd) procesu
mode - uprawnienia utworzonego katalogu
zwraca:
-1, jeżeli wystąpił błąd
0, jeżeli nie wystąpił błąd
*/
int createEmptyDirectory(const char *path, mode_t mode);
/*
Rekurencyjnie usuwa katalog.
odczytuje:
path - ścieżka katalogu bezwzględna lub względem aktualnego katalogu roboczego (cwd) procesu; musi być zakończona '/'
pathLength - długość ścieżki w bajtach
zwraca:
< 0, jeżeli wystąpił błąd krytyczny
> 0, jeżeli wystąpił błąd niekrytyczny
0, jeżeli nie wystąpił błąd
*/
int removeDirectoryRecursively(const char *path, const size_t pathLength);

/*
Kopiuje plik. Odczytuje plik źródłowy funkcją read i zapisuje plik docelowy funkcją write.
odczytuje:
srcFilePath - ścieżka pliku źródłowego bezwzględna lub względem aktualnego katalogu roboczego (cwd) procesu
dstFilePath - ścieżka pliku docelowego bezwzględna lub względem aktualnego katalogu roboczego (cwd) procesu
dstMode - uprawnienia ustawiane plikowi docelowemu
dstAccessTime - czas ostatniego dostępu ustawiany plikowi docelowemu
dstModificationTime - czas ostatniej modyfikacji ustawiany plikowi docelowemu
zwraca:
< 0, jeżeli wystąpił błąd krytyczny
> 0, jeżeli wystąpił błąd niekrytyczny
0, jeżeli nie wystąpił błąd
*/
int copySmallFile(const char *srcFilePath, const char *dstFilePath, const mode_t dstMode, const struct timespec *dstAccessTime, const struct timespec *dstModificationTime);
/*
Kopiuje plik. Odczytuje plik źródłowy z pamięci odwzorowanej funkcją mmap i zapisuje plik docelowy funkcją write.
odczytuje:
srcFilePath - ścieżka pliku źródłowego bezwzględna lub względem aktualnego katalogu roboczego (cwd) procesu
dstFilePath - ścieżka pliku docelowego bezwzględna lub względem aktualnego katalogu roboczego (cwd) procesu
fileSize - rozmiar w bajtach pliku źródłowego i docelowego po prawidłowym skopiowaniu
dstMode - uprawnienia ustawiane plikowi docelowemu
dstAccessTime - czas ostatniego dostępu ustawiany plikowi docelowemu
dstModificationTime - czas ostatniej modyfikacji ustawiany plikowi docelowemu
zwraca:
< 0, jeżeli wystąpił błąd krytyczny
> 0, jeżeli wystąpił błąd niekrytyczny
0, jeżeli nie wystąpił błąd
*/
int copyBigFile(const char *srcFilePath, const char *dstFilePath, const unsigned long long fileSize, const mode_t dstMode, const struct timespec *dstAccessTime, const struct timespec *dstModificationTime);
/*
Usuwa plik.
odczytuje:
path - ścieżka pliku bezwzględna lub względem aktualnego katalogu roboczego (cwd) procesu
zwraca:
-1, jeżeli wystąpił błąd
0, jeżeli nie wystąpił błąd
*/
int removeFile(const char *path);

/*
Wykrywa różnice i aktualizuje pliki w katalogu docelowym. Jeżeli inode (i-węzeł, czyli fizyczny plik w pamięci masowej) ma w katalogu źródłowym więcej niż 1 nazwę (dowiązanie twarde, hard link), to do katalogu docelowego każdą nazwę kopiujemy jako oddzielny inode.
odczytuje:
srcDirPath - ścieżka katalogu źródłowego bezwzględna lub względem aktualnego katalogu roboczego (cwd) procesu; musi być zakończona '/'
srcDirPathLength - długość w bajtach ścieżki katalogu źródłowego
filesSrc - uporządkowana lista plików znajdujących się w katalogu źródłowym
dstDirPath - ścieżka katalogu docelowego bezwzględna lub względem aktualnego katalogu roboczego (cwd) procesu; musi być zakończona '/'
dstDirPathLength - długość w bajtach ścieżki katalogu docelowego
filesDst - lista plików znajdujących się w katalogu docelowym uporządkowana w takim samym porządku jak filesSrc
zwraca:
< 0, jeżeli wystąpił błąd uniemożliwiający sprawdzenie wszystkich plików
> 0, jeżeli wystąpił przynajmniej 1 błąd uniemożliwiający operację na pliku
0, jeżeli nie wystąpił błąd
*/
int updateDestinationFiles(const char *srcDirPath, const size_t srcDirPathLength, list *filesSrc, const char *dstDirPath, const size_t dstDirPathLength, list *filesDst);
/*
Wykrywa różnice i aktualizuje podkatalogi w katalogu docelowym.
odczytuje:
srcDirPath - ścieżka katalogu źródłowego bezwzględna lub względem aktualnego katalogu roboczego (cwd) procesu; musi być zakończona '/'
srcDirPathLength - długość w bajtach ścieżki do katalogu źródłowego
subdirsSrc - uporządkowana lista podkatalogów znajdujących się w katalogu źródłowym
dstDirPath - ścieżka katalogu docelowego bezwzględna lub względem aktualnego katalogu roboczego (cwd) procesu; musi być zakończona '/'
dstDirPathLength - długość w bajtach ścieżki katalogu docelowego
subdirsDst - lista podkatalogów znajdujących się w katalogu docelowym uporządkowana w takim samym porządku jak filesSrc
zapisuje:
isReady - tablica boolowska o długości równej liczbie podkatalogów w katalogu źródłowym; jeżeli i-ty podkatalog z listy subdirsSrc nie istnieje w katalogu docelowym oraz:
- nie uda się go utworzyć, to isReady[i] == 0
- uda się go utworzyć, to isReady[i] == 1
zwraca:
< 0, jeżeli wystąpił błąd uniemożliwiający sprawdzenie wszystkich podkatalogów
> 0, jeżeli wystąpił przynajmniej 1 błąd uniemożliwiający utworzenie podkatalogu
0, jeżeli nie wystąpił błąd
*/
int updateDestinationDirectories(const char *srcDirPath, const size_t srcDirPathLength, list *subdirsSrc, const char *dstDirPath, const size_t dstDirPathLength, list *subdirsDst, char *isReady);

/*
Nierekurencyjnie synchronizuje katalog źródłowy i docelowy.
odczytuje:
sourcePath - ścieżka katalogu źródłowego bezwzględna lub względem aktualnego katalogu roboczego (cwd) procesu; musi być zakończona '/'
sourcePathLength - długość w bajtach ścieżki katalogu źródłowego
destinationPath - ścieżka katalogu docelowego bezwzględna lub względem aktualnego katalogu roboczego (cwd) procesu; musi być zakończona '/'
destinationPathLength - długość w bajtach ścieżki katalogu docelowego
zwraca:
< 0, jeżeli wystąpił błąd
0, jeżeli nie wystąpił błąd
*/
int synchronizeNonRecursively(const char *sourcePath, const size_t sourcePathLength, const char *destinationPath, const size_t destinationPathLength);
/*
Rekurencyjnie synchronizuje katalog źródłowy i docelowy.
odczytuje:
sourcePath - ścieżka katalogu źródłowego bezwzględna lub względem aktualnego katalogu roboczego (cwd) procesu; musi być zakończona '/'
sourcePathLength - długość w bajtach ścieżki do katalogu źródłowego
destinationPath - ścieżka katalogu docelowego bezwzględna lub względem aktualnego katalogu roboczego (cwd) procesu; musi być zakończona '/'
destinationPathLength - długość w bajtach ścieżki do katalogu docelowego
zwraca:
< 0, jeżeli wystąpił błąd
0, jeżeli nie wystąpił błąd
*/
int synchronizeRecursively(const char *sourcePath, const size_t sourcePathLength, const char *destinationPath, const size_t destinationPathLength);
/*
Wskaźnik na funkcję synchronizującą katalog źródłowy i docelowy.
*/
typedef int (*synchronizer)(const char *sourcePath, const size_t sourcePathLength, const char *destinationPath, const size_t destinationPathLength);

#endif