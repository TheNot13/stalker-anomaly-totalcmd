#include <windows.h>
#include <string>
#include <fstream>
#include <vector>
#include <algorithm>
#include "lz4.h"

std::vector<std::string> virtual_files;
int current_file_index = 0;

std::string GetIniPath() {
    char path[MAX_PATH];
    HMODULE hm = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&GetIniPath, &hm);
    GetModuleFileNameA(hm, path, sizeof(path));
    std::string full_path(path);
    size_t pos = full_path.find_last_of("\\/");
    if (pos != std::string::npos) {
        return full_path.substr(0, pos) + "\\anomaly_db.ini";
    }
    return "anomaly_db.ini";
}

std::string GetDbPath() {
    std::string ini_path = GetIniPath();
    char result[MAX_PATH];
    GetPrivateProfileStringA("Settings", "DbPath", "", result, MAX_PATH, ini_path.c_str());
    return std::string(result);
}

void ReadFAT() {
    virtual_files.clear();
    
    std::string db_path = GetDbPath();
    if (db_path.empty()) {
        virtual_files.push_back("ERROR_NO_DBPATH_IN_INI.txt");
        return;
    }
    
    std::ifstream file(db_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        virtual_files.push_back("ERROR_FILE_NOT_FOUND.txt");
        return;
    }
    
    uint32_t file_size = static_cast<uint32_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    uint32_t offset = 0;
    bool found_fat = false;

    // Читаем архив по Чанкам (Chunks) как в LocatorAPI::open_chunk
    while (offset < file_size) {
        uint32_t type = 0, size = 0;
        file.read(reinterpret_cast<char*>(&type), 4);
        file.read(reinterpret_cast<char*>(&size), 4);
        offset += 8;

        // В X-Ray ID чанка - это младшие биты, а старший бит означает сжатие
        uint32_t id = type & 0x7FFFFFFF;
        bool is_compressed = (type & 0x80000000) != 0;

        // Нам нужен Чанк №1 (Это таблица FAT)
        if (id == 1) {
            found_fat = true;
            std::vector<uint8_t> chunk_data(size);
            file.read(reinterpret_cast<char*>(chunk_data.data()), size);

            std::vector<uint8_t> decomp_fat;
            uint32_t decomp_size = 0;

            if (is_compressed) {
                // Если сжато, первые 4 байта - распакованный размер
                decomp_size = *(uint32_t*)chunk_data.data();
                decomp_fat.resize(decomp_size);
                
                int r = LZ4_decompress_safe(
                    reinterpret_cast<char*>(chunk_data.data() + 4), 
                    reinterpret_cast<char*>(decomp_fat.data()), 
                    size - 4, 
                    decomp_size
                );
                
                if (r < 0) {
                    virtual_files.push_back("ERROR_DECOMPRESS_CHUNK_1.txt");
                    return;
                }
            } else {
                decomp_size = size;
                decomp_fat = std::move(chunk_data);
            }

            // --- ПАРСИНГ FAT ---
            uint32_t ptr = 0;
            int count = 0;
            
            while (ptr < decomp_size) {
                // Структура узла как в CLocatorAPI::LoadArchive
                uint16_t item_size = *(uint16_t*)(decomp_fat.data() + ptr); ptr += 2;
                
                uint32_t size_real = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;
                uint32_t size_compr = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;
                uint32_t crc = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;
                
                int name_length = item_size - 16;
                std::string name(reinterpret_cast<char*>(decomp_fat.data() + ptr), name_length);
                ptr += name_length;
                
                uint32_t file_ptr = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;

                // Заменяем слеши, чтобы вывести просто списком
                std::replace(name.begin(), name.end(), '\\', '_');
                std::replace(name.begin(), name.end(), '/', '_');

                if (count < 200) {
                    virtual_files.push_back(name);
                }
                count++;
            }
            
            virtual_files.push_back("SUCCESS_TOTAL_FILES_" + std::to_string(count) + ".txt");
            break;
        } else {
            // Если это не FAT (например INI или сырые данные) - просто перепрыгиваем
            file.seekg(size, std::ios::cur);
            offset += size;
        }
    }

    if (!found_fat) {
        virtual_files.push_back("ERROR_FAT_CHUNK_NOT_FOUND.txt");
    }
}

// --- Функции Total Commander ---
typedef struct {
    int size;
    DWORD PluginInterfaceVersionLow;
    DWORD PluginInterfaceVersionHi;
    char DefaultIniName[MAX_PATH];
} tfsDefaultParamStruct;

extern "C" {
    __declspec(dllexport) int __stdcall FsInit(int PluginNr, tfsDefaultParamStruct* pDefaultParam) { return 0; }

    __declspec(dllexport) HANDLE __stdcall FsFindFirst(char* path, WIN32_FIND_DATAA* FindData) {
        if (std::string(path) == "\\") ReadFAT();
        current_file_index = 0;
        if (virtual_files.empty()) {
            SetLastError(ERROR_NO_MORE_FILES);
            return INVALID_HANDLE_VALUE;
        }
        memset(FindData, 0, sizeof(WIN32_FIND_DATAA));
        lstrcpynA(FindData->cFileName, virtual_files[current_file_index].c_str(), MAX_PATH);
        FindData->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        current_file_index++;
        return (HANDLE)1; 
    }

    __declspec(dllexport) BOOL __stdcall FsFindNext(HANDLE Hdl, WIN32_FIND_DATAA* FindData) {
        if (current_file_index >= virtual_files.size()) {
            SetLastError(ERROR_NO_MORE_FILES);
            return FALSE;
        }
        memset(FindData, 0, sizeof(WIN32_FIND_DATAA));
        lstrcpynA(FindData->cFileName, virtual_files[current_file_index].c_str(), MAX_PATH);
        FindData->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        current_file_index++;
        return TRUE;
    }

    __declspec(dllexport) int __stdcall FsFindClose(HANDLE Hdl) { return 0; }
    __declspec(dllexport) void __stdcall FsGetDefRootName(char* DefRootName, int maxlen) { lstrcpynA(DefRootName, "Anomaly DB", maxlen); }
}
