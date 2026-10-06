#include <windows.h>
#include <string>
#include <fstream>
#include <vector>
#include <algorithm>

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

    // Сканируем все чанки
    while (offset < file_size) {
        uint32_t type = 0, size = 0;
        if (!file.read(reinterpret_cast<char*>(&type), 4)) break;
        if (!file.read(reinterpret_cast<char*>(&size), 4)) break;
        offset += 8;

        uint32_t id = type & 0x7FFFFFFF;
        bool is_comp = (type & 0x80000000) != 0;

        char buf[256];
        snprintf(buf, sizeof(buf), "CHUNK_ID_%u_%s_SIZE_%u.txt", id, is_comp ? "COMPRESSED" : "RAW", size);
        virtual_files.push_back(std::string(buf));

        if (id == 1) { // Это наш FAT!
            std::vector<uint8_t> chunk_data(size);
            file.read(reinterpret_cast<char*>(chunk_data.data()), size);
            
            std::string dump_path = "C:\\Users\\Public\\anomaly_chunk1_dump.txt"; 
            std::ofstream dump(dump_path);
            if (dump.is_open()) {
                dump << "CHUNK 1 (FAT) DUMP:\n";
                for (int i = 0; i < min((int)size, 256); i++) {
                    char hex[8];
                    snprintf(hex, sizeof(hex), "%02X ", chunk_data[i]);
                    dump << hex;
                    if ((i + 1) % 16 == 0) dump << "\n";
                }
                dump << "\nASCII:\n";
                for (int i = 0; i < min((int)size, 256); i++) {
                    if (chunk_data[i] >= 32 && chunk_data[i] <= 126) dump << (char)chunk_data[i];
                    else dump << ".";
                    if ((i + 1) % 64 == 0) dump << "\n";
                }
                dump.close();
                virtual_files.push_back("CHUNK1_DUMP_SAVED_TO_PUBLIC.txt");
            }
            
            offset += size;
        } else {
            file.seekg(size, std::ios::cur);
            offset += size;
        }
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
