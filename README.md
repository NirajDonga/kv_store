# Distributed Key-Value Store (C++)

A high-performance distributed key-value store implemented in C++. It features **Consistent Hashing** for data sharding and efficient load balancing across multiple storage nodes.

## 🏗 Architecture

*   **Sharding**: Data is automatically partitioned across available nodes using a **Consistent Hash Ring**.
*   **Virtual Nodes**: Implements virtual nodes to ensure even data distribution.
*   **Networking**: Uses HTTP for communication between the Coordinator (Client) and Storage Nodes.

## 🛠 Prerequisites

*   **OS**: Windows (tested), Linux, or macOS.
*   **Compiler**: C++17 compatible compiler (MinGW, MSVC, or GCC).
*   **Build System**: CMake (Version 3.10+).

## 🚀 Build Instructions

Open your terminal in the project root.

Create a build directory and compile the project:

```powershell
mkdir build
cd build
cmake ..
cmake --build . --target All
```

(Note: If using CLion, you can simply click "Build Project" from the top menu).

## ⚡ How to Run the Cluster

To simulate a distributed system on one machine, we will run 3 separate server instances on different ports. You need **4 separate Terminal tabs**.

### Step 1: Start the Storage Nodes (Shards)

**Terminal Tab 1 (Node A - Port 8081)**

```powershell
cd cmake-build-debug; .\kv_server.exe 8081
```

**Terminal Tab 2 (Node B - Port 8082)**

```powershell
cd cmake-build-debug; .\kv_server.exe 8082
```

**Terminal Tab 3 (Node C - Port 8083)**

```powershell
cd cmake-build-debug; .\kv_server.exe 8083
```

You should see "Starting Node on Port XXXX..." in each tab.

### Step 2: Start the Client (Coordinator)

**Terminal Tab 4**

```powershell
cd cmake-build-debug; .\kv_client.exe
```

## 🎮 Usage Guide

Once the client is running, you can interact with your cluster using the following commands:

### Write Data (SET)

Syntax: `SET <key> <value>`

```plaintext
> SET username Alice
[OK] Stored 'username' on Node 8082
```

### Read Data (GET)

Syntax: `GET <key>`

```plaintext
> GET username
[FOUND] Node 8082 returned: Alice
```

### Exit

```plaintext
> EXIT
```

## 🔍 How it Works (Under the Hood)

1.  **Hashing**: When you type `SET user1 Bob`, the client calculates `Hash("user1")`.
2.  **Routing**: The Consistent Hash Ring determines which node owns that hash (e.g., Node 8083).
3.  **Request**: The client sends an HTTP POST request directly to `localhost:8083`.
4.  **Storage**: Node 8083 saves the data in its local memory (sharded storage).
