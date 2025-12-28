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

size_t get_shard_id(const string& key) {
    return hash<string>{}(key) % NUM_SHARDS;
}

bool in_range(size_t h, size_t start, size_t end) {
    if (start < end) return h > start && h <= end;
    return h > start || h <= end;
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
    wal_file.open(wal_filename, ios::app); // Append mode

    std::cout.setf(std::ios::unitbuf);
    httplib::Server svr;

    // 2. WRITE (Update Map + Log to Disk)
    svr.Post("/put", [](const httplib::Request& req, httplib::Response& res) {
        string key = req.get_param_value("key");
        string val = req.get_param_value("val");
        int id = get_shard_id(key);

        { lock_guard<mutex> lock(shard_mutexes[id]); db_shards[id][key] = val; }
        log_op("SET", key, val);

        cout << "\033[1;32m[Saved] " << key << "\033[0m" << endl;
        res.set_content("OK", "text/plain");
    });

    // 3. DELETE (Update Map + Log to Disk)
    svr.Post("/del", [](const httplib::Request& req, httplib::Response& res) {
        string key = req.get_param_value("key");
        int id = get_shard_id(key);

        { lock_guard<mutex> lock(shard_mutexes[id]); db_shards[id].erase(key); }
        log_op("DEL", key);

        cout << "\033[1;31m[Deleted] " << key << "\033[0m" << endl;
        res.set_content("OK", "text/plain");
    });

    // 4. READ (In-Memory Only)
    svr.Get("/get", [](const httplib::Request& req, httplib::Response& res) {
        string key = req.get_param_value("key");
        int id = get_shard_id(key);
        lock_guard<mutex> lock(shard_mutexes[id]);
        if (db_shards[id].count(key)) res.set_content(db_shards[id][key], "text/plain");
        else { res.status = 404; res.set_content("Not Found", "text/plain"); }
    });

    // 5. MIGRATION HELPERS (Keep these for the Proxy to use)
    svr.Get("/range", [](const httplib::Request& req, httplib::Response& res) {
        size_t start = stoull(req.get_param_value("start"));
        size_t end = stoull(req.get_param_value("end"));
        stringstream ss;
        for (int i = 0; i < NUM_SHARDS; ++i) {
            lock_guard<mutex> lock(shard_mutexes[i]);
            for (const auto& pair : db_shards[i]) {
                if (in_range(hash<string>{}(pair.first), start, end))
                    ss << pair.first << "\n" << pair.second << "\n";
            }
        }
        res.set_content(ss.str(), "text/plain");
    });

    // 6. Heartbeat / Status
    svr.Get("/status", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("OK", "text/plain");
    });

    svr.Get("/all", [](const httplib::Request& req, httplib::Response& res) {
        stringstream ss;
        for (int i = 0; i < NUM_SHARDS; ++i) {
            lock_guard<mutex> lock(shard_mutexes[i]);
            for (const auto& pair : db_shards[i]) ss << pair.first << "\n" << pair.second << "\n";
        }
        res.set_content(ss.str(), "text/plain");
    });

    cout << "--- Persistent Server Port " << port << " (WAL: " << wal_filename << ") ---" << endl;
    svr.listen("0.0.0.0", port);
}