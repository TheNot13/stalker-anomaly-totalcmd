#include <windows.h>
#include <string>
#include <fstream>
#include <vector>

std::vector<std::string> virtual_files;
int current_file_index = 0;

void DumpFAT() {
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
    uint32_t ini_size = 0;
    
    file.read(reinterpret_cast<char*>(&version), 4);
    file.read(reinterpret_cast<char*>(&ini_size), 4);
    
    // Пропускаем INI текст
    file.seekg(ini_size, std::ios::cur);
    
    uint32_t val1 = 0;
    uint32_t val2 = 0;
    file.read(reinterpret_cast<char*>(&val1), 4);
    file.read(reinterpret_cast<char*>(&val2), 4);

    // В Anomaly 666 смещение на FAT лежит во второй переменной
    uint32_t fat_offset = val2;

    if (fat_offset == 0 || fat_offset >= file_size) {
        virtual_files.push_back("ERROR_INVALID_FAT_OFFSET.txt");
        return;
    }

    // Прыгаем в конец архива к таблице файлов
    file.seekg(fat_offset, std::ios::beg);
    
    uint8_t buf[256] = {0};
    file.read(reinterpret_cast<char*>(buf), 256);
    file.close();

    // Записываем кусок FAT в файл
    std::string dump_path = "C:\\Users\\Public\\anomaly_fat_dump.txt"; 
    std::ofstream dump(dump_path);
    if (dump.is_open()) {
        dump << "FILE SIZE: " << file_size << "\n";
        dump << "FAT OFFSET: " << fat_offset << "\n";
        dump << "VAL1 (fat size?): " << val1 << "\n\n";
        dump << "HEX DUMP OF FAT:\n";
        for (int i = 0; i < 256; i++) {
            char hex[8];
            snprintf(hex, sizeof(hex), "%02X ", buf[i]);
            dump << hex;
            if ((i + 1) % 16 == 0) dump << "\n";
        }
        dump << "\nASCII DUMP OF FAT:\n";
        for (int i = 0; i < 256; i++) {
            if (buf[i] >= 32 && buf[i] <= 126) dump << (char)buf[i];
            else dump << ".";
            if ((i + 1) % 64 == 0) dump << "\n";
        }
        dump.close();
        virtual_files.push_back("FAT_DUMP_SAVED.txt");
    } else {
        virtual_files.push_back("ERROR_WRITE_DUMP.txt");
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
        if (std::string(path) == "\\") DumpFAT();
        
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
