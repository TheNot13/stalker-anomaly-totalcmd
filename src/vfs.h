#pragma once
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
    std::string archive_path;
    std::unordered_map<std::string, VfsNode> children;
};

extern VfsNode vfs_root;
extern bool vfs_loaded;
extern void Logger(const std::string& msg);

inline void AddToVFS(const std::string& full_name, uint32_t real_size, uint32_t comp_size, uint32_t offset, const std::string& arc_path) {
    VfsNode* current = &vfs_root;
    size_t start = 0;
    size_t end = full_name.find('\\');

    // Быстро режем путь и строим папки
    while (end != std::string::npos) {
        std::string part = full_name.substr(start, end - start);
        auto& next_node = current->children[part];
        if (next_node.name.empty()) {
            next_node.name = part;
            next_node.is_dir = true;
        }
        current = &next_node;
        start = end + 1;
        end = full_name.find('\\', start);
    }

    // Сам файл
    std::string filename = full_name.substr(start);
    auto& file_node = current->children[filename];
    file_node.name = filename;
    file_node.is_dir = false;
    file_node.size_real = real_size;
    file_node.size_compr = comp_size;
    file_node.offset = offset;
    file_node.archive_path = arc_path;
}

inline void ParseArchive(const std::string& db_path) {
    Logger("Сканирую архив: " + db_path);
    std::ifstream file(db_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        Logger("[-] Не удалось открыть: " + db_path);
        return;
    }
    
    uint32_t file_size = static_cast<uint32_t>(file.tellg());
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

            // Защита от мусорных данных (ограничение размера)
            if (size == 0 || size > (file_size - offset)) {
                break; 
            }

            if (chunk_id == 1) { // FAT
                std::vector<uint8_t> chunk_data(size);
                file.read(reinterpret_cast<char*>(chunk_data.data()), size);

                std::vector<u8> decomp_fat;
                if (is_comp) {
                    LzhDecoder decoder;
                    decomp_fat = decoder.Decode(chunk_data.data(), size);
                } else {
                    decomp_fat = std::move(chunk_data);
                }

                if (decomp_fat.empty()) {
                    Logger("[-] Ошибка распаковки FAT в архиве: " + db_path);
                    break;
                }

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
                    
                    uint32_t file_ptr = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;

                    AddToVFS(name, size_real, size_compr, file_ptr, db_path);
                }
                Logger("[+] Успех: " + db_path);
                break;
            } else {
                file.seekg(size, std::ios::cur);
                offset += size;
            }
        }
    } catch (...) {
        Logger("[-] КРИТИЧЕСКАЯ ОШИБКА (КРАШ) в архиве: " + db_path);
    }
}
