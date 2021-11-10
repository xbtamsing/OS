#include <stdio.h>
#include <stdlib.h>

void printUsage(char **argv) {
    printf("USAGE: %s filename\n",argv[0]);
    exit(1);
}

void printFileAccessError(char **argv) {
    perror(argv[1]);
    exit(2);
}

int countSetBits(int c) {
    int bitCount = 0;
    while(c) {
        bitCount += (c & 1);
        c >>= 1;
    }
    return bitCount;
}


int main(int argc, char **argv) {
    if (argc != 2) {
        printUsage(argv);
    }

    FILE *inputFile = fopen(argv[1], "r");

    if (inputFile == NULL) {
        printFileAccessError(argv);
    }

    int c;
    int bitCount = 0;

    while ((c = fgetc(inputFile)) != EOF) {
        bitCount += countSetBits(c);
    }

    fclose(inputFile);

    printf("%d\n", bitCount);

    return 0;
}