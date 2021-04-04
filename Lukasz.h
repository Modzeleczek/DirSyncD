// include guardy
#ifndef LUKASZ_H
#define LUKASZ_H

#include <dirent.h>
#include <sys/stat.h>

/*
odczytuje:
argc - liczba parametrów programu (opcji i argumentów razem)
argv - parametry programu

zapisuje:
source - ścieżka źródłowa
destination - ścieżka docelowa
interval - czas spania w sekundach
recursive - rekurecyjna synchronizacja katalogów
threshold - minimalna wielkość pliku, żeby był traktowany jako duży

zwraca:
< 0, jeżeli wystąpił błąd
0, jeżeli nie wystąpił błąd
*/
int parseParameters(int argc, char **argv, char **source, char **destination, unsigned int *interval, char *recursive);

/*
odczytuje:
path - ścieżka do katalogu

zwraca:
-1, jeżeli wystąpił błąd podczas otwierania katalogu
-2, jeżeli wystąpił błąd podczas zamykania katalogu
0, jeżeli nie wystąpił błąd
*/
int directoryValid(const char *path);

/*
odczytuje:
source - ścieżka źródłowa
destination - ścieżka docelowa
interval - czas spania w sekundach
recursive - rekurecyjna synchronizacja katalogów
threshold - minimalna wielkość pliku, żeby był traktowany jako duży
*/
void startDaemon(char *source, char *destination, unsigned int interval, char recursive);

typedef struct element element;
/*
Węzeł listy jednokierunkowej przechowujący wskaźnik do elementu katalogu.
*/
struct element;
/*
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
Lista jednokierunkowa.
*/
struct list;
/*
zapisuje:
l - pusta lista jednokierunkowa przeznaczona do pierwszego użycia
*/
void initialize(list *l);
/*
odczytuje:
newEntry - element katalogu, który dodajemy na koniec listy
zapisuje:
l - lista jednokierunkowa ze wstawionym na końcu węzłem zawierającym element katalogu newEntry
*/
void pushBack(list *l, struct dirent *newEntry);
/*
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
zapisuje:
l - lista jednokierunkowa posortowana z wykorzystaniem funkcji cmp porównującej węzły
*/
void listMergeSort(list *l);

/*
odczytuje:
srcFilePath - ścieżka do pliku źródłowego bezwzględna lub względem aktualnego katalogu roboczego (cwd) procesu
dstFilePath - ścieżka do pliku docelowego bezwzględna lub względem aktualnego katalogu roboczego (cwd) procesu
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
odczytuje:
srcFilePath - ścieżka do pliku źródłowego bezwzględna lub względem aktualnego katalogu roboczego (cwd) procesu
dstFilePath - ścieżka do pliku docelowego bezwzględna lub względem aktualnego katalogu roboczego (cwd) procesu
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
odczytuje:
path - ścieżka do pliku bezwzględna lub względem aktualnego katalogu roboczego (cwd) procesu

zwraca:
-1, jeżeli wystąpił błąd
0, jeżeli nie wystąpił błąd
*/
int removeFile(const char *path);

/*
odczytuje:
path - ścieżka do katalogu bezwzględna lub względem aktualnego katalogu roboczego (cwd) procesu
mode - uprawnienia utworzonego katalogu

zwraca:
-1, jeżeli wystąpił błąd
0, jeżeli nie wystąpił błąd
*/
int createEmptyDirectory(const char *path, mode_t mode);

/*
odczytuje:
path - ścieżka do katalogu bezwzględna lub względem aktualnego katalogu roboczego (cwd) procesu; musi być zakończona '/'
pathLength - długość ścieżki w bajtach

zwraca:
< 0, jeżeli wystąpił błąd krytyczny
> 0, jeżeli wystąpił błąd niekrytyczny
0, jeżeli nie wystąpił błąd
*/
int removeDirectoryRecursively(const char *path, const size_t pathLength);
/*
odczytuje:
path - ścieżka do katalogu bezwzględna lub względem aktualnego katalogu roboczego (cwd) procesu; nie musi być zakończona '/'
*/
int startDirectoryRemoval(const char *path);

/*
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
odczytuje:
dir - strumień katalogu otwarty za pomocą opendir

zapisuje:
files - lista zwykłych plików znajdujących się w katalogu
dirs - lista katalogów znajdujących się w katalogu

zwraca:
< 0, jeżeli wystąpił błąd
0, jeżeli nie wystąpił błąd
*/
int listFilesAndDirectories(DIR *dir, list *files, list *dirs);

#endif