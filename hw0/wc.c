#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>




int* myWC(FILE* inputFile) {
    int bytes = 0, words = 0, lines  = 0;
    char curr;
    char prev = ' ';

    while(curr = fgetc(inputFile)){
        if(!isspace(prev)) {
            if (isspace(curr) || (bytes > 0 && curr == EOF)){
                words ++;
            }
        }
        if(curr == '\n'){
            lines ++;
        }

        if (curr == EOF) {
            break;
        }

        bytes++;
        prev = curr;
    }

    int *rets; 
    rets = (int *) malloc(sizeof(int)*3);
    if (rets != NULL) {
        rets[0] = lines;
        rets[1] = words;
        rets[2] = bytes;
    }
    return rets;
}

int main(int argc, char *argv[]) {
    FILE* inputFile;
    if (argc > 2) {
        printf("Number of arguments not supproted sorry :(");
        return 0;
    } 
    if (argc == 2) {
        inputFile = fopen(argv[1], "r");
        if (!inputFile) {
            printf("Invalid File");
            return 0;
        }
    }

    int *rets = myWC(argc == 1 ? stdin : inputFile);
    printf("%d %d %d %s", rets[0], rets[1], rets[2], argc == 1 ? "" : argv[1]);
    free(rets);
    return 0;

}

