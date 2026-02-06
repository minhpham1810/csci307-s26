// =================================================================================
// CSCI307: C Programming and Unix
// Lab 01: C and Dynamic Memory
// (c) 2025 Marchiori
// Most of the comments are AI generaetd, human checked. :)
//
// This program is designed to teach students about fundamental C programming
// concepts, including:
//  - File I/O: Reading data from a text file (CSV).
//  - Dynamic Memory Allocation: Using malloc, calloc, and free.
//  - Data Structures: Implementing and managing a linked list.
//  - String Manipulation: Working with standard character strings.
//  - Sorting: Using the standard library's qsort function with a custom
//    comparator.
// =================================================================================

// The #include directive is used to import functionality from C's standard
// libraries.
#include <stdio.h>      // For standard input/output functions (e.g., printf, fopen).
#include <stdlib.h>     // For memory allocation (malloc, free), and exit().
#include <string.h>     // For string manipulation functions (e.g., strcpy, strcmp).

// Define global constants to avoid "magic numbers" in the code.
// This makes the code more readable and easier to maintain.
#define MAX_LINE_LENGTH 1024 // Maximum number of characters in a single line of the input file.
#define EMAIL_LEN 128        // Maximum length for an email address.
#define PHONE_LEN 20         // Maximum length for a phone number.

// The `typedef` keyword creates an alias for a data type. Here, we define
// `Contact` as an alias for `struct Contact`. This means we can write `Contact`
// instead of `struct Contact` throughout the code, making it more concise.
typedef struct Contact {
    // These fields are pointers (char *), which means they will store memory
    // addresses. We will dynamically allocate memory on the heap for the actual
    // string data using strdup(). This is necessary for data of variable length.
    char *first_name;
    char *last_name;

    // Pointers for city and country, which can have variable lengths and may
    // contain international characters.
    char *city;
    char *country;

    // These fields are fixed-size character arrays. The memory for them is
    // allocated directly within the struct itself (on the stack or heap,
    // wherever the struct is created). This is suitable for data with a
    // known, limited length like emails and phone numbers.
    // add one byte for the null terminator
    char email[EMAIL_LEN+1];
    char phone[PHONE_LEN+1];

} Contact;

// A node for our linked list. A linked list is a data structure where elements
// are linked together in a sequence. Each node contains:
// 1. A pointer to the data it holds (in this case, a Contact struct).
// 2. A pointer to the next node in the list. The last node's `next` pointer
//    will be NULL, indicating the end of the list.
struct contactlist_node {
  Contact *c;
  struct contactlist_node *next;
};

/**
 * @brief A helper function to safely get the next token from a string.
 *
 * This function is a wrapper around `strsep`, it breaks a string into a 
 * series of tokens based on a delimiter. It is "destructive" as it modifies the 
 * string it's tokenizing by inserting null terminators.
 * `strsep` is a more modern version of strtok. It is thread safe and handles
 * empty tokens (strtok does not).
 *
 * @param str The string to tokenize on the first call, or NULL on subsequent calls
 *            to continue tokenizing the same string.
 * @param sep A string containing the delimiter characters (e.g., "|").
 * @param state A pointer to a char*, used internally by strtok_r to maintain its
 *              position in the string between calls.
 * @param msg An error message to display if tokenizing fails.
 * @return A pointer to the next token. The program exits if no token is found,
 *         as this indicates a formatting error in the input file.
 */
char* get_token(char** str, char* sep, char* msg){
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
 * @param sep The delimiter string (e.g., "|").
 * @param contact A pointer to a Contact struct where the parsed data will be stored.
 * @return A pointer to the populated Contact struct.
 */
Contact* parse_contact(char* line, char* sep, Contact* contact) {
    // the pointer to the line is advanced by strsep
    char **lptr = &line;
    // For fields that are pointers, we must dynamically allocate new
    // memory on the heap to store the string data. `strdup` (string
    // duplicate) is a convenient function for this. It:
    //   1. Allocates the exact amount of memory needed for the string.
    //   2. Copies the content of the token into this new memory block.
    //   3. Returns a pointer to the new memory block.
    // This allocated memory MUST be freed later using free() to prevent memory leaks.
    // note: the first call to strtock needs the pointer to the string,
    // it will populate the state variable, to get the remaining tokens the
    // first parameter is NULL to continue parsing tokens from the line.
    contact->first_name = strdup(get_token(lptr, sep, "first_name"));
    contact->last_name = strdup(get_token(lptr, sep, "last_name"));

    // For fields that are fixed-size arrays (char[]), we do not allocate new memory.
    // (the memory is already alocated in the structure).
    // Instead, we copy the token data into the existing array within the struct.
    // We provide the size of the destination buffer to prevent buffer overflows.
    strncpy(contact->email, get_token(lptr, sep, "email"), EMAIL_LEN);
    strncpy(contact->phone, get_token(lptr, sep, "phone"), PHONE_LEN);

    //ensure null terminators in the email and phone fields.
    contact->email[EMAIL_LEN] = '\0';
    contact->phone[PHONE_LEN] = '\0';  

    // these fields are the same as first/last name.
    contact->city = strdup(get_token(lptr, sep, "city"));
    contact->country = strdup(get_token(lptr, sep, "country"));

    // The last token read from a line often contains a trailing newline character ('\n').
    // We must remove it to ensure the data is clean.
    size_t len = strlen(contact->country);
    if (len > 0 && contact->country[len - 1] == '\n') {
        contact->country[len - 1] = '\0'; // Overwrite '\n' with a null terminator.
    }

    return contact;
}

/**
 * @brief Loads contact data from a CSV file into a dynamically allocated linked list.
 *
 * @param filename The path to the CSV file.
 * @param sep The delimiter string (e.g., "|").
 * @param num_contacts A pointer to an integer that will be updated with the total
 *                     number of contacts loaded.
 * @return A pointer to the head of the newly created linked list, or NULL on failure.
 */
struct contactlist_node * load_csv(const char *filename, char* sep, int* num_contacts){
    FILE *file;
    struct contactlist_node *head = NULL;    // Pointer to the first node in the list.
    struct contactlist_node *current = NULL; // Pointer to the current last node, used for appending.
    char* line = NULL;  // a buffer will be alloacted (heap) to hold the line of text being processed.
    *num_contacts = 0; // Initialize the count of contacts.

    // Open the file in "read" mode.
    file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file"); // perror prints a system error message (e.g., "No such file or directory").
        return NULL;
    }
    line = malloc(MAX_LINE_LENGTH + 1);
    if (line == NULL) {
        perror("Error allocating memory");
        fclose(file);
        return NULL;
    }

    // The first line of a CSV is often a header. We read and discard it.
    // `fgets` reads one line (or until the buffer is full) from the file.
    if (fgets(line, MAX_LINE_LENGTH, file) == NULL) {
        fclose(file); // Always close the file, even on error.
        return NULL;  // File is empty or contains only an empty first line.
    }

    // Loop through the rest of the file, processing one line at a time.
    while (fgets(line, MAX_LINE_LENGTH, file) != NULL) {
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
    free(line);
    return head; // Return the pointer to the start of the list.
}

/**
 * @brief Prints the entire list of contacts in a formatted table.
 *
 * @param list A pointer to the head of the contact list.
 */
void print_contactlist(struct contactlist_node *list) {
    
    printf("%-15s | %-15s | %-25s | %-15s | %-15s | %-15s\n", "First Name", "Last Name", "Email", "Phone", "City", "Country");
    printf("----------------|-----------------|---------------------------|-----------------|-----------------|----------------\n");

    // Traverse the linked list from the head until we reach the end (NULL).
    while(list != NULL) {
        // Print the data for the current contact.
        // `%s` is the format specifier for a character string.
        // `%-15.15ls` means:
        //   - : left-justify the output.
        //   15: ensure the field is at least 15 columns wide (padding with spaces).
        //   .15: print a maximum of 15 characters from the string.
        printf("%-15.15s | %-15.15s | %-25s | %-15s | %-15.15s | %-15.15s\n",
            list->c->first_name,
            list->c->last_name,
            list->c->email,
            list->c->phone,
            list->c->city,
            list->c->country);
        // Move to the next node in the list.
        list = list->next;
    }
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

        // note email and phone numbers are not allocated with wcsdup, so they
        // don't need to be freed!

        // 2. Free the Contact struct itself (allocated with malloc).
        free(current->c);

        // 3. Free the linked list node (allocated with calloc).
        free(current);

        // Move to the next node to continue the process.
        current = next;
    }
}

/**
 * @brief A comparison function for qsort to sort contacts by last name, then first name.
 *
 * This function is required by `qsort`. It takes two generic pointers (`const void *`)
 * and must return:
 *   - A negative value if a < b
 *   - Zero if a == b
 *   - A positive value if a > b
 *
 * @param a A pointer to the first element to compare.
 * @param b A pointer to the second element to compare.
 * @return An integer indicating the relationship between the two elements.
 */
int compare_contact_name(const void *a, const void *b) {
    // We must cast the void pointers back to the correct type.
    // `a` and `b` are pointers to elements in our array, which are of type `Contact *`.
    // So we cast them to `Contact **` and then dereference to get the `Contact *`.
    Contact *contact_a = *(Contact **)a;
    Contact *contact_b = *(Contact **)b;
    
    // compare last names first
    int result = strcmp(contact_a->last_name, contact_b->last_name);

    // If the last names are the same, compare by first name to break the tie.
    if (result == 0) {
        result = strcmp(contact_a->first_name, contact_b->first_name);
    }
    return result;
}

/**
 * @brief Sorts the linked list using the `qsort` standard library function.
 *
 * This function demonstrates a common technique for sorting a linked list:
 * 1. Copy the pointers from the linked list into a temporary array.
 * 2. Sort the array using a fast, standard sorting algorithm like `qsort`.
 * 3. Rebuild the linked list from the sorted array of pointers.
 *
 * @param list The head of the linked list to sort.
 * @param num_contacts The number of nodes in the list.
 * @param compare A function pointer to the comparison function to use for sorting.
 * @return A pointer to the head of the now-sorted linked list.
 */
struct contactlist_node * sort_contactlist(
    struct contactlist_node *list,
    int num_contacts,
    int (*compare)(const void *, const void *)){

    if (num_contacts < 2) {
        return list; // Can't sort a list with 0 or 1 elements.
    }

    // Step 1: Copy the pointers to the Contact structs into a temporary array.
    // We allocate an array of `Contact *` pointers on the heap.
    Contact **array = malloc(num_contacts * sizeof(Contact *));
    struct contactlist_node *current = list;
    for (int i = 0; i < num_contacts; i++) {
        array[i] = current->c;
        current = current->next;
    }

    // Step 2: Sort the array using qsort.
    // `qsort` is a powerful, generic sorting function. It needs:
    //  - The array to sort (array).
    //  - The number of elements in the array (num_contacts).
    //  - The size of each element (sizeof(Contact *)).
    //  - A pointer to a comparison function (`compare`).
    qsort(array, num_contacts, sizeof(Contact *), compare);

    // Step 3: Rebuild the linked list from the sorted array.
    // We don't create new nodes; we just update the `c` pointers in the
    // existing nodes to follow the new sorted order.
    current = list;
    for (int i = 0; i < num_contacts; i++) {
        current->c = array[i];
        current = current->next;
    }

    // Free the temporary array we allocated on the heap.
    free(array);
    return list;
}


/**
 * @brief The main entry point of the program.
 *
 * @param argc The number of command-line arguments passed to the program.
 * @param argv An array of strings, where each string is a command-line argument.
 *             argv[0] is always the name of the program itself.
 * @return EXIT_SUCCESS on successful execution, EXIT_FAILURE on error.
 */
int main(int argc, char** argv) {
    // A simple command-line argument check. The program expects exactly one
    // argument: the name of the file to process.
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // --- Main Program Logic ---
    int num_contacts;

    // 1. Load the data from the specified file into a linked list.
    //    argv[1] contains the first argument passed by the user (the filename).
    struct contactlist_node *list = load_csv(argv[1], "|", &num_contacts);
    if (list == NULL) {
        fprintf(stderr, "Failed to load contacts from file.\n");
        return EXIT_FAILURE;
    }

    // 2. Sort the linked list by name in ascending order.
    list = sort_contactlist(list, num_contacts, compare_contact_name);

    // 3. Print the contents of the sorted linked list to the console.
    print_contactlist(list);

    // 4. Free all the memory that was dynamically allocated during the loading process.
    //    This is a crucial step to prevent memory leaks.
    free_contactlist(list);

    return EXIT_SUCCESS; // Indicate successful execution.
}
