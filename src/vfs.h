#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include "lzhuf.h"
#include "minilzo.h"

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
    if (full_name.empty()) return;

    // Движок X-Ray хранит папки со слешем на конце (например "configs\").
    // Мы должны правильно это обрабатывать, чтобы не плодить пустые файлы 0 байт!
    bool is_explicit_dir = (full_name.back() == '\\' || full_name.back() == '/');
    std::string clean_name = full_name;
    if (is_explicit_dir) {
        clean_name.pop_back(); // Убираем слеш для нормального парсинга
    }

    VfsNode* current = &vfs_root;
    size_t start = 0;
    size_t end = clean_name.find('\\');

    while (end != std::string::npos) {
        std::string part = clean_name.substr(start, end - start);
        auto& next_node = current->children[part];
        if (next_node.name.empty()) {
            next_node.name = part;
            next_node.is_dir = true;
            next_node.time_write = arc_time;
        }
        current = &next_node;
        start = end + 1;
        end = clean_name.find('\\', start);
    }

    std::string filename = clean_name.substr(start);
    if (filename.empty()) return; // Защита от мусора

    auto& file_node = current->children[filename];
    file_node.name = filename;
    
    if (is_explicit_dir) {
        // Если это была просто папка, помечаем её и идем дальше
        file_node.is_dir = true;
        if (file_node.time_write.dwHighDateTime == 0) file_node.time_write = arc_time;
    } else {
        // Если это реальный файл, записываем его метаданные
        file_node.is_dir = false;
        file_node.size_real = real_size;
        file_node.size_compr = comp_size;
        file_node.offset = offset;
        file_node.archive_path = arc_path;
        file_node.time_write = arc_time;
    }
}

inline void ParseArchive(const std::string& db_path) {
    FILETIME arc_time = {0, 0};
    HANDLE hFile = CreateFileA(db_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        GetFileTime(hFile, NULL, NULL, &arc_time);
        CloseHandle(hFile);
    }

    std::ifstream file(db_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return;
    
    uint32_t file_size = static_cast<uint32_t>(file.tellg());
    
    // В Anomaly ВСЕ архивы читаются одинаково — с нулевого байта по чанкам.
    // Никаких проверок "magic" больше нет, они ломали логику.
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

            if (chunk_id == 1) { // Это FAT таблица!
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
                    if (name_length <= 0 || name_length > 2048) break;

                    std::string name((char*)(decomp_fat.data() + ptr), name_length);
                    ptr += name_length;
                    uint32_t file_ptr = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;

                    // Убираем нуль-терминатор, если он вдруг есть
                    size_t null_pos = name.find('\0');
                    if (null_pos != std::string::npos) name = name.substr(0, null_pos);
                    
                    if (!name.empty()) {
                        AddToVFS(name, size_real, size_compr, file_ptr, db_path, arc_time);
                    }
                }
                break; // FAT прочитан, идем к следующему архиву
            } else {
                // Если это не FAT (например, данные или INI-заголовок) - просто прыгаем дальше
                file.seekg(size, std::ios::cur);
                offset += size;
            }
        }
    } catch (...) {}
}
