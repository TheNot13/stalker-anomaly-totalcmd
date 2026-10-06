#include <windows.h>
#include "vfs.h"

VfsNode vfs_root;
bool vfs_loaded = false;

struct FindItem {
    std::string name;
    bool is_dir;
    uint32_t size;
};

std::vector<FindItem> current_find_items;
int current_find_index = 0;

std::string GetIniPath() {
    char path[MAX_PATH];
    HMODULE hm = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&GetIniPath, &hm);
    GetModuleFileNameA(hm, path, sizeof(path));
    std::string full_path(path);
    size_t pos = full_path.find_last_of("\\/");
    if (pos != std::string::npos) return full_path.substr(0, pos) + "\\anomaly_db.ini";
    return "anomaly_db.ini";
}

void BuildVFS() {
    vfs_root.children.clear();
    
    char result[MAX_PATH];
    GetPrivateProfileStringA("Settings", "GamePath", "", result, MAX_PATH, GetIniPath().c_str());
    std::string game_path(result);
    
    if (game_path.empty() || !fs::exists(game_path)) {
        VfsNode err;
        err.name = "ERROR_GAMEPATH_NOT_FOUND_IN_INI.txt";
        err.is_dir = false;
        vfs_root.children[err.name] = err;
        return;
    }

    std::vector<std::string> archives;
    
    // Ищем все .db и .xdb файлы в папке игры (включая вложенные)
    for (const auto& entry : fs::recursive_directory_iterator(game_path)) {
        if (entry.is_regular_file()) {
            std::string name = entry.path().filename().string();
            if (name.find(".db") != std::string::npos || name.find(".xdb") != std::string::npos) {
                archives.push_back(entry.path().string());
            }
        }
    }

    // Сортировка по алфавиту ОБЯЗАТЕЛЬНА! (Так патчи db1 перекроют db0)
    std::sort(archives.begin(), archives.end());

    for (const auto& arc : archives) {
        ParseArchive(arc);
    }
}

// --- WFX API ---
typedef struct { int size; DWORD vLow; DWORD vHi; char ini[MAX_PATH]; } tfsDefaultParamStruct;

extern "C" {
    __declspec(dllexport) int __stdcall FsInit(int PluginNr, tfsDefaultParamStruct* pDefaultParam) { return 0; }

    __declspec(dllexport) HANDLE __stdcall FsFindFirst(char* path, WIN32_FIND_DATAA* FindData) {
        if (!vfs_loaded) {
            BuildVFS();
            vfs_loaded = true;
        }

        current_find_items.clear();
        current_find_index = 0;

        std::string search_path(path);
        std::vector<std::string> parts = SplitPath(search_path);
        
        VfsNode* current = &vfs_root;
        bool found = true;
        for (const std::string& p : parts) {
            if (current->children.find(p) != current->children.end()) {
                current = &current->children[p];
            } else {
                found = false;
                break;
            }
        }

        if (!found || current->children.empty()) {
            SetLastError(ERROR_NO_MORE_FILES);
            return INVALID_HANDLE_VALUE;
        }

        for (auto const& [key, val] : current->children) {
            current_find_items.push_back({val.name, val.is_dir, val.size_real});
        }

        memset(FindData, 0, sizeof(WIN32_FIND_DATAA));
        lstrcpynA(FindData->cFileName, current_find_items[0].name.c_str(), MAX_PATH);
        
        if (current_find_items[0].is_dir) {
            FindData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        } else {
            FindData->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
            FindData->nFileSizeLow = current_find_items[0].size;
        }
        
        current_find_index = 1;
        return (HANDLE)1; 
    }

    __declspec(dllexport) BOOL __stdcall FsFindNext(HANDLE Hdl, WIN32_FIND_DATAA* FindData) {
        if (current_find_index >= current_find_items.size()) {
            SetLastError(ERROR_NO_MORE_FILES);
            return FALSE;
        }
        
        memset(FindData, 0, sizeof(WIN32_FIND_DATAA));
        lstrcpynA(FindData->cFileName, current_find_items[current_find_index].name.c_str(), MAX_PATH);
        
        if (current_find_items[current_find_index].is_dir) {
            FindData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        } else {
            FindData->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
            FindData->nFileSizeLow = current_find_items[current_find_index].size;
        }
        
        current_find_index++;
        return TRUE;
    }

    __declspec(dllexport) int __stdcall FsFindClose(HANDLE Hdl) { return 0; }
    __declspec(dllexport) void __stdcall FsGetDefRootName(char* DefRootName, int maxlen) { lstrcpynA(DefRootName, "Anomaly DB", maxlen); }
}
