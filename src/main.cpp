#include <windows.h>
#include <string>
#include <fstream>
#include <vector>
#include <algorithm>
#include "lz4.h"

std::vector<std::string> virtual_files;
int current_file_index = 0;

// Функция для получения пути к нашему INI файлу
std::string GetIniPath() {
    char path[MAX_PATH];
    HMODULE hm = NULL;
    // Берем адрес самой этой функции, чтобы винда поняла, в какой мы DLL
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&GetIniPath, &hm);
    GetModuleFileNameA(hm, path, sizeof(path));
    std::string full_path(path);
    size_t pos = full_path.find_last_of("\\/");
    if (pos != std::string::npos) {
        return full_path.substr(0, pos) + "\\anomaly_db.ini";
    }
    return "anomaly_db.ini";
}

// Функция чтения пути к .db из INI файла
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
        virtual_files.push_back(GetIniPath()); // Выведет путь, где плагин ищет INI
        return;
    }
    
    std::ifstream file(db_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        virtual_files.push_back("ERROR_FILE_NOT_FOUND.txt");
        return;
    }
    
    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    uint32_t version = 0;
    file.read(reinterpret_cast<char*>(&version), 4);
    
    uint32_t fat_offset = 0;
    uint32_t compressed_fat_size = 0;
    
    if (version == 666) {
        uint32_t ini_size = 0;
        file.read(reinterpret_cast<char*>(&ini_size), 4);
        file.seekg(ini_size, std::ios::cur); 
        
        uint32_t val1 = 0, val2 = 0;
        file.read(reinterpret_cast<char*>(&val1), 4);
        file.read(reinterpret_cast<char*>(&val2), 4);
        
        fat_offset = val2;
        compressed_fat_size = static_cast<uint32_t>(file_size) - fat_offset;
    } else {
        virtual_files.push_back("ERROR_UNSUPPORTED_VERSION.txt");
        return;
    }

    file.seekg(fat_offset, std::ios::beg);
    std::vector<uint8_t> comp_fat(compressed_fat_size);
    file.read(reinterpret_cast<char*>(comp_fat.data()), compressed_fat_size);
    file.close();

    // Готовим буфер на 20 МБ для распаковки
    int max_decomp_size = 20 * 1024 * 1024; 
    std::vector<uint8_t> decomp_fat(max_decomp_size);
    
    // Распаковываем через LZ4!
    int decomp_size = LZ4_decompress_safe(
        reinterpret_cast<const char*>(comp_fat.data()), 
        reinterpret_cast<char*>(decomp_fat.data()), 
        compressed_fat_size, 
        max_decomp_size
    );
    
    if (decomp_size < 0) {
        virtual_files.push_back("ERROR_DECOMPRESS_LZ4_" + std::to_string(decomp_size) + ".txt");
        return;
    }
    
    // Парсим таблицу файлов
    uint32_t fat_ptr = 0;
    int count = 0;
    
    while (fat_ptr < (uint32_t)decomp_size) {
        if (fat_ptr + 12 > (uint32_t)decomp_size) break;
        
        uint32_t size_real = *(uint32_t*)(decomp_fat.data() + fat_ptr); fat_ptr += 4;
        uint32_t size_comp = *(uint32_t*)(decomp_fat.data() + fat_ptr); fat_ptr += 4;
        uint32_t crc = *(uint32_t*)(decomp_fat.data() + fat_ptr); fat_ptr += 4;
        
        std::string name = reinterpret_cast<char*>(decomp_fat.data() + fat_ptr);
        fat_ptr += name.length() + 1; 
        
        // Заменяем слеши на подчеркивания
        std::replace(name.begin(), name.end(), '\\', '_');
        std::replace(name.begin(), name.end(), '/', '_');
        
        if (count < 100) {
            virtual_files.push_back(name);
        }
        count++;
    }
    
    virtual_files.push_back("SUCCESS_TOTAL_FILES_" + std::to_string(count) + ".txt");
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
