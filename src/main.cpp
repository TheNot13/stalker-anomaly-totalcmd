#include <windows.h>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include "minilzo.h"

// --- Структуры X-Ray Anomaly (Формат 29) ---
#pragma pack(push, 1)
struct DbHeader {
    uint32_t version;     // Ожидаем 29 (0x1D)
    uint32_t fat_offset;  // Смещение таблицы FAT в байтах
    uint32_t fat_size;    // Размер сжатой FAT
};
#pragma pack(pop)
// -------------------------------------------

// Глобальные переменные для тестов
std::vector<std::string> virtual_files;
int current_file_index = 0;

// Функция: читает начало реального архива и выводит инфу как фейковые файлы
void TestReadArchive() {
    virtual_files.clear();
    
    // ВПИШИ СЮДА ПУТЬ К АРХИВУ НА СВОЕМ ПК!
    std::string db_path = "Y:\\ANTHOLOGY\\Anomaly-1.5.3-Anthology 2.1\\db\\configs\\configs.xdb0"; 
    
    std::ifstream file(db_path, std::ios::binary);
    if (!file.is_open()) {
        virtual_files.push_back("ERROR_FILE_NOT_FOUND.txt");
        return;
    }

    DbHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(DbHeader));

    std::stringstream ss;
    ss << "VERSION_" << header.version << ".txt";
    virtual_files.push_back(ss.str());

    ss.str("");
    ss << "FAT_OFFSET_" << header.fat_offset << ".txt";
    virtual_files.push_back(ss.str());

    ss.str("");
    ss << "FAT_SIZE_" << header.fat_size << ".txt";
    virtual_files.push_back(ss.str());
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
        // Инициализируем библиотеку LZO
        if (lzo_init() != LZO_E_OK) {
            return -1; 
        }
        return 0;
    }

    __declspec(dllexport) HANDLE __stdcall FsFindFirst(char* path, WIN32_FIND_DATAA* FindData) {
        // Если просят корень плагина, читаем архив
        if (std::string(path) == "\\") {
            TestReadArchive();
        }
        
        current_file_index = 0;

        if (virtual_files.empty()) {
            SetLastError(ERROR_NO_MORE_FILES);
            return INVALID_HANDLE_VALUE;
        }

        // Отдаем первый "файл"
        memset(FindData, 0, sizeof(WIN32_FIND_DATAA));
        lstrcpynA(FindData->cFileName, virtual_files[current_file_index].c_str(), MAX_PATH);
        FindData->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        
        current_file_index++;
        return (HANDLE)1; // Возвращаем фейковый handle
    }

    __declspec(dllexport) BOOL __stdcall FsFindNext(HANDLE Hdl, WIN32_FIND_DATAA* FindData) {
        if (current_file_index >= virtual_files.size()) {
            SetLastError(ERROR_NO_MORE_FILES);
            return FALSE;
        }

        // Отдаем следующий "файл"
        memset(FindData, 0, sizeof(WIN32_FIND_DATAA));
        lstrcpynA(FindData->cFileName, virtual_files[current_file_index].c_str(), MAX_PATH);
        FindData->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;

        current_file_index++;
        return TRUE;
    }

    __declspec(dllexport) int __stdcall FsFindClose(HANDLE Hdl) {
        return 0;
    }

    __declspec(dllexport) void __stdcall FsGetDefRootName(char* DefRootName, int maxlen) {
        lstrcpynA(DefRootName, "Anomaly DB", maxlen);
    }
}
