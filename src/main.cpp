#include <windows.h>
#include <string>
#include <fstream>
#include <vector>
#include <algorithm>
#include "minilzo.h"

std::vector<std::string> virtual_files;
int current_file_index = 0;

void ReadFAT() {
    virtual_files.clear();
    
    // ВПИШИ СЮДА ПУТЬ К АРХИВУ НА СВОЕМ ПК!
    std::string db_path = "Y:\\ANTHOLOGY\\Anomaly-1.5.3-Anthology 2.1\\db\\configs\\configs.xdb0";  
    
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
        file.seekg(ini_size, std::ios::cur); // Пропускаем текст INI
        
        uint32_t val1 = 0, val2 = 0;
        file.read(reinterpret_cast<char*>(&val1), 4);
        file.read(reinterpret_cast<char*>(&val2), 4);
        
        fat_offset = val2;
        compressed_fat_size = static_cast<uint32_t>(file_size) - fat_offset;
    } else if (version == 29) {
        // Поддержка старых форматов оригинала на всякий случай
        file.read(reinterpret_cast<char*>(&fat_offset), 4);
        file.read(reinterpret_cast<char*>(&compressed_fat_size), 4);
    } else {
        virtual_files.push_back("ERROR_UNSUPPORTED_VERSION_" + std::to_string(version) + ".txt");
        return;
    }

    // Читаем сжатый блок таблицы FAT
    file.seekg(fat_offset, std::ios::beg);
    std::vector<uint8_t> comp_fat(compressed_fat_size);
    file.read(reinterpret_cast<char*>(comp_fat.data()), compressed_fat_size);
    file.close();

    // Готовим буфер на 20 МБ для распаковки
    lzo_uint decomp_size = 20 * 1024 * 1024; 
    std::vector<uint8_t> decomp_fat(decomp_size);
    
    // Распаковываем!
    int r = lzo1x_decompress_safe(comp_fat.data(), comp_fat.size(), decomp_fat.data(), &decomp_size, NULL);
    
    if (r != LZO_E_OK) {
        virtual_files.push_back("ERROR_DECOMPRESS_LZO_" + std::to_string(r) + ".txt");
        return;
    }
    
    // Парсим таблицу файлов
    uint32_t fat_ptr = 0;
    int count = 0;
    
    while (fat_ptr < decomp_size) {
        if (fat_ptr + 12 > decomp_size) break;
        
        uint32_t size_real = *(uint32_t*)(decomp_fat.data() + fat_ptr); fat_ptr += 4;
        uint32_t size_comp = *(uint32_t*)(decomp_fat.data() + fat_ptr); fat_ptr += 4;
        uint32_t crc = *(uint32_t*)(decomp_fat.data() + fat_ptr); fat_ptr += 4;
        
        std::string name = reinterpret_cast<char*>(decomp_fat.data() + fat_ptr);
        fat_ptr += name.length() + 1; // Пропускаем имя и нуль-терминатор
        
        // Заменяем слеши на подчеркивания для плоского вывода
        std::replace(name.begin(), name.end(), '\\', '_');
        std::replace(name.begin(), name.end(), '/', '_');
        
        if (count < 100) {
            virtual_files.push_back(name);
        }
        count++;
    }
    
    virtual_files.push_back("SUCCESS_TOTAL_FILES_PARSED_" + std::to_string(count) + ".txt");
}

// --- Функции Total Commander ---
typedef struct {
    int size;
    DWORD PluginInterfaceVersionLow;
    DWORD PluginInterfaceVersionHi;
    char DefaultIniName[MAX_PATH];
} tfsDefaultParamStruct;

extern "C" {
    __declspec(dllexport) int __stdcall FsInit(int PluginNr, tfsDefaultParamStruct* pDefaultParam) {
        if (lzo_init() != LZO_E_OK) return -1;
        return 0;
    }

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
