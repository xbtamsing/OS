#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define BUCKET_COUNT 256
#define COUNTER_T_COUNT 4

int hash_code(char *str) {
   int str_value = 0;
   for (int i = 0; i < strlen(str); i++) {
       str_value += (int) str[i];
   }
   int hash_code = str_value % BUCKET_COUNT;

   return hash_code;
}

int get_queue_num(const char *str) {
   return (str[0] & 0b11);
}

typedef struct node {
   char str[50];
   int count;
   struct node *next;
} node;

typedef struct {
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
   new->count = 1;

   if (list->head == NULL) {
       new->next = NULL;
   } else {
       new->next = list->head;
   }

   list->head  = new;
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

typedef struct {
   list *lists[BUCKET_COUNT];
   int max_count;
} hash_table;

hash_table *hash_table_init() {
   hash_table *table = (hash_table *) malloc(sizeof(hash_table));
   for (int i = 0; i < BUCKET_COUNT; i++) {
       table->lists[i] = list_init();
   }
   table->max_count = 0;
   return table;
}

void hash_insert(hash_table *table, char *str) {
   int index = hash_code(str);

   if (1 > table->max_count) {
       table->max_count = 1;
   }

   list_insert(table->lists[index], str);
}

int hash_search(hash_table *table, char *str) {
   int index = hash_code(str);
   int retval;

   retval = list_search(table->lists[index], str);

   return retval;
}

int hash_dump(hash_table *table) {
   int count = 0;
   for (int i = 0; i < BUCKET_COUNT; i++) {
       count += count_list(table->lists[i]);
   }
   return count;
}

typedef struct {
   node *head;
   node *tail;
   pthread_mutex_t head_lock, tail_lock;
} queue;

queue *queue_init() {
   queue *q = (queue *)malloc(sizeof(queue));
   if (q == NULL) {
       perror("malloc");
   }

   q->head = q->tail = NULL;

   pthread_mutex_init(&q->head_lock, NULL);
   pthread_mutex_init(&q->tail_lock, NULL);

   return q;
}

void enqueue(queue *q, const char *str) {
   node *new = (node *)malloc(sizeof(node));
   if (new == NULL) {
       perror("malloc");
   }

   new->next = NULL;
   strcpy(new->str, str);
   new->count = 0;

   if (q->head == NULL) {
       q->head = new;
       q->tail = q->head;
   }
   else {
       q->tail->next = new;
       q->tail = new;
   }

   printf("enqueued %s\n", str);
}

char *dequeue(queue *q) {
   char *str = "";
   node *temp = q->head;

   if (!temp) {
       str = temp->str;
       printf("dequeued %s\n", str);
       q->head = q->head->next;
   }
   free(temp);

   return str;
}

typedef struct {
   pthread_t thread;
   queue *q;
   hash_table  *table;
   int max_count;
} counter_t;


counter_t *counter_t_init() {
   counter_t *t = (counter_t *)malloc(sizeof(counter_t));
   if (t == NULL) {
       perror("malloc");
   }

   t->q = queue_init();
   t->table = hash_table_init();
   t->max_count = 0;

   return t;
}

counter_t *counter_threads[4];
pthread_cond_t filled[4];
int done[4] = { 0,0,0,0 };
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
   counter_t *thread;
   int num;
} counter_args;

void increment_count(hash_table *table, char *str) {
   int index = hash_code(str);

   node *current = table->lists[index]->head;
   while (current) {
       if (strcmp(current->str, str) == 0) {
           current->count += 1;

           if (current->count > table->max_count) {
               table->max_count = current->count;
           }
           break;
       }
       else { current = current->next; }
   }
}

void *count_popular(void *arg) {
   counter_args *arguments = (counter_args *)arg;
   counter_t *counter = arguments->thread;

   pthread_mutex_lock(&counter->q->head_lock);
   while (done[arguments->num] == 0) {
       pthread_cond_wait(&filled[arguments->num], &counter->q->head_lock);
   }

   char *str = dequeue(counter->q);
   pthread_mutex_unlock(&counter->q->head_lock);

   switch (hash_search(counter->table, str)) {
       case -1:
           hash_insert(counter->table, str);
           break;
       case 0:
           increment_count(counter->table, str);
           break;
   }
   counter->max_count = counter->table->max_count;

   return NULL;
}

void *shuttle(void *file_name) {
   file_name = (char *)file_name;
   FILE *fh = fopen(file_name, "r");

   if (!fh) {
       perror(file_name);
       return NULL;
   }

   char str[50] = "";
   int queue_num = 0;
   while (fscanf(fh, "%s", str) != EOF) {
       queue_num = get_queue_num(str);

       pthread_mutex_lock(&counter_threads[queue_num]->q->tail_lock);
       enqueue(counter_threads[queue_num]->q, str);
       pthread_mutex_unlock(&counter_threads[queue_num]->q->tail_lock);
   }
   free(str);

   pthread_mutex_lock(&m);
   done[queue_num] = 1;
   pthread_cond_signal(&filled[queue_num]);
   pthread_mutex_unlock(&m);

   return NULL;
}

int main(int argc, char **argv) {
   for (int i = 0; i < COUNTER_T_COUNT; i++) {
       pthread_cond_init(&filled[i], NULL);
   }

   pthread_t threads[argc];
   for (int i = 1; i < argc; i++) {
       pthread_t p = 0;
       threads[i - 1]  = p;
   }

   // producers
   for (int i = 1; i < argc; i++) {
       // shuttling...
       pthread_create(&threads[i - 1], NULL, shuttle, argv[i]);
   }

   // consumers
   for (int i = 0; i < COUNTER_T_COUNT; i++) {
       counter_threads[i] = counter_t_init();
       counter_args arguments = {counter_threads[i], i};
       pthread_create(&counter_threads[i]->thread, NULL, count_popular, (void *)&arguments);
   }

   for (int i = 1; i < argc; i++) {
       pthread_join(threads[i - 1], NULL);
   }
   for (int i = 0; i < COUNTER_T_COUNT; i++) {
       pthread_join(counter_threads[i]->thread, NULL);
   }

   for (int i = 0; i < COUNTER_T_COUNT; i++) {
       for (int j = 0; j < BUCKET_COUNT; j++) {
           node *current = counter_threads[i]->table->lists[j]->head;
           while (current) {
               if (current->count == counter_threads[i]->max_count) {
                   printf("%s %d\n", current->str, counter_threads[i]->max_count);
               }
               current = current->next;
           }
       }
   }

   for (int i = 0; i < COUNTER_T_COUNT; i++) {
       for (int j = 0; j < BUCKET_COUNT; j++) {
           free(counter_threads[i]->table->lists[j]);
       }
       free(counter_threads[i]->table);
       free(counter_threads[i]);
   }

   return 0;
}

