#include "contact.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


#define MAX_LINE_LENGTH 1024 // Maximum characters in a line from the input file.

// Global buffers for reading and processing file lines.
char line[MAX_LINE_LENGTH];

/**
 * @brief A helper function to safely get the next token from a string.
 *
 * This is a wrapper around `wcstok` for robust tokenizing of strings.
 * It exits on failure, as a missing token indicates a malformed input file.
 *
 * @param str The string to tokenize on the first call, or NULL on subsequent calls.
 * @param sep A string containing the delimiter characters.
 * @param msg An error message to display if tokenizing fails.
 * @return A pointer to the next token.
 */
char* get_token(char** str, const char* sep, char* msg){
    // strsep is similar to strtok but a more modern replacement
    // it is thread safe and also properly handles empty tokens
    char* token = strsep(str, sep);
    if (token == NULL) {
        // If NULL, it means no more tokens could be found.
        // For this program, that implies a malformed line in the CSV file.
        // We print an error and exit to prevent further processing errors.
        fprintf(stderr, "Error parsing line, missing token for: %s\n", msg);
        exit(EXIT_FAILURE);
    }
    return token;
}

/**
 * @brief Parses a single line of text into a Contact struct.
 *
 * @param line The string to parse.
 * @param sep The delimiter string.
 * @param contact A pointer to a Contact struct to store the parsed data.
 */
void parse_contact(char* line, const char* sep, Contact* contact) {
    // the pointer to the line is advanced by strsep
    char **lptr = &line;
    // Parse each token and populate the contact struct.
    contact->first_name = strdup(get_token(lptr, sep, "first_name"));
    contact->last_name = strdup(get_token(lptr, sep, "last_name"));
    strncpy(contact->email, get_token(lptr, sep, "email"), sizeof(contact->email));
    contact->email[EMAIL_LEN] = 0;
    strncpy(contact->phone, get_token(lptr, sep, "phone"), sizeof(contact->phone));
    contact->phone[PHONE_LEN] = 0;

    contact->city = strdup(get_token(lptr, sep, "city"));
    contact->country = strdup(get_token(lptr, sep, "country"));

    //TODO all strdup fields need to be checked for null

    // The last field in the CSV often has a newline, which we need to remove.
    size_t len = strlen(contact->country);
    if (len > 0 && contact->country[len - 1] == L'\n') {
        contact->country[len - 1] = L'\0';
    }
}

/**
 * @brief Loads contact data from a CSV file into a dynamically allocated linked list.
 *
 * @param filename The path to the CSV file.
 * @param sep The delimiter string (e.g., L"|").
 * @param num_contacts A pointer to an integer that will be updated with the total
 *                     number of contacts loaded.
 * @return A pointer to the head of the newly created linked list, or NULL on failure.
 */
struct contactlist_node * load_csv(const char *filename, const char* sep, int* num_contacts){
    FILE *file;
    struct contactlist_node *head = NULL;    // Pointer to the first node in the list.
    struct contactlist_node *current = NULL; // Pointer to the current last node, used for appending.

    *num_contacts = 0; // Initialize the count of contacts.

    // Open the file in "read" mode.
    file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file"); // perror prints a system error message (e.g., "No such file or directory").
        return NULL;
    }

    // The first line of a CSV is often a header. We read and discard it.
    // `fgets` reads one line (or until the buffer is full) from the file.
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file); // Always close the file, even on error.
        return NULL;  // File is empty or contains only an empty first line.
    }

    // Loop through the rest of the file, processing one line at a time.
    while (fgets(line, sizeof(line), file) != NULL) {
        // printf("got line: %s\n", line);

        // --- DYNAMIC MEMORY ALLOCATION ---
        // Here, we allocate memory from the heap for our new data. This memory
        // persists until it is explicitly freed.

        // 1. Allocate memory for a new linked list node.
        // `calloc` is used here because it allocates memory AND initializes it to
        // zero. This is useful as it automatically sets the `next` pointer to NULL.
        struct contactlist_node *new_node = (struct contactlist_node*)calloc(1, sizeof(struct contactlist_node));

        // 2. Allocate memory for the Contact struct that the node will point to.
        // `malloc` just allocates the memory; its contents are uninitialized.
        new_node->c = (Contact*)malloc(sizeof(Contact));

        if (new_node == NULL || new_node->c == NULL) {
            // If memory allocation fails, free any allocated memory and return NULL.
            perror("Memory allocation failed\n");
            continue;
        }

        // Now that memory is allocated, parse the line and populate the Contact struct.
        parse_contact(line, sep, new_node->c);

        // --- LINKED LIST MANAGEMENT ---
        // Add the newly created and populated node to the end of our list.
        if (head == NULL) {
            // If the list is empty, this new node becomes the first node (the head).
            head = new_node;
            current = head;
        } else {
            // Otherwise, link the current last node to our new node...
            current->next = new_node;
            // ...and then update `current` to point to the new end of the list.
            current = new_node;
        }
        (*num_contacts)++; // Increment the contact counter.
    }

    printf("Successfully read %d contacts from %s.\n", *num_contacts, filename);

    // It is crucial to close the file stream to release system resources.
    fclose(file);

    return head; // Return the pointer to the start of the list.
}

/**
 * @brief Frees all dynamically allocated memory for the contact list.
 *
 * This is a critical function to prevent memory leaks. A memory leak occurs
 * when a program allocates memory on the heap but loses all pointers to it,
 * making it impossible to free. For every `malloc`, `calloc`, or `wcsdup`,
 * there must be a corresponding `free`.
 *
 * @param head A pointer to the head of the list to be freed.
 */
void free_contactlist(struct contactlist_node *head) {
    struct contactlist_node *current = head;
    struct contactlist_node *next;

    while (current != NULL) {
        // We must store the pointer to the next node before we free the current one.
        // If we didn't, we would lose our only link to the rest of the list.
        next = current->next;

        // Free the memory in the reverse order of allocation (from the inside out).
        // 1. Free the strings inside the Contact struct (allocated with wcsdup).
        free(current->c->first_name);
        free(current->c->last_name);
        free(current->c->city);
        free(current->c->country);

        // 2. Free the Contact struct itself (allocated with malloc).
        free(current->c);

        // 3. Free the linked list node (allocated with calloc).
        free(current);

        // Move to the next node to continue the process.
        current = next;
    }
}