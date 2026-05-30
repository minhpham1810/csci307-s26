# Project 2: Secure Client-Server Authentication & Role-Based Access

This project implements a secure client-server system with robust authentication and role-based access control in C, making extensive use of cryptographic primitives. The system is designed to balance security, flexibility, and usability, and will serve as a hands-on exploration of authentication protocols and modern cryptography.

## Objectives

- **User Authentication**: Establish a challenge-response protocol using SHA-256 hashing with salted passwords and dynamic nonces to ensure that authentication is secure and resistant to replay attacks.
- **Data Security**: All communication after authentication is encrypted using AES-256-CBC to prevent eavesdropping and tampering.
- **Role-Based Access Control (RBAC)**: Users are assigned roles (USER, ADMIN), which restrict or grant access to sensitive commands and system operations.

## Key Features

- **Modular Design**: Clear separation of concerns with distinct modules for cryptographic functions, database management, and protocol definitions.
- **User Database**: Persistent storage of users and their roles with secure handling of password salts and hashes.
- **Extensible Protocol**: Well-defined struct types for user requests, server challenges, and responses facilitate extensibility and maintainability.
- **Post-Authentication Command Loop**: After successful login, authenticated users may execute a suite of allowed commands, with access determined by their role.
- **Security Best Practices**: Leverages OpenSSL for all cryptography and is structured to avoid common security pitfalls.

## Technical Stack

- **Language**: C
- **Cryptography**: OpenSSL (SHA-256, AES-256-CBC)
- **Build System**: Makefile

## Directory Structure

```
Projects/Project02/
  ├─ crypto_utils.c / .h   # SHA-256, AES-256-CBC, helpers
  ├─ db_utils.c / .h       # User database, load/save logic
  ├─ protocol.h            # Protocol structs and definitions
  ├─ user_db.txt           # Persistent user/role data
  └─ [Other project files]
```

## Project Phases

1. **Infrastructure**: Implement cryptographic and database utilities, define data structures for protocol messages.
2. **Authentication**: Implement secure challenge-response protocol between client and server, protecting user credentials.
3. **Encryption & RBAC**: Encrypt all post-auth communication; enforce user roles and restrict command access.
4. **Commands & Polish**: Finalize command loop, implement all required commands, ensure proper build/link with Makefile.

---

This project is a practical exercise in designing and implementing secure systems, providing a foundation for understanding network security and system design in C.
