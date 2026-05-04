## Implementation Plan - Project 2

Phase 1: Infrastructure
- Create `crypto_utils.c/.h`: SHA-256 hashing, nonce/salt generation, hex conversion helpers
- Create `db_utils.c/.h`: linked-list user database with load/save for `user_db.txt`
- Extend `protocol.h`: add auth structs (ClientRequest, ServerChallenge, ClientResponse) and command structs (ClientCommand, ServerResponse)

Phase 2: Authentication
- Server: parse username -> generate random nonce + retrieve salt -> send challenge
- Client: receive challenge -> prompt password → compute SHA-256(password + salt) -> compute response = SHA-256(nonce + hash) -> send response
- Server: verify response matches expected value

Phase 3: Encryption & RBAC
- Implement AES-256-CBC encryption for all post-auth traffic
- Add role-based access control: block USER from ADMIN commands (adduser, listusers, setrole)

Phase 4: Commands & Polish
- Implement post-auth command loop: client reads stdin, sends ClientCommand; server executes and responds
- All required commands: req1, req2, adduser, listusers, setrole, exit
- Update Makefile to link OpenSSL and compile all object files