all: build/DirSyncD

build/DirSyncD: build/main.o build/Anna.o build/Lukasz.o build/Mariusz.o
	cd build; \
	gcc -o DirSyncD main.o Anna.o Lukasz.o Mariusz.o

build/main.o: main.c
	gcc -c main.c -o build/main.o

build/Anna.o: Anna.c
	gcc -c Anna.c -o build/Anna.o

build/Lukasz.o: Lukasz.c
	gcc -c Lukasz.c -o build/Lukasz.o

build/Mariusz.o: Mariusz.c
	gcc -c Mariusz.c -o build/Mariusz.o

clean:
	cd build; \
	rm DirSyncD main.o Anna.o Lukasz.o Mariusz.o