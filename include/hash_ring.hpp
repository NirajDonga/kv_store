#pragma once
#include <string>
#include <map>
#include <vector>

struct MigrationTask {
    std::string source_node; // The "Victim" we steal from
    size_t start_hash;       // Range Start (exclusive)
    size_t end_hash;         // Range End (inclusive)
};

class ConsistentHashRing {
private:
    std::map<size_t, std::string> ring;
    int virtual_nodes;
    size_t hash_key(const std::string& key);

public:
    ConsistentHashRing(int v_nodes = 100);
    void addNode(const std::string& node_address);
    void removeNode(const std::string& node_address);
    std::string getNode(const std::string& key);
    std::vector<MigrationTask> getRebalancingTasks(const std::string& new_node);
};