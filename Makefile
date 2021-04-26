all: build/DirSyncD

build/DirSyncD: build/Lukasz.o
	cd build; \
	gcc -o DirSyncD Lukasz.o

build/Lukasz.o: Lukasz.c
	gcc -c Lukasz.c -o build/Lukasz.o

clean:
	cd build; \
	rm DirSyncD Lukasz.o