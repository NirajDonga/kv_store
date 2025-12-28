#include "../../include/httplib.h"
#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <sstream>
#include <fstream>

using namespace std;

// --- STORAGE & CONCURRENCY ---
const int NUM_SHARDS = 16;
unordered_map<string, string> db_shards[NUM_SHARDS];
mutex shard_mutexes[NUM_SHARDS];

// --- WAL GLOBALS ---
ofstream wal_file;
mutex wal_mutex;

// --- HASHING HELPERS ---

// 1. Internal Sharding Hash (Keeps local locking fast)
size_t get_shard_id(const string& key) {
    return hash<string>{}(key) % NUM_SHARDS;
}

// 2. FNV-1a Hash (CRITICAL FIX: Matches Proxy's Logic)
size_t fnv1a_hash(const std::string& key) {
    const size_t FNV_prime = 1099511628211u;
    const size_t offset_basis = 14695981039346656037u;
    size_t hash = offset_basis;
    for (char c : key) {
        hash ^= static_cast<size_t>(c);
        hash *= FNV_prime;
    }
    return hash;
}

// 3. Ring Range Check
bool in_range(size_t h, size_t start, size_t end) {
    if (start < end) return h > start && h <= end;
    return h > start || h <= end; // Handles Wrap-Around
}

// --- PERSISTENCE HELPERS ---
void log_op(const string& op, const string& key, const string& val = "") {
    lock_guard<mutex> lock(wal_mutex);
    wal_file << op << " " << key << " " << val << endl;
}

void restore_from_wal(const string& filename) {
    ifstream infile(filename);
    if (!infile.is_open()) return;

    cout << "[WAL] Restoring from " << filename << "..." << endl;
    string op, key, val;
    while (infile >> op >> key) {
        if (op == "SET") {
            getline(infile, val);
            if (!val.empty() && val[0] == ' ') val = val.substr(1);
            int id = get_shard_id(key);
            db_shards[id][key] = val;
        } else if (op == "DEL") {
            int id = get_shard_id(key);
            db_shards[id].erase(key);
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) { cerr << "Usage: ./kv_server <PORT>" << endl; return 1; }
    int port = atoi(argv[1]);

    // 1. SETUP WAL
    string wal_filename = "wal_" + to_string(port) + ".log";
    restore_from_wal(wal_filename);
    wal_file.open(wal_filename, ios::app);

    std::cout.setf(std::ios::unitbuf);
    httplib::Server svr;

    // 2. WRITE
    svr.Post("/put", [](const httplib::Request& req, httplib::Response& res) {
        string key = req.get_param_value("key");
        string val = req.get_param_value("val");
        int id = get_shard_id(key);

        { lock_guard<mutex> lock(shard_mutexes[id]); db_shards[id][key] = val; }
        log_op("SET", key, val);

        cout << "\033[1;32m[Saved] " << key << "\033[0m" << endl;
        res.set_content("OK", "text/plain");
    });

    // 3. DELETE
    svr.Post("/del", [](const httplib::Request& req, httplib::Response& res) {
        string key = req.get_param_value("key");
        int id = get_shard_id(key);

        { lock_guard<mutex> lock(shard_mutexes[id]); db_shards[id].erase(key); }
        log_op("DEL", key);

        cout << "\033[1;31m[Deleted] " << key << "\033[0m" << endl;
        res.set_content("OK", "text/plain");
    });

    // 4. READ
    svr.Get("/get", [](const httplib::Request& req, httplib::Response& res) {
        string key = req.get_param_value("key");
        int id = get_shard_id(key);
        lock_guard<mutex> lock(shard_mutexes[id]);
        if (db_shards[id].count(key)) res.set_content(db_shards[id][key], "text/plain");
        else { res.status = 404; res.set_content("Not Found", "text/plain"); }
    });

    // 5. MIGRATION HELPERS (THE FIX IS HERE)
    svr.Get("/range", [](const httplib::Request& req, httplib::Response& res) {
        size_t start = stoull(req.get_param_value("start"));
        size_t end = stoull(req.get_param_value("end"));
        stringstream ss;

        for (int i = 0; i < NUM_SHARDS; ++i) {
            lock_guard<mutex> lock(shard_mutexes[i]);
            for (const auto& pair : db_shards[i]) {
                // *** FIX: Use fnv1a_hash() instead of hash<string>{} ***
                if (in_range(fnv1a_hash(pair.first), start, end)) {
                    ss << pair.first << "\n" << pair.second << "\n";
                }
            }
        }
        res.set_content(ss.str(), "text/plain");
    });

    // 6. Heartbeat / Status
    svr.Get("/status", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("OK", "text/plain");
    });

    // 7. Debug Dump
    svr.Get("/all", [](const httplib::Request& req, httplib::Response& res) {
        stringstream ss;
        for (int i = 0; i < NUM_SHARDS; ++i) {
            lock_guard<mutex> lock(shard_mutexes[i]);
            for (const auto& pair : db_shards[i]) ss << pair.first << "\n" << pair.second << "\n";
        }
        res.set_content(ss.str(), "text/plain");
    });

    // 8. Reset
    svr.Post("/reset", [&](const httplib::Request&, httplib::Response& res) {
        for (int i = 0; i < NUM_SHARDS; ++i) {
            lock_guard<mutex> lock(shard_mutexes[i]);
            db_shards[i].clear();
        }
        wal_file.close();
        string filename = "wal_" + to_string(port) + ".log";
        std::remove(filename.c_str());
        wal_file.open(filename, ios::app);
        res.set_content("Database Reset", "text/plain");
    });

    cout << "--- Persistent Server Port " << port << " (WAL: " << wal_filename << ") ---" << endl;
    svr.listen("0.0.0.0", port);
}