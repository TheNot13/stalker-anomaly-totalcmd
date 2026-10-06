#pragma once
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <iostream>
#include "lzhuf.h"

namespace fs = std::filesystem;

struct VfsNode {
    std::string name;
    bool is_dir;
    uint32_t size_real;
    uint32_t size_compr;
    uint32_t offset;
    std::string archive_path; // Знаем, в каком архиве лежит файл!
    std::map<std::string, VfsNode> children;
};

extern VfsNode vfs_root;
extern bool vfs_loaded;

inline std::vector<std::string> SplitPath(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string item;
    while (std::getline(ss, item, '\\')) {
        if (!item.empty()) parts.push_back(item);
    }
    return parts;
}

inline void AddToVFS(const std::string& full_name, uint32_t real_size, uint32_t comp_size, uint32_t offset, const std::string& arc_path) {
    std::vector<std::string> parts = SplitPath(full_name);
    VfsNode* current = &vfs_root;
    
    for (size_t i = 0; i < parts.size(); ++i) {
        const std::string& part = parts[i];
        if (current->children.find(part) == current->children.end()) {
            VfsNode node;
            node.name = part;
            node.is_dir = (i < parts.size() - 1);
            node.size_real = 0;
            node.size_compr = 0;
            node.offset = 0;
            current->children[part] = node;
        }
        current = &current->children[part];
    }
    
    // Перезаписываем данные файла, если он уже был в старом архиве (логика патчей)
    current->is_dir = false;
    current->size_real = real_size;
    current->size_compr = comp_size;
    current->offset = offset;
    current->archive_path = arc_path;
}

inline void ParseArchive(const std::string& db_path) {
    std::ifstream file(db_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return;
    
    uint32_t file_size = static_cast<uint32_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    uint32_t offset = 0;

    while (offset < file_size) {
        uint32_t type = 0, size = 0;
        if (!file.read(reinterpret_cast<char*>(&type), 4)) break;
        if (!file.read(reinterpret_cast<char*>(&size), 4)) break;
        offset += 8;

        uint32_t chunk_id = type & 0x7FFFFFFF;

        if (chunk_id == 1) { // FAT таблица
            std::vector<uint8_t> chunk_data(size);
            file.read(reinterpret_cast<char*>(chunk_data.data()), size);

            LzhDecoder decoder;
            std::vector<u8> decomp_fat = decoder.Decode(chunk_data.data(), size);
            
            if (decomp_fat.empty()) break;

            uint32_t ptr = 0;
            uint32_t decomp_size = decomp_fat.size();

            while (ptr < decomp_size) {
                if (ptr + 2 > decomp_size) break;
                uint16_t item_size = *(uint16_t*)(decomp_fat.data() + ptr); ptr += 2;
                if (ptr - 2 + item_size > decomp_size) break;
                
                uint32_t size_real = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;
                uint32_t size_compr = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;
                uint32_t crc = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;
                
                int name_length = item_size - 16;
                std::string name((char*)(decomp_fat.data() + ptr), name_length);
                ptr += name_length;
                
                uint32_t file_ptr = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;

                AddToVFS(name, size_real, size_compr, file_ptr, db_path);
            }
            break;
        } else {
            file.seekg(size, std::ios::cur);
            offset += size;
        }
    }
}
