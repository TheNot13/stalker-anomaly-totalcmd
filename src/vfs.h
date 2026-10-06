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

    bool is_explicit_dir = (full_name.back() == '\\' || full_name.back() == '/');
    std::string clean_name = full_name;
    
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

// Проверка валидности распакованной таблицы FAT
inline bool TryParseFAT(const std::vector<u8>& decomp_fat, const std::string& db_path, const std::string& entry_point_prefix, FILETIME arc_time, int& files_added) {
    if (decomp_fat.size() < 18) return false;
    
    // Проверяем первый же элемент: размер узла не может быть больше MAX_PATH (260) + 16 байт
    uint16_t first_size = *(uint16_t*)decomp_fat.data();
    if (first_size < 17 || first_size > 276) return false;
    
    int first_name_len = first_size - 16;
    const char* first_name = (const char*)(decomp_fat.data() + 14);
    for (int i = 0; i < first_name_len; ++i) {
        unsigned char c = (unsigned char)first_name[i];
        if (c < 32 || c > 126) return false; // Имена файлов Сталкера ВСЕГДА чистый ASCII!
    }

    // Если первая запись — чистый ASCII, парсим весь буфер
    uint32_t ptr = 0;
    uint32_t decomp_size = (uint32_t)decomp_fat.size();
    files_added = 0;

    while (ptr + 2 <= decomp_size) {
        uint16_t item_size = *(uint16_t*)(decomp_fat.data() + ptr); 
        ptr += 2;
        
        if (item_size < 16 || (ptr - 2 + item_size) > decomp_size) break;
        
        uint32_t size_real = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;
        uint32_t size_compr = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;
        uint32_t crc = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;
        
        int name_length = item_size - 16;
        if (name_length <= 0 || name_length > 260) break;

        std::string name((char*)(decomp_fat.data() + ptr), name_length);
        ptr += name_length;
        uint32_t file_ptr = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;

        size_t null_pos = name.find('\0');
        if (null_pos != std::string::npos) name = name.substr(0, null_pos);

        // Строжайшая проверка на чистоту символов
        bool valid = !name.empty();
        for (unsigned char c : name) {
            if (c < 32 || c > 126) { valid = false; break; }
        }

        if (valid) {
            std::string full_name = entry_point_prefix + name;
            AddToVFS(full_name, size_real, size_compr, file_ptr, db_path, arc_time);
            files_added++;
        }
    }
    return files_added > 0;
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
    uint32_t offset = 0;
    std::string entry_point_prefix = "";

    try {
        while (offset < file_size && (file_size - offset) >= 8) {
            uint32_t type = 0, size = 0;
            file.read(reinterpret_cast<char*>(&type), 4);
            file.read(reinterpret_cast<char*>(&size), 4);
            offset += 8;

            uint32_t chunk_id = type & 0x7FFFFFFF;
            bool is_comp = (type & 0x80000000) != 0;

            if (size == 0 || size > (file_size - offset)) break; 

            // Чанки заголовка INI
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
            // Чанк FAT таблицы
            else if (chunk_id == 1) { 
                std::vector<uint8_t> chunk_data(size);
                file.read(reinterpret_cast<char*>(chunk_data.data()), size);
                offset += size;

                int files_count = 0;
                bool ok = false;

                if (!is_comp) {
                    ok = TryParseFAT(chunk_data, db_path, entry_point_prefix, arc_time, files_count);
                    if (ok) Logger("[RAW FAT] " + db_path + " (" + std::to_string(files_count) + " файлов)");
                } else {
                    // 1. Сначала пробуем стандартный LZO!
                    if (size > 4) {
                        uint32_t expected_size = *(uint32_t*)chunk_data.data();
                        if (expected_size > 0 && expected_size < 50 * 1024 * 1024) {
                            std::vector<u8> decomp_lzo(expected_size);
                            lzo_uint out_len = expected_size;
                            int r = lzo1x_decompress_safe(chunk_data.data() + 4, size - 4, decomp_lzo.data(), &out_len, NULL);
                            if (r == LZO_E_OK) {
                                ok = TryParseFAT(decomp_lzo, db_path, entry_point_prefix, arc_time, files_count);
                                if (ok) Logger("[LZO FAT] " + db_path + " (" + std::to_string(files_count) + " файлов)");
                            }
                        }
                    }

                    // 2. Если LZO не подошел — пробуем LzHuf!
                    if (!ok) {
                        LzhDecoder decoder;
                        std::vector<u8> decomp_lzh = decoder.Decode(chunk_data.data(), size);
                        if (!decomp_lzh.empty()) {
                            ok = TryParseFAT(decomp_lzh, db_path, entry_point_prefix, arc_time, files_count);
                            if (ok) Logger("[LzHuf FAT] " + db_path + " (" + std::to_string(files_count) + " файлов)");
                        }
                    }

                    if (!ok) {
                        Logger("[-] Не удалось прочитать FAT: " + db_path);
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
