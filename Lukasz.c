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
#include <errno.h>
#include <sys/mman.h>
#include <syslog.h>

static unsigned long long THRESHOLD; // static - zmienna globalna widoczna tylko w tym pliku; extern - zmienna globalna widoczna we wszystkich plikach
#define BUFFERSIZE 4096

void stringAppend(char *dst, const size_t offset, const char *src)
{
    strcpy(dst + offset, src); // przesuwamy się o offset bajtów względem początku stringa dst; od obliczonej pozycji wklejamy stringa src
    // możnaby użyć strcat, ale on dokleja src na końcu dst i prawdopodobnie oblicza długość dst strlenem, marnując czas
}

struct element
{
    element *next;
    struct dirent *entry; // element katalogu
};
int cmp(element *a, element *b)
{
    return strcmp(a->entry->d_name, b->entry->d_name); // funkcja porównująca elementy; porządek leksykograficzny
}

struct list
{
    element *first, *last;
    unsigned int count;
};
// w metodach struktury list zakładamy, że podano prawidłowy wskaźnik do listy
void initialize(list *l)
{
    l->first = l->last = NULL;
    l->count = 0;
}
int pushBack(list *l, struct dirent *newEntry)
{
    element *new = NULL;
    if((new = malloc(sizeof(element))) == NULL)
        return -1;
    new->entry = newEntry;
    new->next = NULL;
    if(l->first == NULL) // jeżeli lista jest pusta, to first i last są NULLami
    {
        l->first = new;
        l->last = new;
    }
    else // jeżeli lista nie jest pusta, to ani first ani last nie są NULLami
    {
        l->last->next = new; // ustawiamy aktualnemu lastowi nowy jako następny
        l->last = new; // przestawiamy aktualny last na nowy
    }
    ++l->count;
    return 0;
}
void clear(list *l)
{
    element *cur = l->first, *next;
    while(cur != NULL)
    {
        next = cur->next;
        free(cur);
        cur = next;
    }
    initialize(l);
}
void listMergeSort(list *l) {
    element *p, *q, *e, *tail, *list = l->first;
    int insize, nmerges, psize, qsize, i;
    if (list == NULL) // Silly special case: if `list' was passed in as NULL, return immediately.
        return;
    insize = 1;
    while (1) {
        p = list;
        list = NULL;
        tail = NULL;
        nmerges = 0; // count number of merges we do in this pass
        while (p) {
            nmerges++; // there exists a merge to be done
            // step `insize' places along from p
            q = p;
            psize = 0;
            for (i = 0; i < insize; i++) {
                psize++;
                    q = q->next;
                if (!q) break;
            }
            // if q hasn't fallen off end, we have two lists to merge
            qsize = insize;
            // now we have two lists; merge them
            while (psize > 0 || (qsize > 0 && q)) {
                // decide whether next element of merge comes from p or q
                if (psize == 0) {
                    // p is empty; e must come from q.
                    e = q; q = q->next; qsize--;
                } else if (qsize == 0 || !q) {
                    // q is empty; e must come from p.
                    e = p; p = p->next; psize--;
                } else if (cmp(p,q) <= 0) {
                    // First element of p is lower (or same); e must come from p.
                    e = p; p = p->next; psize--;
                } else {
                    // First element of q is lower; e must come from q.
                    e = q; q = q->next; qsize--;
                }
                // add the next element to the merged list
                if (tail) {
                    tail->next = e;
                } else {
                    list = e;
                }
                tail = e;
            }
            // now p has stepped `insize' places along, and q has too
            p = q;
        }
        tail->next = NULL;
        // If we have done only one merge, we're finished.
        if (nmerges <= 1) // allow for nmerges==0, the empty list case
        {
            l->last = tail;
            l->first = list;
            return;
        }
        // Otherwise repeat, merging lists twice the size
        insize *= 2;
    }
}

int copySmallFile(const char *srcFilePath, const char *dstFilePath, const mode_t dstMode, const struct timespec *dstAccessTime, const struct timespec *dstModificationTime)
{
    int ret = 0, in = -1, out = -1;
    if((in = open(srcFilePath, O_RDONLY)) == -1) // deskryptor pliku wejściowego; O_RDONLY - tylko odczyt
        ret = -1;
    else // jeżeli in != -1, czyli plik źródłowy został poprawnie otwarty, to open(dstFilePath, ...) się wykona;
    if((out = open(dstFilePath, O_WRONLY | O_CREAT | O_TRUNC, 0000)) == -1) // deskryptor pliku wyjściowego; O_WRONLY - tylko zapis; O_CREAT - utworzenie pliku, jeżeli jeszcze nie istnieje; O_TRUNC - jeżeli plik istnieje, to usunięcie jego zawartości; jeżeli plik jeszcze nie istnieje, to na początku nie dostanie żadnych uprawnień
        ret = -2;
    else // jeżeli in != -1 i out != -1, czyli plik źródłowy i docelowy zostały prawidłowo otwarte
    if(fchmod(out, dstMode) == -1) // ustawiamy plikowi docelowemu uprawnienia dstMode
        ret = -3;
    else
    {
        if(posix_fadvise(in, 0, 0, POSIX_FADV_SEQUENTIAL) == -1) // str. 124; wysyłamy jądru wskazówkę (poradę), że plik wejściowy będzie odczytywany sekwencyjnie i dlatego należy go wczytywać z wyprzedzeniem
            ret = 1; // błąd niekrytyczny
        char *buffer = NULL;
        if((buffer = malloc(sizeof(char) * BUFFERSIZE)) == NULL) // optymalny rozmiar bufora operacji wejścia-wyjścia dla danego pliku można sprawdzić w metadanych pobranych statem
            ret = -4;
        else
        {
            while(1) 
            {
                // brak obsługi błędów
                // int readBytes = read(in, buffer, BUFFERSIZE);
                // write(out, buffer, readBytes);
                // if(readBytes < BUFFERSIZE)
                //     break;
                char *position = buffer; // str. 45; pozycja w buforze wyjściowym
                size_t remainingBytes = BUFFERSIZE;
                ssize_t bytesRead;
                while(remainingBytes != 0 && (bytesRead = read(in, position, remainingBytes)) != 0)
                {
                    if(bytesRead == -1)
                    {
                        if(errno == EINTR)
                            continue;
                        ret = -5;
                        remainingBytes = BUFFERSIZE; // BUFFERSIZE - BUFFERSIZE == 0, więc druga pętla się nie wykona
                        bytesRead = 0; // ustawiamy na 0, aby if(bytesRead == 0) zakończył zewnętrzną pętlę
                        break;
                    }
                    remainingBytes -= bytesRead;
                    position += bytesRead;
                }
                position = buffer; // str. 48
                remainingBytes = BUFFERSIZE - remainingBytes; // zapisujemy całkowitą liczbę odczytanych bajtów, która zawsze jest mniejsza lub równa rozmiarowi bufora
                ssize_t bytesWritten;
                while(remainingBytes != 0 && (bytesWritten = write(out, position, remainingBytes)) != 0)
                {
                    if(bytesWritten == -1)
                    {
                        if(errno == EINTR)
                            continue;
                        ret = -6;
                        bytesRead = 0; // ustawiamy na 0, aby if(bytesRead == 0) zakończył zewnętrzną pętlę
                        break;
                    }
                    remainingBytes -= bytesWritten;
                    position += bytesWritten;
                }
                if(bytesRead == 0) // doszliśmy do końca pliku (EOF) lub wystąpił błąd
                    break;
            }
            free(buffer);
            if(ret >= 0) // jeżeli nie wystąpiły błędy podczas kopiowania
            {
                const struct timespec times[2] = { *dstAccessTime, *dstModificationTime };
                if(futimens(out, times) == -1) // ustawiamy plikowi wyjściowemu czasy ostatniego dostępu i modyfikacji; czas modyfikacji musi być ustawiony po skończeniu zapisów do pliku wyjściowego, bo zmieniają one go na aktualny czas systemowy
                    ret = -7;
            }
        }
    }
    if(in != -1 && close(in) == -1)
        ret = -8;
    if(out != -1 && close(out) == -1)
        ret = -9;
    return ret;
}

int copyBigFile(const char *srcFilePath, const char *dstFilePath, const unsigned long long fileSize, const mode_t dstMode, const struct timespec *dstAccessTime, const struct timespec *dstModificationTime)
{
    int ret = 0, in = -1, out = -1;
    if((in = open(srcFilePath, O_RDONLY)) == -1) // deskryptor pliku wejściowego; O_RDONLY - tylko odczyt
        ret = -1;
    else // jeżeli in != -1, czyli plik źródłowy został poprawnie otwarty, to open(dstFilePath, ...) się wykona;
    if((out = open(dstFilePath, O_WRONLY | O_CREAT | O_TRUNC, 0000)) == -1) // deskryptor pliku wyjściowego; O_WRONLY - tylko zapis; O_CREAT - utworzenie pliku, jeżeli jeszcze nie istnieje; O_TRUNC - jeżeli plik istnieje, to usunięcie jego zawartości; jeżeli plik jeszcze nie istnieje, to na początku nie dostanie żadnych uprawnień
        ret = -2;
    else // jeżeli in != -1 i out != -1, czyli plik źródłowy i docelowy zostały prawidłowo otwarte
    if(fchmod(out, dstMode) == -1) // ustawiamy plikowi docelowemu uprawnienia dstMode
        ret = -3;
    else
    {
        char *map;
        if((map = mmap(0, fileSize, PROT_READ, MAP_SHARED, in, 0)) == MAP_FAILED) // odwzorowujemy (mapujemy) w pamięci plik wejściowy w trybie do odczytu; jeżeli nie udało się zmapować pliku
            ret = -4;
        else
        {
            if(madvise(map, fileSize, MADV_SEQUENTIAL) == -1) // str. 121; wysyłamy jądru wskazówkę (poradę), że plik wejściowy będzie odczytywany sekwencyjnie
                ret = 1; // błąd niekrytyczny
            char *buffer = NULL;
            if((buffer = malloc(sizeof(char) * BUFFERSIZE)) == NULL) // optymalny rozmiar bufora operacji wejścia-wyjścia dla danego pliku można sprawdzić w metadanych pobranych statem; jeżeli nie udało się zarezerwować pamięci na bufor
                ret = -5;
            else
            {
                unsigned long long b; // numer bajtu w pliku wejściowym
                char *position; // pozycja w buforze wyjściowym
                size_t remainingBytes;
                ssize_t bytesWritten;
                for(b = 0; b + BUFFERSIZE < fileSize; b += BUFFERSIZE) // nie może być (b < fileSize - BUFFERSIZE), bo b i fileSize są unsigned, więc jeżeli fileSize < BUFFERSIZE i odejmiemy, to mamy przepełnienie
                {
                    memcpy(buffer, map + b, BUFFERSIZE); // kopiujemy BUFFERSIZE (rozmiar bufora) bajtów ze zmapowanej pamięci do bufora zapisu
                    position = buffer; // str. 48
                    remainingBytes = BUFFERSIZE; // zapisujemy całkowitą liczbę odczytanych bajtów, która zawsze jest równa rozmiarowi bufora
                    while(remainingBytes != 0 && (bytesWritten = write(out, position, remainingBytes)) != 0)
                    {
                        if(bytesWritten == -1)
                        {
                            if(errno == EINTR)
                                continue;
                            ret = -6;
                            b = ULLONG_MAX - BUFFERSIZE; // dla pewności można ustawić b = ULLONG_MAX - BUFFERSIZE; w praktyce fileSize zawsze jest mniejsze od ULLONG_MAX (nie ma plików o rozmiarze 2^64 B), więc można ustawić b = fileSize, aby zakończyć fora i spowodować, że przed poniższą pętlą remainingBytes == 0 - wtedy memcpy nie skopiuje bajtów, a pętla się nie wykona
                            break;
                        }
                        remainingBytes -= bytesWritten;
                        position += bytesWritten;
                    }
                }
                if(ret >= 0) // jeżeli nie wystąpił błąd podczas dotychczasowego kopiowania
                {
                    remainingBytes = fileSize - b; // liczba bajtów z końca pliku, które nie zmieściły się w buforze
                    memcpy(buffer, map + b, remainingBytes);
                    position = buffer;
                    while(remainingBytes != 0 && (bytesWritten = write(out, position, remainingBytes)) != 0)
                    {
                        if(bytesWritten == -1)
                        {
                            if(errno == EINTR)
                                continue;
                            ret = -7;
                            break;
                        }
                        remainingBytes -= bytesWritten;
                        position += bytesWritten;
                    }
                }
                free(buffer);
                if(ret >= 0) // jeżeli nie wystąpiły błędy podczas kopiowania
                {
                    const struct timespec times[2] = { *dstAccessTime, *dstModificationTime };
                    if(futimens(out, times) == -1) // ustawiamy plikowi wyjściowemu czasy ostatniego dostępu i modyfikacji
                        ret = -8;
                }
            }
            if(munmap(map, fileSize) == -1)
                ret = -9;
        }
    }
    if(in != -1 && close(in) == -1)
        ret = -10;
    if(out != -1 && close(out) == -1)
        ret = -11;
    return ret;
}

int removeFile(const char *path)
{
    return unlink(path); // unlink służy tylko do usuwania plików; katalog trzeba usunąć rmdirem, ale najpierw trzeba zapewnić, aby był pusty
}

int createEmptyDirectory(const char *path, mode_t mode)
{
    return mkdir(path, mode);
}

// ścieżka musi być zakończona '/'
int removeDirectoryRecursively(const char *path, const size_t pathLength)
{
    int ret = 0;
    DIR *dir = NULL;
    if((dir = opendir(path)) == NULL)
        ret = -1;
    else
    {
        list dirs, files;
        initialize(&dirs);
        initialize(&files);
        if(listFilesAndDirectories(dir, &files, &dirs) < 0)
            ret = -2;
        else
        {
            char *subPath = NULL;
            if((subPath = malloc(sizeof(char) * PATH_MAX)) == NULL) // rezerwujemy PATH_MAX bajtów na ścieżki podkatalogów i plików; jeżeli nie udało się zarezerwować pamięci
                ret = -3;
            else
            {
                strcpy(subPath, path); // kopiujemy path do nextPath
                element *cur = dirs.first;
                while(cur != NULL)
                {
                    stringAppend(subPath, pathLength, cur->entry->d_name); // dopisujemy nazwę podkatalogu do aktualnej ścieżki katalogu
                    size_t subPathLength = pathLength + strlen(cur->entry->d_name);
                    stringAppend(subPath, subPathLength++, "/"); // dopisujemy '/' do utworzonej ścieżki
                    if(removeDirectoryRecursively(subPath, subPathLength) < 0) // rekurencyjnie wywołujemy usuwanie podkatalogów; jeżeli nie udało się usunąć któregoś podkatalogu
                        ret = -4; // zaznaczamy błąd wyższemu wywołaniu
                    cur = cur->next;
                }
                cur = files.first;
                while(cur != NULL) // usuwamy pliki z aktualnego katalogu
                {
                    stringAppend(subPath, pathLength, cur->entry->d_name); // dopisujemy nazwę pliku do aktualnej ścieżki katalogu
                    if(removeFile(subPath) == -1) // usuwamy plik
                        ret = -5;
                    cur = cur->next;
                }
                free(subPath);
            }
        }
        clear(&dirs); // czyścimy listę podkatalogów
        clear(&files); // czyścimy listę plików
    }
    if(dir != NULL && closedir(dir) == -1) // zamykamy aktualny katalog
        ret = 1; // jeżeli nie uda się zamknąć, to zaznaczamy liczbą dodatnią błąd niekrytyczny wyższemu wywołaniu
    // błąd krytyczny w funkcji występuje, jeżeli nie uda się usunąć któregokolwiek elementu z podkatalogów aktualnego katalogu
    if(ret >= 0 && rmdir(path) == -1) // jeżeli nie wystąpił żaden błąd krytyczny, usuwamy aktualny katalog; jeżeli nie uda się usunąć aktualnego katalogu
        ret = -6; // zaznaczamy błąd krytyczny wyższemu wywołaniu
    return ret;
}
// do usuwania katalogów bez kontekstu, czyli kiedy jako path podamy np. argument programu, którego nie możemy zmieniać
int startDirectoryRemoval(const char *path)
{
    char *rootPath = NULL;
    if((rootPath = malloc(sizeof(char) * PATH_MAX)) == NULL) // rezerwujemy PATH_MAX bajtów na ścieżki katalogów
        return -1;
    strcpy(rootPath, path); // kopiujemy path do rootPath
    size_t rootPathLength = strlen(rootPath);
    if(rootPath[rootPathLength - 1] != '/') // jeżeli bezpośrednio przed null terminatorem nie ma '/'
        stringAppend(rootPath, rootPathLength++, "/"); // wstawiamy '/' na miejscu null terminatora; zwiększamy długość ścieżki o 1; wstawiamy null terminator za '/'
    int ret = removeDirectoryRecursively(rootPath, rootPathLength);
    free(rootPath);
    return ret;
}

// zakładamy, że podano prawidłowy wskaźnik do listy i do katalogu
int listFiles(DIR *dir, list *files)
{
    struct dirent *entry;
    errno = 0;
    while((entry = readdir(dir)) != NULL)
    {
        if(entry->d_type == DT_REG && pushBack(files, entry) < 0) // jeżeli element jest zwykłym plikiem (regular file) i wystąpił błąd podczas dodawania elementu
            return -1; // przerywamy, bo listy elementów muszą być kompletne do porównywania katalogów
    }
    if(errno != 0) // jeżeli wystąpił błąd podczas odczytywania elementu
        return -2;
    return 0;
}
int listFilesAndDirectories(DIR *dir, list *files, list *dirs)
{
    struct dirent *entry;
    errno = 0;
    while((entry = readdir(dir)) != NULL)
    {
        if(entry->d_type == DT_REG) // jeżeli element jest zwykłym plikiem (regular file)
        {
            if(pushBack(files, entry) < 0)
                return -1;
        }
        else if(entry->d_type == DT_DIR) // jeżeli element jest katalogiem (directory)
        {
            if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 && 
                pushBack(dirs, entry) < 0) // jeżeli katalog ma nazwę różną od "." i różną od "..", dodajemy go do listy katalogów
                return -2;
        }
        // elementy o innych typach (symlinki; urządzenia blokowe, tekstowe; sockety; itp.) ignorujemy
    }
    if(errno != 0) // jeżeli wystąpił błąd podczas odczytywania elementu
        return -3;
    return 0;
}

// jeżeli inode (i-węzeł, czyli fizyczny plik w pamięci masowej) ma w katalogu źródłowym więcej niż 1 nazwę, to w katalogu docelowym każda nazwa jest kopiowana jako oddzielny inode
int updateDestinationFiles(const char *srcDirPath, const size_t srcDirPathLength, list *filesSrc, const char *dstDirPath, const size_t dstDirPathLength, list *filesDst/*, const unsigned long long threshold*/)
{
    // ściezka w systemie plików ext4 może mieć maksymalnie PATH_MAX (4096) bajtów
    char *srcFilePath = NULL, *dstFilePath = NULL;
    if((srcFilePath = malloc(sizeof(char) * PATH_MAX)) == NULL) // rezerwujemy PATH_MAX bajtów na ścieżkę do aktualnego pliku z katalogu źródłowego
        return -1;
    if((dstFilePath = malloc(sizeof(char) * PATH_MAX)) == NULL) // rezerwujemy PATH_MAX bajtów na ścieżkę do aktualnego pliku z katalogu docelowego
    {
        free(srcFilePath);
        return -2;
    }
    strcpy(srcFilePath, srcDirPath); // kopiujemy ścieżkę katalogu źródłowego do srcFilePath
    strcpy(dstFilePath, dstDirPath); // kopiujemy ścieżkę katalogu docelowego do dstFilePath
    element *curS = filesSrc->first, *curD = filesDst->first;
    struct stat srcFile, dstFile;
    int status = 0, ret = 0;
    openlog("DirSyncD", LOG_ODELAY | LOG_PID, LOG_DAEMON); // otwieramy połączenie z logiem /var/log/syslog
    while(curS != NULL && curD != NULL)
    {
        char *srcFileName = curS->entry->d_name, *dstFileName = curD->entry->d_name;
        int comparison = strcmp(srcFileName, dstFileName);
        if(comparison > 0) // srcFileName > dstFileName
        {
            stringAppend(dstFilePath, dstDirPathLength, dstFileName); // dopisujemy nazwę pliku docelowego do ścieżki katalogu docelowego
            status = removeFile(dstFilePath); // usuwamy plik docelowy
            syslog(LOG_INFO, "usuwamy plik %s; %i\n", dstFilePath, status);
            if(status != 0) ret = 1;
            curD = curD->next;
        }
        else
        {
            stringAppend(srcFilePath, srcDirPathLength, srcFileName); // dopisujemy nazwę pliku źródłowego do ścieżki katalogu źródłowego
            if(stat(srcFilePath, &srcFile) == -1) // odczytujemy metadane pliku źródłowego
            {
                curS = curS->next; // przesuwamy wskaźnik na liście źródłowej
                if(comparison < 0)
                {
                    syslog(LOG_INFO, "kopiujemy plik %s do katalogu %s; %i\n", srcFilePath, dstDirPath, errno); // status zapisany przez stat do errno (liczba dodatnia)
                    ret = 2;
                }
                else // if(comparison == 0)
                {
                    syslog(LOG_INFO, "odczytujemy metadane pliku źródłowego %s; %i\n", srcFilePath, errno);
                    curD = curD->next; // przesuwamy wskaźnik na liście docelowej
                    ret = 3;
                }
                continue; // przechodzimy do następnej iteracji, ale nie przerywamy algorytmu
            }
            if(comparison < 0) // srcFileName < dstFileName
            {
                stringAppend(dstFilePath, dstDirPathLength, srcFileName); // dopisujemy nazwę pliku źródłowego do ścieżki katalogu docelowego
                // kopiujemy plik źródłowy do katalogu docelowego; przepisujemy czas modyfikacji z pliku źródłowego do pliku docelowego
                if(srcFile.st_size < THRESHOLD) // jeżeli plik ma rozmiar w bajtach mniejszy niż poziom dużego pliku
                    status = copySmallFile(srcFilePath, dstFilePath, srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim); // kopiujemy uprawnienia i typ (zawsze plik zwykły) pliku źródłowego do pliku docelowego
                else
                    status = copyBigFile(srcFilePath, dstFilePath, srcFile.st_size, srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim);
                syslog(LOG_INFO, "kopiujemy plik %s do katalogu %s; %i\n", srcFilePath, dstDirPath, status); // nie przerywamy, jeżeli nie udało się skopiować pliku, ale wypisujemy status operacji
                if(status != 0) ret = 4;
                curS = curS->next;
            }
            else // srcFileName == dstFileName
            {
                stringAppend(dstFilePath, dstDirPathLength, dstFileName); // dopisujemy nazwę pliku docelowego do ścieżki katalogu docelowego
                if(stat(dstFilePath, &dstFile) == -1) // odczytujemy metadane pliku docelowego
                {
                    syslog(LOG_INFO, "odczytujemy metadane pliku docelowego %s; %i\n", dstFilePath, errno);
                    ret = 5;
                }
                else // jeżeli poprawnie odczytaliśmy metadane
                if(srcFile.st_mtim.tv_sec != dstFile.st_mtim.tv_sec || srcFile.st_mtim.tv_nsec != dstFile.st_mtim.tv_nsec) // srcFile.st_mtim != dstFile.st_mtim; jeżeli plik docelowy ma inny czas modyfikacji (wcześniejszy - jest nieaktualny lub późniejszy - został edytowany)
                {
                    // przepisujemy plik źródłowy do istniejącego pliku docelowego
                    if(srcFile.st_size < THRESHOLD) // jeżeli plik ma rozmiar w bajtach mniejszy niż poziom dużego pliku
                        status = copySmallFile(srcFilePath, dstFilePath, srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim); // kopiujemy uprawnienia i typ (zawsze plik zwykły) pliku źródłowego do pliku docelowego
                    else
                        status = copyBigFile(srcFilePath, dstFilePath, srcFile.st_size, srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim);
                    syslog(LOG_INFO, "przepisujemy %s do %s; %i\n", srcFilePath, dstFilePath, status); // nie przerywamy, jeżeli nie udało się przepisać pliku, ale wypisujemy status operacji
                    if(status != 0) ret = 6;
                }
                else // przy kopiowaniu przepisujemy uprawnienia, ale jeżeli nie przepisywaliśmy pliku, to sprawdzamy, czy pliki nie mają różnych uprawnień
                // if(srcFile.st_ctim.tv_sec != dstFile.st_ctim.tv_sec || srcFile.st_ctim.tv_nsec != dstFile.st_ctim.tv_nsec) // jeżeli plik docelowy ma inny czas zmiany (czas modyfikacji metadanych m. in. uprawnień, właściciela, atim, mtim, itp.); aby zmienić czas zmiany (ctim), trzeba przestawić czas systemowy, wprowadzić jakąś zmianę w metadanych pliku i z przywrócić czas systemowy; zmiana czasy systemowego może zaburzyć inne procesy, więc tego nie robimy
                if(srcFile.st_mode != dstFile.st_mode) // jeżeli pliki mają różne uprawnienia
                {
                    if(chmod(dstFilePath, srcFile.st_mode) == -1) // przepisujemy uprawnienia z pliku źródłowego do katalogu docelowego
                    {
                        status = errno;
                        ret = 7;
                    }
                    else
                        status = 0;
                    syslog(LOG_INFO, "przepisujemy uprawnienia pliku %s do %s; %i\n", srcFilePath, dstFilePath, status);
                }
                curS = curS->next; // przesuwamy wskaźniki elementów na listach, bo niżej nie będą już potrzebne
                curD = curD->next;
            }
        }
    }
    while(curD != NULL) // jeżeli w powyższej pętli nie wystąpił błąd; usuwamy pozostałe pliki z katalogu docelowego, począwszy od aktualnie wskazywanego przez curD, ponieważ nie istnieją one w katalogu źródłowym
    {
        char *dstFileName = curD->entry->d_name;
        stringAppend(dstFilePath, dstDirPathLength, dstFileName); // dopisujemy nazwę pliku docelowego do ścieżki katalogu docelowego
        status = removeFile(dstFilePath);
        syslog(LOG_INFO, "usuwamy plik %s; %i\n", dstFilePath, status);
        if(status != 0) ret = 8;
        curD = curD->next;
    }
    while(curS != NULL) // kopiujemy pozostałe pliki z katalogu źródłowego, począwszy od aktualnie wskazywanego przez curS
    {
        char *srcFileName = curS->entry->d_name;
        stringAppend(srcFilePath, srcDirPathLength, srcFileName); // dopisujemy nazwę pliku źródłowego do ścieżki katalogu źródłowego
        if(stat(srcFilePath, &srcFile) == -1) // odczytujemy metadane pliku źródłowego
        {
            syslog(LOG_INFO, "kopiujemy plik %s do katalogu %s; %i\n", srcFilePath, dstDirPath, errno);
            ret = 9;
        }
        else
        {
            stringAppend(dstFilePath, dstDirPathLength, srcFileName); // dopisujemy nazwę pliku źródłowego do ścieżki katalogu docelowego
            // kopiujemy plik źródłowy do katalogu docelowego; przepisujemy czas modyfikacji z pliku źródłowego do pliku docelowego
            if(srcFile.st_size < THRESHOLD) // jeżeli plik ma rozmiar w bajtach mniejszy niż poziom dużego pliku
                status = copySmallFile(srcFilePath, dstFilePath, srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim); // kopiujemy uprawnienia i typ (zawsze plik zwykły) pliku źródłowego do pliku docelowego
            else
                status = copyBigFile(srcFilePath, dstFilePath, srcFile.st_size, srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim);
            syslog(LOG_INFO, "kopiujemy plik %s do katalogu %s; %i\n", srcFilePath, dstDirPath, status);
            if(status != 0) ret = 10;
        }
        curS = curS->next;
    }
    free(srcFilePath);
    free(dstFilePath);
    closelog();
    return ret;
}
// wszędzie w nazwach zmiennych wyraz "file" jest użyty w znaczeniu "katalog", bo nie chce mi się zmieniać, a katalog to też plik
int updateDestinationDirectories(const char *srcDirPath, const size_t srcDirPathLength, list *filesSrc, const char *dstDirPath, const size_t dstDirPathLength, list *filesDst, char *isReady)
{
    // ściezka w systemie plików ext4 może mieć maksymalnie PATH_MAX (4096) bajtów
    char *srcFilePath = NULL, *dstFilePath = NULL;
    if((srcFilePath = malloc(sizeof(char) * PATH_MAX)) == NULL) // rezerwujemy PATH_MAX bajtów na ścieżkę do aktualnego pliku z katalogu źródłowego
        return -1;
    if((dstFilePath = malloc(sizeof(char) * PATH_MAX)) == NULL) // rezerwujemy PATH_MAX bajtów na ścieżkę do aktualnego pliku z katalogu docelowego
    {
        free(srcFilePath);
        return -2;
    }
    strcpy(srcFilePath, srcDirPath); // kopiujemy ścieżkę katalogu źródłowego do srcFilePath
    strcpy(dstFilePath, dstDirPath); // kopiujemy ścieżkę katalogu docelowego do dstFilePath
    element *curS = filesSrc->first, *curD = filesDst->first;
    struct stat srcFile, dstFile;
    unsigned int i = 0;
    int status = 0, ret = 0;
    openlog("DirSyncD", LOG_ODELAY | LOG_PID, LOG_DAEMON); // otwieramy połączenie z logiem /var/log/syslog
    while(curS != NULL && curD != NULL)
    {
        char *srcFileName = curS->entry->d_name, *dstFileName = curD->entry->d_name;
        int comparison = strcmp(srcFileName, dstFileName);
        if(comparison > 0) // srcFileName > dstFileName
        {
            stringAppend(dstFilePath, dstDirPathLength, dstFileName); // dopisujemy nazwę katalogu do usunięcia do ścieżki katalogu docelowego
            size_t length = dstDirPathLength + strlen(dstFileName);
            stringAppend(dstFilePath, length++, "/"); // dopisujemy '/' do ścieżki usuwanego katalogu
            status = removeDirectoryRecursively(dstFilePath, length);
            syslog(LOG_INFO, "usuwamy katalog %s; %i\n", dstFilePath, status);
            if(status != 0) ret = 1;
            curD = curD->next;
        }
        else
        {
            stringAppend(srcFilePath, srcDirPathLength, srcFileName); // dopisujemy nazwę katalogu źródłowego do ścieżki katalogu (korzenia) źródłowego
            if(stat(srcFilePath, &srcFile) == -1) // odczytujemy metadane katalogu źródłowego
            {
                curS = curS->next; // przesuwamy wskaźnik na liście źródłowej
                if(comparison < 0)
                {
                    syslog(LOG_INFO, "tworzymy katalog %s; %i\n", dstFilePath, errno);
                    isReady[i++] = 0; // zaznaczamy, że katalog nie jest gotowy do synchronizacji, bo nie istnieje
                    ret = 2;
                }
                else // if(comparison == 0)
                {
                    syslog(LOG_INFO, "odczytujemy metadane katalogu źródłowego %s; %i\n", srcFilePath, errno);
                    isReady[i++] = 1; // jeżeli nie udało się sprawdzić, czy katalog docelowy ma takie same uprawnienia, jak źródłowy, to zakładamy, że ma
                    ret = 3;
                    curD = curD->next; // przesuwamy wskaźnik na liście docelowej
                }
                continue; // przechodzimy do następnej iteracji, ale nie przerywamy algorytmu
            }
            if(comparison < 0) // srcFileName < dstFileName
            {
                stringAppend(dstFilePath, dstDirPathLength, srcFileName); // dopisujemy nazwę katalogu do stworzenia do ścieżki katalogu docelowego
                status = createEmptyDirectory(dstFilePath, srcFile.st_mode); // tworzymy w katalogu docelowym katalog o nazwie takiej, jak w katalogu źródłowym; nie przepisujemy czasu modyfikacji, bo nie zwracamy na niego uwagi przy synchronizacji - wszystkie katalogi i tak będą rekurencyjnie przejrzane w celu wykrycia zmian plików
                syslog(LOG_INFO, "tworzymy katalog %s; %i\n", dstFilePath, status);
                if(status < 0)
                {
                    isReady[i++] = 0;
                    ret = 4;
                }
                else isReady[i++] = 1;
                curS = curS->next;
            }
            else // if(comparison == 0) // srcFileName == dstFileName
            {
                isReady[i++] = 1;
                stringAppend(dstFilePath, dstDirPathLength, dstFileName); // dopisujemy nazwę katalogu docelowego do ścieżki katalogu (korzenia) docelowego
                if(stat(dstFilePath, &dstFile) == -1) // odczytujemy metadane katalogu docelowego
                {
                    syslog(LOG_INFO, "odczytujemy metadane katalogu docelowego %s; %i\n", dstFilePath, errno);
                    ret = 5;
                }
                else // jeżeli poprawnie odczytaliśmy metadane
                // if(srcFile.st_mtim.tv_sec != dstFile.st_mtim.tv_sec || srcFile.st_mtim.tv_nsec != dstFile.st_mtim.tv_nsec) // ignorujemy czas modyfikacji zawartości (zmienia się on podczas tworzenia i usuwania plików z katalogu)
                if(srcFile.st_mode != dstFile.st_mode) // jeżeli katalogi mają różne uprawnienia
                {
                    if(chmod(dstFilePath, srcFile.st_mode) == -1) // przepisujemy uprawnienia z katalogu źródłowego do katalogu docelowego
                    {
                        status = errno;
                        ret = 6;
                    }
                    else
                        status = 0;
                    syslog(LOG_INFO, "przepisujemy uprawnienia katalogu %s do %s; %i\n", srcFilePath, dstFilePath, status);
                }
                curS = curS->next;
                curD = curD->next;
            }
        }
    }
    while(curD != NULL) // usuwamy pozostałe katalogi z katalogu docelowego, począwszy od aktualnie wskazywanego przez curD, ponieważ nie istnieją one w katalogu źródłowym
    {
        char *dstFileName = curD->entry->d_name;
        stringAppend(dstFilePath, dstDirPathLength, dstFileName); // dopisujemy nazwę katalogu do usunięcia do ścieżki katalogu docelowego
        size_t length = dstDirPathLength + strlen(dstFileName);
        stringAppend(dstFilePath, length++, "/"); // dopisujemy '/' do ścieżki usuwanego katalogu
        status = removeDirectoryRecursively(dstFilePath, length);
        syslog(LOG_INFO, "usuwamy katalog %s; %i\n", dstFilePath, status);
        ret = 7;
        curD = curD->next;
    }
    while(curS != NULL) // kopiujemy pozostałe katalogi z katalogu źródłowego, począwszy od aktualnie wskazywanego przez curS
    {
        char *srcFileName = curS->entry->d_name;
        stringAppend(srcFilePath, srcDirPathLength, srcFileName); // dopisujemy nazwę katalogu źródłowego do ścieżki katalogu (korzenia) źródłowego
        if(stat(srcFilePath, &srcFile) == -1) // odczytujemy metadane katalogu źródłowego
        {
            curS = curS->next; // przesuwamy wskaźnik na liście źródłowej
            syslog(LOG_INFO, "tworzymy katalog %s; %i\n", dstFilePath, errno);
            isReady[i++] = 0; // zaznaczamy, że katalog nie jest gotowy do synchronizacji, bo nie istnieje
            ret = 8;
            continue; // przechodzimy do następnej iteracji, ale nie przerywamy algorytmu
        }
        stringAppend(dstFilePath, dstDirPathLength, srcFileName); // dopisujemy nazwę katalogu do stworzenia do ścieżki katalogu docelowego
        status = createEmptyDirectory(dstFilePath, srcFile.st_mode);
        syslog(LOG_INFO, "tworzymy katalog %s; %i\n", dstFilePath, status);
        if(status < 0)
        {
            isReady[i++] = 0;
            ret = 9;
        }
        else isReady[i++] = 1;
        curS = curS->next;
    }
    free(srcFilePath);
    free(dstFilePath);
    closelog();
    return ret;
}

int synchronizeNonRecursively(const char *sourcePath, const size_t sourcePathLength, const char *destinationPath, const size_t destinationPathLength)
{
    int ret = 0;
    DIR *dirS = NULL, *dirD = NULL;
    if((dirS = opendir(sourcePath)) == NULL)
        ret = -1;
    else if((dirD = opendir(destinationPath)) == NULL)
        ret = -2;
    else
    {
        list filesS, filesD;
        initialize(&filesS);
        initialize(&filesD);
        if(listFiles(dirS, &filesS) < 0)
            ret = -3;
        else if(listFiles(dirD, &filesD) < 0)
            ret = -4;
        else
        {
            listMergeSort(&filesS);
            listMergeSort(&filesD);
            updateDestinationFiles(sourcePath, sourcePathLength, &filesS, destinationPath, destinationPathLength, &filesD); // ignorujemy błędy synchronizacji, bo i tak statusy operacji na plikach są zapisywane w logu
        }
        clear(&filesS);
        clear(&filesD);
    }
    // zamknąć katalog można dopiero, gdy skończymy używać obiektów typu dirent, które odczytaliśmy readdirem na tym katalogu, bo są one usuwane z pamięci w momencie zamknięcia katalogu closedirem
    if(dirS != NULL) // jeżeli dirS == NULL, to closedir(dirS) się nie wykona
        closedir(dirS); // ignorujemy błąd zamknięcia katalogu
    if(dirD != NULL)
        closedir(dirD);
    return ret;
}
int synchronizeRecursively(const char *sourcePath, const size_t sourcePathLength, const char *destinationPath, const size_t destinationPathLength)
{
    int ret = 0;
    DIR *dirS = NULL, *dirD = NULL;
    if((dirS = opendir(sourcePath)) == NULL)
        ret = -1;
    else if((dirD = opendir(destinationPath)) == NULL) // jeżeli dirS != NULL, czyli katalog źródłowy został poprawnie otwarty, to opendir(destinationPath) się wykona
        ret = -2;
    else
    {
        list filesS, dirsS;
        initialize(&filesS);
        initialize(&dirsS);
        list filesD, dirsD;
        initialize(&filesD);
        initialize(&dirsD);
        if(listFilesAndDirectories(dirS, &filesS, &dirsS) < 0)
            ret = -3;
        else if(listFilesAndDirectories(dirD, &filesD, &dirsD) < 0)
            ret = -4;
        else
        {
            listMergeSort(&filesS);
            listMergeSort(&filesD);
            updateDestinationFiles(sourcePath, sourcePathLength, &filesS, destinationPath, destinationPathLength, &filesD);
            clear(&filesS);
            clear(&filesD);

            listMergeSort(&dirsS);
            listMergeSort(&dirsD);
            char *isReady = NULL; // można zrobić bitmapę (bitset z c++)
            if((isReady = malloc(sizeof(char) * dirsS.count)) == NULL)
                ret = -5;
            else
            {
                updateDestinationDirectories(sourcePath, sourcePathLength, &dirsS, destinationPath, destinationPathLength, &dirsD, isReady);
                // jeszcze nie czyścimy dirsS, bo rekurencyjnie będziemy wywoływać funkcję synchronizeRecursively na katalogach z dirsS
                clear(&dirsD);

                char *nextSourcePath = NULL, *nextDestinationPath = NULL;
                if((nextSourcePath = malloc(sizeof(char) * PATH_MAX)) == NULL) // rezerwujemy PATH_MAX bajtów na ścieżki katalogów źródłowych
                    ret = -6;
                else if((nextDestinationPath = malloc(sizeof(char) * PATH_MAX)) == NULL) // rezerwujemy PATH_MAX bajtów na ścieżki katalogów docelowych
                    ret = -7;
                else
                {
                    strcpy(nextSourcePath, sourcePath); // kopiujemy sourcePath do nextSourcePath
                    strcpy(nextDestinationPath, destinationPath); // kopiujemy destinationPath do nextDestinationPath
                    element *curS = dirsS.first;
                    unsigned int i = 0;
                    while(curS != NULL)
                    {
                        if(isReady[i++] == 1) // jeżeli podkatalog jest gotowy do synchronizacji
                        {
                            stringAppend(nextSourcePath, sourcePathLength, curS->entry->d_name); // dopisujemy nazwę katalogu do aktualnej ścieżki katalogu źródłowego
                            size_t nextSourcePathLength = sourcePathLength + strlen(curS->entry->d_name);
                            stringAppend(nextSourcePath, nextSourcePathLength, "/"); // dopisujemy '/' do utworzonej ścieżki
                            nextSourcePathLength += 1; // +1, bo dopisaliśmy '/'

                            stringAppend(nextDestinationPath, destinationPathLength, curS->entry->d_name); // dopisujemy nazwę katalogu do aktualnej ścieżki katalogu docelowego
                            size_t nextDestinationPathLength = destinationPathLength + strlen(curS->entry->d_name);
                            stringAppend(nextDestinationPath, nextDestinationPathLength, "/"); // dopisujemy '/' do utworzonej ścieżki
                            nextDestinationPathLength += 1; // +1, bo dopisaliśmy '/'

                            synchronizeRecursively(nextSourcePath, nextSourcePathLength, nextDestinationPath, nextDestinationPathLength); // ignorujemy błędy synchronizacji podkatalogów
                        } // jeżeli podkatalog nie jest gotowy do synchronizacji, to go pomijamy
                        curS = curS->next;
                    }
                }
                free(isReady);
                if(nextSourcePath != NULL)
                    free(nextSourcePath);
                if(nextDestinationPath != NULL)
                    free(nextDestinationPath);
            }
        }
        clear(&dirsS); // czyścimy dirsS
        if(dirsD.count != 0) // jeżeli nie udało się zarezerwować pamięci na isReady
            clear(&dirsD);
    }
    // zamknąć katalog można dopiero, gdy skończymy używać obiektów typu dirent, które odczytaliśmy readdirem na tym katalogu, bo są one usuwane z pamięci w momencie zamknięcia katalogu closedirem
    if(dirS != NULL && closedir(dirS) == -1) // jeżeli dirS == NULL, to closedir(dirS) się nie wykona
        ret = -8;
    if(dirD != NULL && closedir(dirD) == -1)
        ret = -9;
    return ret;
}
// do synchronizacji bez kontekstu
int startSynchronization(const char *source, const char *destination, synchronizer synchronize)
{
    char *sourcePath = NULL, *destinationPath = NULL;
    if((sourcePath = malloc(sizeof(char) * PATH_MAX)) == NULL) // rezerwujemy PATH_MAX bajtów na ścieżki katalogów źródłowych
        return -1;
    if((destinationPath = malloc(sizeof(char) * PATH_MAX)) == NULL) // rezerwujemy PATH_MAX bajtów na ścieżki katalogów docelowych
    {
        free(sourcePath);
        return -2;
    }
    strcpy(sourcePath, source); // kopiujemy source do sourcePath
    strcpy(destinationPath, destination); // kopiujemy destination do destinationPath
    size_t sourcePathLength = strlen(sourcePath);
    if(sourcePath[sourcePathLength - 1] != '/') // jeżeli bezpośrednio przed null terminatorem nie ma '/'
        stringAppend(sourcePath, sourcePathLength++, "/"); // wstawiamy '/' na miejscu null terminatora; zwiększamy długość ścieżki o 1; wstawiamy null terminator za '/'
    size_t destinationPathLength = strlen(destinationPath);
    if(destinationPath[destinationPathLength - 1] != '/') // jeżeli bezpośrednio przed null terminatorem nie ma '/'
        stringAppend(destinationPath, destinationPathLength++, "/");
    int ret = synchronize(sourcePath, sourcePathLength, destinationPath, destinationPathLength);
    free(sourcePath);
    free(destinationPath);
    return ret;
}

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

char forcedSynchronization;
void sigusr1Handler(int signo) // funkcja obsługi sygnału SIGUSR1
{
    forcedSynchronization = 1;
}

char stop;
void sigtermHandler(int signo) // funkcja obsługi sygnału SIGTERM
{
    stop = 1;
}

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
    char *sourcePath = NULL, *destinationPath = NULL;
    if((sourcePath = malloc(sizeof(char) * PATH_MAX)) == NULL) // rezerwujemy PATH_MAX bajtów na ścieżkę katalogu źródłowego
        ret = -1;
    else if((destinationPath = malloc(sizeof(char) * PATH_MAX)) == NULL) // rezerwujemy PATH_MAX bajtów na ścieżkę katalogu docelowego
        ret = -2;
    else if(realpath(source, sourcePath) == NULL) // wyznaczamy ścieżkę bezwzględną katalogu źródłowego
    {
        perror("realpath; source");
        ret = -3;
    }
    else if(realpath(destination, destinationPath) == NULL) // wyznaczamy ścieżkę bezwzględną katalogu docelowego
    {
        perror("realpath; destination");
        ret = -4;
    }
    else if(setsid() == -1) // tworzymy nową sesję i grupę procesów
        ret = -5; // nie wywołujemy perror, bo w procesie potomnym nie możemy już wypisać błędu do terminala uruchamiającego proces rodzicielski
    else if(chdir("/") == -1) // ustawiamy katalog roboczy procesu na /
        ret = -6;
    else
    {
        int i;
        for(i = 0; i <= 2; ++i) // obowiązkowo zamykamy stdin, stdout, stderr (deskryptory 0, 1, 2)
            if(close(i) == -1) // jeżeli nie uda się zamknąć któregoś deskryptora
            {
                ret = -(50 + i);
                break;
            }
    }
    if(ret >= 0)
    {
        int i;
        // jeżeli są otwarte, to zamykamy dalsze deskryptory - od 3 do 1023, bo domyślnie w Linuxie proces może mieć otwarte maksymalnie 1024 deskryptory
        for(i = 3; i <= 1023; ++i)
            close(i); // jeżeli nie uda się zamknąć któregoś deskryptora, to ignorujemy błąd
        sigset_t set;
        // przeadresowujemy deskryptory 0, 1, 2 na /dev/null
        if(open("/dev/null", O_RDWR) == -1) // deskryptor 0 (stdin; najmniejszy wolny) wskazuje teraz na /dev/null
            ret = -8;
        else if(dup(0) == -1) // deskryptor 1 (stdout) wskazuje teraz na to samo co deskryptor 0 - na /dev/null
            ret = -9;
        else if(dup(0) == -1) // deskryptor 2 (stderr) wskazuje teraz na to samo co deskryptor 0 - na /dev/null
            ret = -10;
        // jeżeli nie wystąpił błąd, to w tym momencie proces potomny jest już demonem
        else if(signal(SIGUSR1, sigusr1Handler) == SIG_ERR) // rejestrujemy funkcji obsługującą sygnał SIGUSR1
            ret = -11;
        else if(signal(SIGTERM, sigtermHandler) == SIG_ERR) // rejestrujemy funkcji obsługującą sygnał SIGTERM
            ret = -12;
        else if(sigemptyset(&set) == -1) // inicjujemy zbiór sygnałów
            ret = -13;
        else if(sigaddset(&set, SIGUSR1) == -1) // dodajemy SIGUSR1 do zbioru sygnałów
            ret = -14;
        else if(sigaddset(&set, SIGTERM) == -1) // dodajemy SIGTERM do zbioru sygnałów
            ret = -15;
        else
        {
            size_t sourcePathLength = strlen(sourcePath);
            if(sourcePath[sourcePathLength - 1] != '/') // jeżeli bezpośrednio przed null terminatorem nie ma '/'
                stringAppend(sourcePath, sourcePathLength++, "/"); // wstawiamy '/' na miejscu null terminatora; zwiększamy długość ścieżki o 1; wstawiamy null terminator za '/'
            size_t destinationPathLength = strlen(destinationPath);
            if(destinationPath[destinationPathLength - 1] != '/')
                stringAppend(destinationPath, destinationPathLength++, "/");
            synchronizer synchronize;
            if(recursive == 0)
                synchronize = synchronizeNonRecursively;
            else
                synchronize = synchronizeRecursively;
            stop = 0;
            forcedSynchronization = 0;
            while(1)
            {
                if(forcedSynchronization == 0) // jeżeli nie wymuszono synchronizacji sygnałem SIGUSR1, to możemy spać
                {
                    openlog("DirSyncD", LOG_ODELAY | LOG_PID, LOG_DAEMON); // otwieramy połączenie z logiem /var/log/syslog
                    syslog(LOG_INFO, "uspienie");
                    closelog();
                    unsigned int timeLeft = sleep(interval); // usypiamy demona
                    openlog("DirSyncD", LOG_ODELAY | LOG_PID, LOG_DAEMON);
                    syslog(LOG_INFO, "obudzenie; przespano %u s", interval - timeLeft);
                    closelog();
                    if(stop == 1)
                        break;
                }
                if(sigprocmask(SIG_BLOCK, &set, NULL) == -1) // włączamy blokowanie sygnałów ze zbioru: SIGUSR1 i SIGTERM
                {
                    ret = -16;
                    break;
                }
                int status = synchronize(sourcePath, sourcePathLength, destinationPath, destinationPathLength);
                openlog("DirSyncD", LOG_ODELAY | LOG_PID, LOG_DAEMON); // otwieramy połączenie z logiem /var/log/syslog
                syslog(LOG_INFO, "koniec synchronizacji; %i", status);
                closelog();
                forcedSynchronization = 0;
                if(sigprocmask(SIG_UNBLOCK, &set, NULL) == -1) // wyłączamy blokowanie sygnałów ze zbioru: SIGUSR1 i SIGTERM
                {
                    ret = -17;
                    break;
                }
                // jeżeli podczas synchronizacji odebraliśmy SIGUSR1 lub SIGTERM, to po wyłączeniu ich blokowania zostaną wykonane funkcje ich obsługi
                // jeżeli podczas synchronizacji odebraliśmy SIGUSR1, to po jej zakończeniu od razu zostanie wykonana kolejna synchronizacja
                // jeżeli podczas synchronizacji nie odebraliśmy SIGUSR1 i odebraliśmy SIGTERM, to po jej zakończeniu demon się zakończy
                // jeżeli podczas synchronizacji odebraliśmy SIGUSR1 i SIGTERM, to po jej zakończeniu najpierw zostanie wykonana kolejna synchronizacja, a jeżeli podczas niej nie odbierzemy SIGUSR1, to po niej demon się zakończy; jeżeli jednak podczas tej drugiej synchronizacji odbierzemy SIGUSR1, to zostanie wykonana trzecia synchronizacja, itd.; po zakończeniu pierwszej synchronizacji, podczas której nie odbierzemy SIGUSR1, demon się zakończy
                if(forcedSynchronization == 0 && stop == 1)
                    break;
            }
        }
    }
    if(sourcePath != NULL)
        free(sourcePath);
    if(destinationPath != NULL)
        free(destinationPath);
    openlog("DirSyncD", LOG_ODELAY | LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "zakonczenie %i", ret);
    closelog();
    exit(ret); // zamykamy proces demona
}