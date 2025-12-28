#include "../../include/httplib.h"
#include <iostream>
#include <sstream>

int main() {
    // Connect to the PROXY (Port 8000), not the servers
    httplib::Client proxy("127.0.0.1", 8000);
    proxy.set_connection_timeout(2);

    std::cout << "--- Thin KV Client (Connected to Proxy:8000) ---\n";
    std::cout << "Commands: ADD <host:port>, REMOVE <host:port>, SET <k> <v>, GET <k>, EXIT\n";

    std::string line, cmd, arg1, arg2;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        std::stringstream ss(line); ss >> cmd;

        if (cmd == "EXIT") break;

        else if (cmd == "ADD") {
            ss >> arg1;
            httplib::Params p; p.emplace("host", arg1);
            auto res = proxy.Post("/add_node", p);
            if(res) std::cout << "[Proxy] " << res->body << "\n";
            else std::cout << "[Error] Proxy Unreachable\n";
        }

        else if (cmd == "REMOVE") {
            ss >> arg1;
            httplib::Params p; p.emplace("host", arg1);
            auto res = proxy.Post("/remove_node", p);
            if(res) std::cout << "[Proxy] " << res->body << "\n";
            else std::cout << "[Error] Proxy Unreachable\n";
        }

        else if (cmd == "DEL") {
            ss >> arg1; // The key to delete
            httplib::Params p; p.emplace("key", arg1);
            auto res = proxy.Post("/del", p);

            if(res && res->status == 200) std::cout << "Success: Deleted " << arg1 << "\n";
            else std::cout << "Error: Could not delete (Status " << (res ? res->status : 0) << ")\n";
        }

        else if (cmd == "SET") {
            ss >> arg1; std::getline(ss, arg2);
            if (!arg2.empty()) arg2 = arg2.substr(1); // Remove leading space

            httplib::Params p; p.emplace("key", arg1); p.emplace("val", arg2);
            auto res = proxy.Post("/put", p);
            if(res && res->status == 200) std::cout << "OK\n";
            else std::cout << "Error (Status " << (res ? res->status : 0) << ")\n";
        }

        else if (cmd == "GET") {
            ss >> arg1;
            auto res = proxy.Get(("/get?key=" + arg1).c_str());
            if (res && res->status == 200) std::cout << "Value: " << res->body << "\n";
            else std::cout << "Not found\n";
        }
    }
    return 0;
}