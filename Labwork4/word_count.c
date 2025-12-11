#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    char word[100]; 
    int count; 
} KeyValue;

KeyValue *intermediate_data = NULL;
int intermediate_count = 0;
int intermediate_capacity = 0;

#define INITIAL_CAPACITY 1000

void mapper(char *line) {
    char *token;
    char delim[] = " \t\n\r,.!?;:()[]\"'/\\";

    token = strtok(line, delim);
    while (token != NULL) {
        for (int i = 0; token[i]; i++) {
            token[i] = tolower((unsigned char)token[i]);
        }

        if (strlen(token) > 0) {
            if (intermediate_count >= intermediate_capacity) {
                intermediate_capacity = (intermediate_capacity == 0) ? INITIAL_CAPACITY : intermediate_capacity * 2;
                intermediate_data = (KeyValue *)realloc(intermediate_data, intermediate_capacity * sizeof(KeyValue));
                if (intermediate_data == NULL) {
                    perror("Error reallocating memory");
                    exit(EXIT_FAILURE);
                }
            }

            strcpy(intermediate_data[intermediate_count].word, token);
            intermediate_data[intermediate_count].count = 1;
            intermediate_count++;
        }

        token = strtok(NULL, delim);
    }
}

int compare_keys(const void *a, const void *b) {
    return strcmp(((KeyValue *)a)->word, ((KeyValue *)b)->word);
}

void reducer() {
    printf("WORD COUNT (OUTPUT OF REDUCER)\n");

    if (intermediate_count == 0) {
        printf("No word founds\n");
        return;
    }

    // Shuffle & Sort 
    qsort(intermediate_data, intermediate_count, sizeof(KeyValue), compare_keys);

    // Reduce
    int i = 0;
    while (i < intermediate_count) {
        char current_word[100];
        strcpy(current_word, intermediate_data[i].word);
        int total_count = 0;

        // Reduce Logic
        int j = i;
        while (j < intermediate_count && strcmp(intermediate_data[j].word, current_word) == 0) {
            total_count += intermediate_data[j].count;
            j++;
        }

        printf("<%s, %d>\n", current_word, total_count);

        i = j;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uses: %s <tên_file_input>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *file = fopen(argv[1], "r");
    if (file == NULL) {
        perror("Cannot open file input");
        return EXIT_FAILURE;
    }

    char line[1024];

    while (fgets(line, sizeof(line), file) != NULL) {
        char *line_copy = strdup(line); 
        if (line_copy == NULL) {
             perror("Error");
             fclose(file);
             free(intermediate_data);
             return EXIT_FAILURE;
        }
        mapper(line_copy);
        free(line_copy);
    }
    
    fclose(file);

    reducer();

    free(intermediate_data);
    
    return 0;
}