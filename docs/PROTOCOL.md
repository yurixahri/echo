# Echo Protocol Specification

## 1. Network Transport Overview

Communication uses two decoupled protocols:
1. **Control & Chat Protocol (TCP):** ASN.1 DER-encoded messages over TCP (default port `5555`). Encrypted via TLS if SSL enabled.
2. **Voice Audio Protocol (UDP):** Low-latency binary packets over UDP on the same port (`5555`) with Opus-encoded audio.

---

## 2. TCP Control Protocol Framing

All TCP packets use a 2-byte big-endian length prefix followed by raw ASN.1 DER data:

```
+-----------------------------+------------------------------------+
| 2-byte Length (Big-Endian)  | ASN.1 DER Encoded Payload (N bytes)|
+-----------------------------+------------------------------------+
```

- **Maximum packet size:** 65,535 bytes.
- Large transfers (files, avatars) are split into chunks.

---

## 3. ASN.1 Protocol Messages (`ProtocolMessage` Choice)

Defined in `ChatProtocol.asn`. Messages are indexed by tag `[0]` through `[34]`:

| Tag | Name | Type | Direction | Description |
|---|---|---|---|---|
| `[0]` | `chat` | `ChatMessage` | Both | Sends or broadcasts a room chat message |
| `[1]` | `system` | `SystemStatus` | Server → Client | Server system telemetry (CPU, uptime) |
| `[2]` | `response` | `Response` | Server → Client | Generic acknowledgement or error for actions |
| `[3]` | `client-check-id` | `ClientCheckID` | Client → Server | Initial handshake: client UUID, name, password |
| `[4]` | `new-user-challenge`| `NewUserChallenge` | Server → Client | Proof-of-Work challenge for new registrations |
| `[5]` | `new-user-proof` | `NewUserProof` | Client → Server | Solution to PoW challenge |
| `[6]` | `claim-admin-privilege` | `ClaimAdminPrivilege` | Client → Server | Submits one-time setup token to claim admin |
| `[7]` | `member_list` | `MemberList` | Server → Client | Returns list of all registered server members |
| `[8]` | `member_detail` | `MemberDetail` | Server → Client | Detailed member card info (join date, roles) |
| `[9]` | `room_list` | `RoomList` | Server → Client | Full list of rooms, occupant users, power levels |
| `[10]` | `create-room` | `CreateRoom` | Client → Server | Request to create a new room with permissions |
| `[11]` | `join-room` | `JoinRoom` | Client → Server | Authenticates & joins chat (0) or voice (1) room |
| `[12]` | `update-server-settings` | `UpdateServerSettings` | Client → Server | Admin updates server display name or password |
| `[13]` | `leave-room` | `LeaveRoom` | Client → Server | Leaves chat (0) or voice (1) room |
| `[14]` | `file-upload-chunk` | `FileUploadChunk` | Client → Server | Uploads a chunk of a chat file attachment |
| `[15]` | `file-download-req` | `FileDownloadReq` | Client → Server | Requests a chunk of an attachment or avatar |
| `[16]` | `file-download-chunk` | `FileDownloadChunk` | Server → Client | Delivers chunk data of requested file |
| `[17]` | `history-req` | `HistoryReq` | Client → Server | Paginates chat history before a given `msg_id` |
| `[18]` | `delete-msg` | `DeleteMsg` | Client → Server | Hard-deletes a message and its disk attachment |
| `[19]` | `get-my-info` | `GetMyInfoReq` | Client → Server | Requests current user profile and server uptime |
| `[20]` | `my-info` | `MyInfo` | Server → Client | Returns user's own UUID, registration, uptime |
| `[21]` | `avatar-upload-chunk`| `AvatarUploadChunk` | Client → Server | Uploads avatar chunk (validated for image magic bytes) |
| `[22]` | `get-roles-req` | `GetRolesReq` | Client → Server | Requests server role definition list |
| `[23]` | `role-list` | `RoleList` | Server → Client | List of roles and permission mappings |
| `[24]` | `create-role` | `CreateRole` | Client → Server | Creates a new role with a name |
| `[25]` | `update-role-perms` | `UpdateRolePerms` | Client → Server | Updates integer/boolean permissions for a role |
| `[26]` | `delete-role` | `DeleteRole` | Client → Server | Deletes a role (cascades user assignments) |
| `[27]` | `get-user-roles-req`| `GetUserRolesReq` | Client → Server | Requests assigned roles for a target user ID |
| `[28]` | `user-roles-list` | `UserRolesList` | Server → Client | List of roles showing checked state for target user |
| `[29]` | `assign-user-roles` | `AssignUserRoles` | Client → Server | Submits comma-separated list of roles for user |
| `[30]` | `kick-user` | `KickUser` | Client → Server | Kicks target user from current room or server |
| `[31]` | `update-room` | `UpdateRoom` | Client → Server | Updates room name, password, and power thresholds |
| `[32]` | `poke-user` | `PokeUser` | Client → Server | Sends attention alert toast to target user |
| `[33]` | `get-members-req` | `GetMembersReq` | Client → Server | Requests server member list |
| `[34]` | `get-member-detail-req` | `GetMemberDetailReq` | Client → Server | Requests detail profile card for target user |

---

## 4. Response Events (`ASN1_RESPONE_EVENT`)

Returned inside `response` tag `[2]` with status (`SUCCESS = 0`, `FAILED = 1`, `PASSWORD_REQUIRED = 2`):

| Event ID | Enum Name | Description |
|---|---|---|
| 0 | `RESPONSE_USER_CHECK_ID` | Handshake response: returns user ID, avatar, server name, admin status |
| 1 | `RESPONSE_USER_CLAIM_ADMIN_PRIVILEGE` | Result of admin setup key submission |
| 2 | `RESPONSE_CREATE_ROOM` | Result of room creation |
| 3 | `RESPONSE_JOIN_ROOM` | Room authentication response (format: `<room_id>:<type>:<msg>`) |
| 4 | `RESPONSE_UPDATE_SERVER_SETTINGS` | Result of updating server configuration |
| 5 | `RESPONSE_FILE_UPLOAD` | File upload status |
| 6 | `RESPONSE_HISTORY` | End of chat history paging batch |
| 7 | `RESPONSE_DELETE_MSG` | Hard delete confirmation broadcast |
| 8 | `RESPONSE_AVATAR_UPLOAD` | Avatar upload completion status |
| 9 | `RESPONSE_CREATE_ROLE` | Role creation status |
| 10 | `RESPONSE_UPDATE_ROLE_PERMS` | Role permission update status |
| 11 | `RESPONSE_DELETE_ROLE` | Role deletion status |
| 12 | `RESPONSE_ASSIGN_USER_ROLES` | User role assignment status |
| 13 | `RESPONSE_KICK_USER` | Kick execution status |
| 14 | `RESPONSE_UPDATE_ROOM` | Room update status |
| 15 | `RESPONSE_KICKED` | Broadcast to client when kicked from room or server |
| 16 | `RESPONSE_PERMS_UPDATED` | Broadcast when user's permissions or roles change in realtime |
| 17 | `RESPONSE_POKE_USER` | Status of poke delivery |
| 18 | `RESPONSE_POKED` | Incoming poke notification (format: `<sender_name>:<sender_id>:<msg>`) |
| 19 | `RESPONSE_CHAT` | Chat send rejection (e.g., rate limit timeout) |

---

## 5. Voice UDP Protocol Specification

Voice transmission operates via connectionless UDP datagrams.

### Header Format (29 Bytes Packed)

```cpp
#pragma pack(push, 1)
struct voice_packet_header_t {
    uint8_t packet_type;   // 1: PING, 2: DATA, 3: LEAVE, 4: STATE
    uint64_t room_id;      // Target room ID
    uint64_t user_id;      // Sender user ID
    uint32_t sequence;     // Packet sequence counter
    uint64_t timestamp;    // Millisecond timestamp
};
#pragma pack(pop)
```

### Packet Types

- **`VOICE_PING` (1):** Keep-alive heartbeat sent every 2.5 seconds to maintain NAT port mapping.
- **`VOICE_DATA` (2):** Audio payload. Header is followed immediately by Opus encoded byte stream.
- **`VOICE_LEAVE` (3):** Sent when leaving channel to immediately flush client from routing table.
- **`VOICE_STATE` (4):** Informs server/peers of mute/deaf state changes.

### Audio Codec Parameters

- **Codec:** Opus
- **Sample Rate:** 48,000 Hz
- **Channels:** 1 (Mono)
- **Sample Format:** 16-bit signed integer PCM
- **Frame Duration:** 20 ms (960 samples per frame)
- **Bitrate:** Configurable 16–128 kbps (default: 40 kbps)
