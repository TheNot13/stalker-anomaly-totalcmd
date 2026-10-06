#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include "lzhuf.h"

namespace fs = std::filesystem;

struct VfsNode {
    std::string name;
    bool is_dir;
    uint32_t size_real;
    uint32_t size_compr;
    uint32_t offset;
    FILETIME time_write;
    std::string archive_path;
    std::unordered_map<std::string, VfsNode> children;
};

extern VfsNode vfs_root;
extern bool vfs_loaded;
extern void Logger(const std::string& msg);

inline void AddToVFS(const std::string& full_name, uint32_t real_size, uint32_t comp_size, uint32_t offset, const std::string& arc_path, FILETIME arc_time) {
    VfsNode* current = &vfs_root;
    size_t start = 0;
    size_t end = full_name.find('\\');

    while (end != std::string::npos) {
        std::string part = full_name.substr(start, end - start);
        auto& next_node = current->children[part];
        if (next_node.name.empty()) {
            next_node.name = part;
            next_node.is_dir = true;
            next_node.time_write = arc_time;
        }
        current = &next_node;
        start = end + 1;
        end = full_name.find('\\', start);
    }

    std::string filename = full_name.substr(start);
    auto& file_node = current->children[filename];
    file_node.name = filename;
    file_node.is_dir = false;
    file_node.size_real = real_size;
    file_node.size_compr = comp_size;
    file_node.offset = offset;
    file_node.archive_path = arc_path;
    file_node.time_write = arc_time;
}

inline void ParseArchive(const std::string& db_path) {
    // Получаем дату создания архива
    FILETIME arc_time = {0, 0};
    HANDLE hFile = CreateFileA(db_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        GetFileTime(hFile, NULL, NULL, &arc_time);
        CloseHandle(hFile);
    }

    std::ifstream file(db_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return;
    
    uint32_t file_size = static_cast<uint32_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    
    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), 4);
    if ((magic & 0x7FFFFFFF) != 666) return; // Строгий пропуск чужих форматов
    
    file.seekg(0, std::ios::beg);
    uint32_t offset = 0;

    try {
        while (offset < file_size && (file_size - offset) >= 8) {
            uint32_t type = 0, size = 0;
            file.read(reinterpret_cast<char*>(&type), 4);
            file.read(reinterpret_cast<char*>(&size), 4);
            offset += 8;

            uint32_t chunk_id = type & 0x7FFFFFFF;
            bool is_comp = (type & 0x80000000) != 0;

            if (size == 0 || size > (file_size - offset)) break; 

            if (chunk_id == 1) { 
                std::vector<uint8_t> chunk_data(size);
                file.read(reinterpret_cast<char*>(chunk_data.data()), size);

                std::vector<u8> decomp_fat;
                if (is_comp) {
                    LzhDecoder decoder;
                    decomp_fat = decoder.Decode(chunk_data.data(), size);
                } else {
                    decomp_fat = std::move(chunk_data);
                }

                if (decomp_fat.empty()) break;

                uint32_t ptr = 0;
                uint32_t decomp_size = decomp_fat.size();

                while (ptr + 2 <= decomp_size) {
                    uint16_t item_size = *(uint16_t*)(decomp_fat.data() + ptr); 
                    ptr += 2;
                    if (item_size < 16 || (ptr - 2 + item_size) > decomp_size) break;
                    
                    uint32_t size_real = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;
                    uint32_t size_compr = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;
                    uint32_t crc = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;
                    
                    int name_length = item_size - 16;
                    std::string name((char*)(decomp_fat.data() + ptr), name_length);
                    ptr += name_length;

                    // Убиваем нуль-терминатор и мусор после него
                    name.erase(std::find(name.begin(), name.end(), '\0'), name.end());
                    
                    bool is_garbage = false;
                    for (char c : name) {
                        if ((unsigned char)c < 32 || c == '*' || c == '?' || c == '<' || c == '>' || c == '|' || c == '"') {
                            is_garbage = true; break;
                        }
                    }

                    uint32_t file_ptr = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;

                    if (!is_garbage && !name.empty()) {
                        AddToVFS(name, size_real, size_compr, file_ptr, db_path, arc_time);
                    }
                }
                break;
            } else {
                file.seekg(size, std::ios::cur);
                offset += size;
            }
        }
    } catch (...) {}
}
