# Distributed Key-Value Store (Persistent & Proxy-Based)

This is a C++ project demonstrating a production-ready **Distributed Key-Value Store** architecture. It implements **Consistent Hashing** for dynamic scaling, **Write-Ahead Logs (WAL)** for data persistence, and a **Proxy Gateway** pattern to decouple clients from storage logic.

## 🚀 Key Features

* **Consistent Hashing:** Distributes data across nodes with minimal movement during scaling.
* **Persistence (WAL):** Implementation of a Write-Ahead Log to ensure data survives server restarts.
* **Proxy Architecture:** A "Smart Gateway" handles all routing and rebalancing, allowing for "Thin Clients".
* **Dynamic Scaling:** Add or remove nodes on the fly with automatic data migration.
* **Internal Sharding:** High-concurrency local storage using mutex-protected shards.

## 🛠️ Architecture

1. **Client (`kv_client`):** A dumb terminal. Connects *only* to the Proxy.
2. **Proxy (`kv_proxy`):** The brain. Holds the Hash Ring, routes requests, and manages data migration.
3. **Server (`kv_server`):** The storage. Saves data to memory and appends to a disk log (`wal_PORT.log`).

## 📦 Getting Started

### 1. Build the Project

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### 2. Start the Cluster

You will need 4 separate terminals to simulate the cluster on one machine.

**Terminal 1: Storage Node A (Port 8081)**

```bash
./kv_server 8081
# Output: --- Persistent Server Port 8081 (WAL: wal_8081.log) ---
```

**Terminal 2: Storage Node B (Port 8082)**

```bash
./kv_server 8082
# Output: --- Persistent Server Port 8082 (WAL: wal_8082.log) ---
```

**Terminal 3: The Proxy (Port 8000)**

```bash
./kv_proxy
# Output: --- KV Proxy/Gateway running on Port 8000 ---
```

**Terminal 4: The Client**

```bash
./kv_client
# Output: --- Thin KV Client (Connected to Proxy:8000) ---
```

## 🎮 Usage

Once the client is running, you can interact with the system. The commands are sent to the Proxy, which handles the rest.

### 1. Initialize the Cluster

Tell the proxy about your storage nodes.

```
> ADD 127.0.0.1:8081
[Proxy] Node Added: 127.0.0.1:8081

> ADD 127.0.0.1:8082
[Proxy] Node Added: 127.0.0.1:8082
```

### 2. Store & Retrieve Data

```
> SET user1 Alice
OK

> GET user1
Value: Alice
```

The Proxy automatically hashed `user1` and routed it to either 8081 or 8082.

### 3. Verify Persistence (The "Kill Test")

Stop the server holding your data (Ctrl+C in Terminal 1 or 2).

Restart that server (`./kv_server 8081`).

Get the key again in the client.

```
> GET user1
Value: Alice
```

It works! The server replayed `wal_8081.log` on startup.

### 4. Dynamic Scaling (Rebalancing)

Add a new node and watch data move automatically.

```
> ADD 127.0.0.1:8083
[Proxy] Rebalancing for new node...
[Proxy] Rebalancing Complete.
```

### 5. Remove a Node (Evacuation)

Safely remove a node; the proxy will move its data to others before it disconnects.

```
> REMOVE 127.0.0.1:8081
[Proxy] Evacuating node...
[Proxy] Node Removed.
```

## 📁 Project Structure

```
├── src/
│   ├── client/         # Client UI logic
│   ├── proxy/          # Coordinator logic (Hash Ring & Migration)
│   ├── server/         # Storage engine (In-Memory Map + WAL)
│   └── common/         # Shared Hash Ring algorithms
├── include/
│   ├── hash_ring.hpp   # Hash Ring interface
│   └── httplib.h       # HTTP library
└── CMakeLists.txt      # Build configuration
```

## 🔧 Requirements

- C++17 or higher
- CMake 3.10 or higher
- A C++ compiler (GCC, Clang, or MSVC)

## 📝 License

This project is provided as-is for educational purposes.