#include "../../include/httplib.h"
#include <iostream>
#include <string>
#include <vector>

int main() {
    httplib::Client proxy("localhost", 8000);
    proxy.set_connection_timeout(5);

    std::string command;
    std::cout << "--- Distributed KV Store Client ---\n";
    std::cout << "Commands: SET k v | GET k | DEL k | ADD host | REMOVE host\n";

    while (true) {
        std::cout << "> ";
        std::cin >> command;

        if (command == "SET") {
            std::string k, v;
            std::cin >> k >> v;
            httplib::Params params;
            params.emplace("key", k);
            params.emplace("val", v);
            auto res = proxy.Post("/put", params);
            if (res) std::cout << res->body << "\n";
            else std::cout << "Error: Proxy unreachable\n";
        }
        else if (command == "GET") {
            std::string k;
            std::cin >> k;
            auto res = proxy.Get(("/get?key=" + k).c_str());
            if (res) std::cout << res->body << "\n";
            else std::cout << "Error: Proxy unreachable\n";
        }
        else if (command == "ADD") {
            std::string host;
            std::cin >> host;
            httplib::Params params;
            params.emplace("host", host);
            auto res = proxy.Post("/add_node", params);
            if (res) std::cout << res->body << "\n";
            else std::cout << "Error: Proxy unreachable\n";
        }
        else if (command == "REMOVE") {
            std::string host;
            std::cin >> host;
            httplib::Params params;
            params.emplace("host", host);
            auto res = proxy.Post("/remove_node", params);
            if (res) std::cout << res->body << "\n";
            else std::cout << "Error: Proxy unreachable\n";
        }
        else if (command == "EXIT") {
            break;
        }
    }
    return 0;
}