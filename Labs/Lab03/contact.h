#ifndef CONTACT_H
#define CONTACT_H

#define EMAIL_LEN 128        // Maximum length for an email address.
#define PHONE_LEN 20         // Maximum length for a phone number.

// Structure to hold contact information.
typedef struct Contact {
    char *first_name;
    char *last_name;
    char email[EMAIL_LEN+1];
    char phone[PHONE_LEN+1];
    char *city;
    char *country;
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

struct contactlist_node * load_csv(const char *filename, const char* sep, int* num_contacts);
void free_contactlist(struct contactlist_node *head);

#endif // CONTACT_H