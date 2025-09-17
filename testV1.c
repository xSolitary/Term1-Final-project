#include <stdio.h>
#include <stdlib.h>


int main() {
    char line[1000];
    FILE *file = fopen("orders.csv", "r");
    if (file == NULL)
    {
        printf("File: %p\n", file);
        printf("fail to open file\n");
    }
    else
    {
        printf("File: %p\n", file);
        printf("sucsess to open file\n");
    }
   
    while (fgets(line, sizeof(line), file)) {
        printf("%s", line);
    }
    

    fclose(file);
}