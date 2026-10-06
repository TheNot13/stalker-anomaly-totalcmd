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

inline bool IsValidString(const std::string& str) {
    if (str.empty()) return false;
    for (unsigned char c : str) {
        // Отсекаем управляющие символы и запрещенные для Windows знаки
        if (c < 32 || c == '*' || c == '?' || c == '<' || c == '>' || c == '|' || c == '"') {
            return false;
        }
    }
    return true;
}

inline void AddToVFS(const std::string& full_name, uint32_t real_size, uint32_t comp_size, uint32_t offset, const std::string& arc_path, FILETIME arc_time) {
    if (full_name.empty() || !IsValidString(full_name)) return;

    bool is_explicit_dir = (full_name.back() == '\\' || full_name.back() == '/');
    std::string clean_name = full_name;
    
    // Убираем ведущие слеши
    while (!clean_name.empty() && (clean_name.front() == '\\' || clean_name.front() == '/')) {
        clean_name.erase(0, 1);
    }
    if (clean_name.empty()) return;

    if (is_explicit_dir) clean_name.pop_back();

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
    if (filename.empty()) return;

    auto& file_node = current->children[filename];
    file_node.name = filename;
    
    if (is_explicit_dir) {
        file_node.is_dir = true;
        if (file_node.time_write.dwHighDateTime == 0) file_node.time_write = arc_time;
    } else {
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
    file.seekg(0, std::ios::beg);
    
    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), 4);
    
    try {
        if (magic == 29) {
            // =========================================================
            // СТАРЫЙ ФОРМАТ 29 (ОРИГИНАЛЬНЫЙ STALKER CoP / БАЗА ANOMALY)
            // =========================================================
            uint32_t fat_offset = 0, fat_size = 0;
            file.read(reinterpret_cast<char*>(&fat_offset), 4);
            file.read(reinterpret_cast<char*>(&fat_size), 4);

            if (fat_offset < 12 || fat_size == 0 || fat_offset >= file_size) return;

            file.seekg(fat_offset, std::ios::beg);
            std::vector<uint8_t> comp_fat(fat_size);
            file.read(reinterpret_cast<char*>(comp_fat.data()), fat_size);

            std::vector<u8> decomp_fat(30 * 1024 * 1024);
            lzo_uint decomp_size = decomp_fat.size();
            int r = lzo1x_decompress_safe(comp_fat.data(), fat_size, decomp_fat.data(), &decomp_size, NULL);

            if (r == LZO_E_OK) {
                uint32_t ptr = 0;
                // В старом формате НЕТ item_size. Парсим строго по нуль-терминатору!
                while (ptr + 16 <= decomp_size) {
                    uint32_t size_real = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;
                    uint32_t size_compr = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;
                    uint32_t crc = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;

                    std::string name;
                    while (ptr < decomp_size && decomp_fat[ptr] != '\0') {
                        name += (char)decomp_fat[ptr];
                        ptr++;
                    }
                    ptr++; // Пропускаем '\0'
                    
                    if (ptr + 4 > decomp_size) break;
                    uint32_t file_ptr = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;

                    AddToVFS(name, size_real, size_compr, file_ptr, db_path, arc_time);
                }
            }
        }
        else {
            // =========================================================
            // НОВЫЙ ФОРМАТ (ЧАНКИ .xdb / ИЗМЕНЕННЫЕ .db)
            // =========================================================
            file.seekg(0, std::ios::beg);
            uint32_t offset = 0;
            std::string entry_point_prefix = "";

            while (offset < file_size && (file_size - offset) >= 8) {
                uint32_t type = 0, size = 0;
                file.read(reinterpret_cast<char*>(&type), 4);
                file.read(reinterpret_cast<char*>(&size), 4);
                offset += 8;

                uint32_t chunk_id = type & 0x7FFFFFFF;
                bool is_comp = (type & 0x80000000) != 0;

                if (size == 0 || size > (file_size - offset)) break; 

                if (chunk_id == 666 || chunk_id == 29) { 
                    std::string ini_data(size, '\0');
                    file.read(ini_data.data(), size);
                    offset += size;

                    size_t pos = ini_data.find("entry_point");
                    if (pos != std::string::npos) {
                        size_t eq_pos = ini_data.find('=', pos);
                        if (eq_pos != std::string::npos) {
                            size_t start = ini_data.find_first_not_of(" \t", eq_pos + 1);
                            size_t end = ini_data.find_first_of("\r\n", start);
                            if (start != std::string::npos && end != std::string::npos) {
                                std::string ep = ini_data.substr(start, end - start);
                                size_t d2 = ep.rfind('$');
                                if (d2 != std::string::npos && d2 + 1 < ep.length()) {
                                    entry_point_prefix = ep.substr(d2 + 1);
                                } else {
                                    entry_point_prefix = ep;
                                }
                                if (!entry_point_prefix.empty() && (entry_point_prefix[0] == '\\' || entry_point_prefix[0] == '/')) {
                                    entry_point_prefix = entry_point_prefix.substr(1);
                                }
                                std::replace(entry_point_prefix.begin(), entry_point_prefix.end(), '/', '\\');
                                if (!entry_point_prefix.empty() && entry_point_prefix.back() != '\\') {
                                    entry_point_prefix += "\\";
                                }
                            }
                        }
                    }
                }
                else if (chunk_id == 1) { // FAT таблица
                    std::vector<uint8_t> chunk_data(size);
                    file.read(reinterpret_cast<char*>(chunk_data.data()), size);
                    offset += size;

                    std::vector<u8> decomp_fat;
                    if (is_comp && size > 4) {
                        uint32_t expected_size = *(uint32_t*)chunk_data.data();
                        
                        // Сначала пытаемся распаковать стандартным LZO
                        std::vector<u8> decomp_lzo(expected_size + 4096);
                        lzo_uint out_len = decomp_lzo.size();
                        int r = lzo1x_decompress_safe(chunk_data.data() + 4, size - 4, decomp_lzo.data(), &out_len, NULL);
                        
                        if (r == LZO_E_OK && out_len == expected_size) {
                            decomp_fat = std::move(decomp_lzo);
                            decomp_fat.resize(out_len);
                        } else {
                            // Если LZO выдал ошибку, значит это LzHuf из Anomaly!
                            LzhDecoder decoder;
                            decomp_fat = decoder.Decode(chunk_data.data(), size);
                        }
                    } else {
                        decomp_fat = std::move(chunk_data);
                    }

                    if (decomp_fat.empty()) break;

                    uint32_t ptr = 0;
                    uint32_t decomp_size = decomp_fat.size();

                    // В новом формате Чанков ЕСТЬ параметр item_size
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

                        size_t null_pos = name.find('\0');
                        if (null_pos != std::string::npos) name = name.substr(0, null_pos);

                        if (!name.empty() && IsValidString(name)) {
                            std::string full_name = entry_point_prefix + name;
                            AddToVFS(full_name, size_real, size_compr, file_ptr, db_path, arc_time);
                        }
                    }
                } else {
                    file.seekg(size, std::ios::cur);
                    offset += size;
                }
            }
        }
    } catch (...) {}
}
