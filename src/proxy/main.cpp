#include "../../include/hash_ring.hpp"
#include "../../include/httplib.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <map>

// --- HELPER FUNCTIONS ---
std::string sanitize_host(std::string address) {
    size_t colonPos = address.find(":");
    if (colonPos == std::string::npos) return address;
    std::string ip = address.substr(0, colonPos);
    std::string port = address.substr(colonPos + 1);
    if (ip == "localhost") return "127.0.0.1:" + port;
    return address;
}

bool get_ip_port(const std::string& address, std::string& ip, int& port) {
    std::string clean = sanitize_host(address);
    size_t colon = clean.find(":");
    if (colon == std::string::npos) return false;
    ip = clean.substr(0, colon);
    try { port = std::stoi(clean.substr(colon + 1)); } catch (...) { return false; }
    return true;
}

// --- OPTIMIZED ADD MIGRATION (Executed by Proxy) ---
void optimized_rebalance_add(ConsistentHashRing& ring, const std::string& new_node) {
    std::cout << "[Proxy] Rebalancing for new node: " << new_node << "...\n";
    auto tasks = ring.getRebalancingTasks(new_node);

    std::string new_ip; int new_port;
    if (!get_ip_port(new_node, new_ip, new_port)) return;
    httplib::Client dest_cli(new_ip, new_port);
    dest_cli.set_connection_timeout(1);

    for (const auto& task : tasks) {
        std::string src_ip; int src_port;
        if (!get_ip_port(task.source_node, src_ip, src_port)) continue;
        httplib::Client src_cli(src_ip, src_port);
        src_cli.set_connection_timeout(1);

        std::string path = "/range?start=" + std::to_string(task.start_hash) +
                           "&end=" + std::to_string(task.end_hash);
        auto res = src_cli.Get(path.c_str());

        if (res && res->status == 200) {
            std::stringstream ss(res->body);
            std::string key, val;
            while (std::getline(ss, key)) {
                if (std::getline(ss, val)) {
                    // 1. Copy to NEW node
                    httplib::Params p_put; p_put.emplace("key", key); p_put.emplace("val", val);
                    if (dest_cli.Post("/put", p_put)) {
                        // 2. Delete from OLD node
                        httplib::Params p_del; p_del.emplace("key", key);
                        src_cli.Post("/del", p_del);
                    }
                }
            }
        }
    }
    std::cout << "[Proxy] Rebalancing Complete.\n";
}

// --- REMOVE MIGRATION (Executed by Proxy) ---
void rebalance_remove(ConsistentHashRing& ring, const std::string& node_to_remove) {
    std::cout << "[Proxy] Evacuating node: " << node_to_remove << "...\n";
    std::string ip; int port;
    if (!get_ip_port(node_to_remove, ip, port)) {
        ring.removeNode(node_to_remove);
        return;
    }

    httplib::Client victim_cli(ip, port);
    victim_cli.set_connection_timeout(1);
    auto res = victim_cli.Get("/all");

    // Remove from ring immediately so new lookups don't go there
    ring.removeNode(node_to_remove);

    if (res && res->status == 200) {
        std::stringstream ss(res->body);
        std::string key, val;
        while (std::getline(ss, key)) {
            if (std::getline(ss, val)) {
                // Find NEW owner for this key
                std::string target = ring.getNode(key);
                std::string t_ip; int t_port;
                if (get_ip_port(target, t_ip, t_port)) {
                    httplib::Client dest(t_ip, t_port);
                    httplib::Params p; p.emplace("key", key); p.emplace("val", val);
                    if (dest.Post("/put", p)) {
                        httplib::Params del_p; del_p.emplace("key", key);
                        victim_cli.Post("/del", del_p);
                    }
                }
            }
        }
    }
    std::cout << "[Proxy] Evacuation Complete.\n";
}

int main() {
    ConsistentHashRing ring;
    httplib::Server svr;

    std::cout << "--- KV Proxy/Gateway running on Port 8000 ---\n";

    // 1. DATA API: PUT
    svr.Post("/put", [&](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.get_param_value("key");
        std::string target = ring.getNode(key);

        if (target.empty()) {
            res.status = 503;
            res.set_content("No storage servers available", "text/plain");
            return;
        }

        std::string ip; int port;
        get_ip_port(target, ip, port);
        httplib::Client cli(ip, port);

        // Forward the request
        httplib::Params p;
        p.emplace("key", key);
        p.emplace("val", req.get_param_value("val"));
        auto cli_res = cli.Post("/put", p);

        if(cli_res) {
            res.status = cli_res->status;
            res.set_content(cli_res->body, "text/plain");
        } else {
            res.status = 500;
        }
    });

    // 2. DATA API: GET
    svr.Get("/get", [&](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.get_param_value("key");
        std::string target = ring.getNode(key);

        if (target.empty()) { res.status = 503; return; }

        std::string ip; int port;
        get_ip_port(target, ip, port);
        httplib::Client cli(ip, port);

        auto cli_res = cli.Get(("/get?key=" + key).c_str());
        if(cli_res) {
            res.status = cli_res->status;
            res.set_content(cli_res->body, "text/plain");
        } else {
            res.status = 500;
        }
    });

    // 2.5 DATA API: DEL (New!)
    svr.Post("/del", [&](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.get_param_value("key");
        std::string target = ring.getNode(key);

        if (target.empty()) { res.status = 503; return; }

        std::string ip; int port;
        get_ip_port(target, ip, port);
        httplib::Client cli(ip, port);

        // Forward the delete request to the specific server
        httplib::Params p; p.emplace("key", key);
        auto cli_res = cli.Post("/del", p);

        if(cli_res) {
            res.status = cli_res->status;
            res.set_content("Deleted", "text/plain");
        } else {
            res.status = 500;
        }
    });

    // 3. ADMIN API: ADD NODE
    svr.Post("/add_node", [&](const httplib::Request& req, httplib::Response& res) {
        std::string host = req.get_param_value("host");
        host = sanitize_host(host);

        std::string ip; int port;
        get_ip_port(host, ip, port);

        httplib::Client temp_client(ip, port);
        temp_client.set_connection_timeout(2);

        // Try to ping the new server's /status endpoint
        auto check = temp_client.Get("/status");

        if (!check || check->status != 200) {
            // IT FAILED! Abort everything.
            std::cout << "[Error] Refusing to add dead node: " << host << "\n";
            res.status = 503;
            res.set_content("Error: Target node is not reachable.", "text/plain");
            return;
        }
        std::cout << "[Proxy] Health Check Passed for " << host << ". Adding to ring...\n";
        // --------------------------------------

        // --- STEP 2: UPDATE RING ---
        // Now it is safe to add it.
        ring.addNode(host);

        // --- STEP 3: MIGRATE DATA ---
        // Move keys from the old owner to this new node.
        optimized_rebalance_add(ring, host);

        res.set_content("Success: Node Added " + host, "text/plain");
    });

    // 4. ADMIN API: REMOVE NODE
    svr.Post("/remove_node", [&](const httplib::Request& req, httplib::Response& res) {
        std::string host = req.get_param_value("host");
        host = sanitize_host(host);

        // Trigger data evacuation
        rebalance_remove(ring, host);

        res.set_content("Node Removed: " + host, "text/plain");
    });

    svr.listen("0.0.0.0", 8000);
}