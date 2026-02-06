## Lab00 - Review C - Defensive Code
Spring 2026
Minh Pham

### Part 1: 
- `scanf` is dangerous for fix-sized struct because it keeps reading as the user inputs, which can cause buffer overflow.
- `fgets` is safer for fix-sized characters because it ensures the buffer length.

### Part 2:
- We can use `Valgrind` to check for memory leaks, showing whether all dynamically allocated memory was freed and no memory errors were detected.  