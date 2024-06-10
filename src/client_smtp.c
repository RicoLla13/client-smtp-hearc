#include <stdio.h>

int main(int argc, char* argv[]) {
    if(argc > 1) {
        for (int i = 1; i < argc; i++) {
            printf("Argument %d: %s\n", i, argv[i]);
        }
    } else {
        printf("No arguments provided\n");
    }

    return 0;
}
