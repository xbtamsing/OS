#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define BUCKET_COUNT 256

hash_table *table;
pthread_mutex_t bucket_mutexes[BUCKET_COUNT];

int hash_code(char *str) {
    int str_value = 0;
    for (int i = 0; i < strlen(str); i++) {
        str_value += (int) str[i];
    }
    int hash_code = str_value % BUCKET_COUNT;

    return hash_code;
}

void mutex_init() {
    for (int i = 0; i < BUCKET_COUNT; i++) {
        pthread_mutex_init(&bucket_mutexes[i], NULL);
    }
}

typedef struct node {
    char str[50];
    struct node *next;
} node;

typedef struct list {
    node *head;
} list;

list *list_init() {
    list *l = (list *) malloc(sizeof(list));
    l->head = NULL;

    return l;
}

void list_insert(list *list, char *str) {
    node *new = (node *) malloc(sizeof(node));
    if (new == NULL) {
        perror("malloc");
        return;
    }

    strcpy(new->str, str);

    if (list->head == NULL) {
        new->next = NULL;
    } else {
        new->next = list->head;
    }

    list->head = new;
}

int list_search(list *list, char *str) {
    int retval = -1;

    node *current = list->head;
    while(current) {
        if (strcmp(current->str, str) == 0) {
            retval = 0;
            break;
        }
        current = current->next;
    }
    return retval;
}

int count_list(list *list) {
    int count = 0;

    node *current = list->head;
    while (current) {
        count++;
        current = current->next;
    }

    return count;
}

typedef struct hash_table {
    list *lists[BUCKET_COUNT];
} hash_table;

hash_table *hash_table_init() {
    hash_table *table = (hash_table *) malloc(sizeof(hash_table));
    for (int i = 0; i < BUCKET_COUNT; i++) {
        table->lists[i] = list_init();
    }
    return table;
}

void hash_insert(hash_table *table, char *str) {
    int index = hash_code(str);

    pthread_mutex_lock(&bucket_mutexes[index]);
    list_insert(table->lists[index], str);
    pthread_mutex_unlock(&bucket_mutexes[index]);
}

int hash_search(hash_table *table, char *str) {
    int index = hash_code(str);
    int retval;

    pthread_mutex_lock(&bucket_mutexes[index]);
    retval = list_search(table->lists[index], str);
    pthread_mutex_unlock(&bucket_mutexes[index]);

    return retval;
}

int hash_dump(hash_table *table) {
    int count = 0;

    for (int i = 0; i < BUCKET_COUNT; i++) {
        count += count_list(table->lists[i]);
    }

    return count;
}

void *read_unique(void *file_name) {
    file_name   = (char *)file_name;
    FILE *fh    = fopen(file_name, "r");

    if (!fh) {
        perror(file_name);
        return NULL;
    }

    char *str;
    while (fscanf(fh, "%ms", &str) != EOF) {
        if (hash_search(table, str) == -1) {
            hash_insert(table, str);
        }
        free(str);
    }

    return NULL;
}

int main(int argc, char **argv) {
    table = hash_table_init();
    mutex_init();

    pthread_t threads[argc];
    for (int i = 1; i < argc; i++) {
        pthread_t p = 0;
        threads[i - 1]  = p;
    }

    for (int i = 1; i < argc; i++) {
        pthread_create(&threads[i - 1], NULL, read_unique,argv[i]);
    }

    for (int i = 1; i < argc; i++) {
        pthread_join(threads[i - 1], NULL);
    }

    int total = hash_dump(table);
    printf("%d\n", total);

    for (int i = 0; i < BUCKET_COUNT; i++) {
        free(table->lists[i]);
    }
    free(table);

    return 0;
}




