#include "../../include/hash_ring.hpp"
#include "../../include/httplib.h"
#include <iostream>
#include <sstream>

// Helper to print colored text (optional, but nice)
void print_success(const std::string& msg) { std::cout << "\033[1;32m" << msg << "\033[0m\n"; }
void print_error(const std::string& msg)   { std::cout << "\033[1;31m" << msg << "\033[0m\n"; }
void print_info(const std::string& msg)    { std::cout << "\033[1;34m" << msg << "\033[0m\n"; }

// --- SEND DATA (WRITE) ---
void put_data(ConsistentHashRing& ring, const std::string& key, const std::string& val) {
    std::string target = ring.getNode(key);

    std::string ip = target.substr(0, target.find(":"));
    int port = std::stoi(target.substr(target.find(":") + 1));

    httplib::Client cli(ip, port);
    httplib::Params params;
    params.emplace("key", key);
    params.emplace("val", val);

    auto res = cli.Post("/put", params);

    if (res && res->status == 200) {
        print_success("[OK] Stored '" + key + "' on Node " + std::to_string(port));
    } else {
        print_error("[FAIL] Could not write to Node " + std::to_string(port));
    }
}

// --- GET DATA (READ) ---
void get_data(ConsistentHashRing& ring, const std::string& key) {
    std::string target = ring.getNode(key);

    std::string ip = target.substr(0, target.find(":"));
    int port = std::stoi(target.substr(target.find(":") + 1));

    httplib::Client cli(ip, port);

    // Send GET request: /get?key=...
    auto res = cli.Get(("/get?key=" + key).c_str());

    if (res && res->status == 200) {
        print_success("[FOUND] Node " + std::to_string(port) + " returned: " + res->body);
    } else if (res && res->status == 404) {
        print_error("[404] Key not found on Node " + std::to_string(port));
    } else {
        print_error("[FAIL] Could not read from Node " + std::to_string(port));
    }
}

int main() {
    ConsistentHashRing ring;

    // Connect to your cluster
    ring.addNode("localhost:8081");
    ring.addNode("localhost:8082");
    ring.addNode("localhost:8083");

    print_info("--- Distributed KV Store Client ---");
    print_info("Commands:");
    print_info("  SET <key> <value>   (Example: SET user_1 Alice)");
    print_info("  GET <key>           (Example: GET user_1)");
    print_info("  EXIT                (Quit)");
    std::cout << "-----------------------------------\n";

    std::string line, command, key, value;

    // Interactive Loop
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        std::stringstream ss(line);
        ss >> command;

        if (command == "EXIT" || command == "exit") {
            break;
        }
        else if (command == "SET" || command == "set") {
            ss >> key;
            // Read the rest of the line as value (handles spaces)
            std::getline(ss, value);
            // Trim leading space from value
            if (!value.empty() && value[0] == ' ') value = value.substr(1);

            if (key.empty() || value.empty()) {
                print_error("Usage: SET <key> <value>");
            } else {
                put_data(ring, key, value);
            }
        }
        else if (command == "GET" || command == "get") {
            ss >> key;
            if (key.empty()) {
                print_error("Usage: GET <key>");
            } else {
                get_data(ring, key);
            }
        }
        else {
            print_error("Unknown command. Try SET or GET.");
        }
    }

    return 0;
}