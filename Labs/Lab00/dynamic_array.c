#include <stdio.h>
#include <stdlib.h>

struct IntArray {
    int *data;          
    size_t size;        
    size_t capacity;    
};

void add_element(struct IntArray *a, int value) {
    if (a == NULL) return;

    if (a->size == a->capacity) {
        size_t new_capacity = (a->capacity == 0) ? 1 : a->capacity * 2;

        int *tmp = (int *)realloc(a->data, new_capacity * sizeof(int));
        if (tmp == NULL) {
            fprintf(stderr, "Error: realloc failed while growing array.\n");
            return;
        }

        a->data = tmp;
        a->capacity = new_capacity;
    }

    a->data[a->size] = value; 
    a->size++;                
}

int get_element(struct IntArray *a, size_t index, int *error) {
    if (error) *error = 0;

    if (a == NULL || a->data == NULL) {
        if (error) *error = 1;
        return -1;
    }

    if (index > a->size) {
        if (error) *error = 1;
        return -1;
    }

    if (index >= a->size) {
        if (error) *error = 1;
        return -1;
    }

    return a->data[index];
}

void free_array(struct IntArray *a) {
    if (a == NULL) return;
    free(a->data);
    a->data = NULL;
    a->size = 0;
    a->capacity = 0;
}

int main(void) {
    struct IntArray a;
    a.size = 0;
    a.capacity = 10;
    a.data = (int *)malloc(a.capacity * sizeof(int));
    if (a.data == NULL) {
        fprintf(stderr, "Error: malloc failed.\n");
        return 1;
    }

    // Add 1 through 100
    for (int i = 1; i <= 100; i++) {
        add_element(&a, i);
    }

    long long sum = 0;
    for (size_t i = 0; i < a.size; i++) {
        int err = 0;
        int v = get_element(&a, i, &err);
        if (err) {
            fprintf(stderr, "Error: invalid access at index %zu\n", i);
            free_array(&a);
            return 1;
        }
        sum += v;
    }

    printf("%lld\n", sum);
    free_array(&a);
    return 0;
}
