#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define WORDS 20

const char *word_list[WORDS] = {"hello", "world", "data", "text", "file", "generate", "code", "example", "program", "data",
                                "random", "words", "list", "array", "function", "variable", "constant", "loop", "condition", "syntax"};

void generate_text_file(int size_kb) {
    char filename[50];
    sprintf(filename, "test_%dKB.txt", size_kb);

    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        printf("Error creating file!\n");
        return;
    }

    int size_bytes = size_kb * 1024;
    int written = 0;

    srand(time(NULL));

    while (written < size_bytes) {
        const char *word = word_list[rand() % WORDS];
        int len = strlen(word);

        if (written + len + 1 > size_bytes) break;

        fprintf(file, "%s ", word);
        if (rand () % 20 == 0)
        {
            fprintf(file, "\n");
            written += 1;
        }

        written += len + 1;
    }

    fclose(file);
    printf("File '%s' generated successfully!\n", filename);
}

int main() {
    int x;
    printf("Enter the size of the text file in KB: ");
    scanf("%d", &x);

    if (x <= 0) {
        printf("Invalid size!\n");
        return 1;
    }

    generate_text_file(x);
    return 0;
}
