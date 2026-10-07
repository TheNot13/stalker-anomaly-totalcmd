#include <windows.h>
#include "vfs.h"
#include "minilzo.h"

#define FS_FILE_OK 0
#define FS_FILE_NOTFOUND 2
#define FS_FILE_READERROR 3
#define FS_FILE_WRITEERROR 4

VfsNode vfs_root;
bool vfs_loaded = false;

struct FindItem {
    std::string name;
    bool is_dir;
    uint32_t size;
    FILETIME ft;
};

std::vector<FindItem> current_find_items;
int current_find_index = 0;

void Logger(const std::string& msg) {
    std::ofstream log("C:\\Users\\Public\\anomaly_wfx.log", std::ios::app);
    if (log.is_open()) log << msg << "\n";
}

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
    
    Logger("=== СКАНИРОВАНИЕ ДИРЕКТОРИИ: " + game_path + " ===");
    if (game_path.empty() || !stdfs::exists(game_path)) return;

    std::vector<std::string> archives;
    try {
        for (const auto& entry : stdfs::recursive_directory_iterator(game_path)) {
            if (entry.is_regular_file()) {
                std::string name = entry.path().filename().string();
                if (name.find(".db") != std::string::npos || name.find(".xdb") != std::string::npos) {
                    archives.push_back(entry.path().string());
                }
            }
        }
        std::sort(archives.begin(), archives.end());
        for (const auto& arc : archives) ParseArchive(arc);
    } catch (...) {}
    Logger("=== СКАНИРОВАНИЕ ЗАВЕРШЕНО ===");
}

VfsNode* FindNode(const std::string& path) {
    if (path == "\\" || path.empty()) return &vfs_root;
    size_t start = 0;
    if (path[0] == '\\') start = 1;
    
    VfsNode* current = &vfs_root;
    size_t end = path.find('\\', start);

    while (end != std::string::npos) {
        std::string part = path.substr(start, end - start);
        if (current->children.find(part) != current->children.end()) {
            current = &current->children[part];
        } else return nullptr;
        start = end + 1;
        end = path.find('\\', start);
    }

    if (start < path.length()) {
        std::string part = path.substr(start);
        if (current->children.find(part) != current->children.end()) {
            return &current->children[part];
        } else return nullptr;
    }
    return current;
}

typedef struct { int size; DWORD vLow; DWORD vHi; char ini[MAX_PATH]; } tfsDefaultParamStruct;
typedef struct { DWORD sizeLow; DWORD sizeHigh; FILETIME lastWriteTime; int attr; } RemoteInfoStruct;

extern "C" {
    __declspec(dllexport) int __stdcall FsInit(int PluginNr, tfsDefaultParamStruct* pDefaultParam) { 
        lzo_init();
        std::ofstream log("C:\\Users\\Public\\anomaly_wfx.log", std::ios::trunc);
        log << "Anomaly WFX Plugin Initialized\n";
        return 0; 
    }

    __declspec(dllexport) HANDLE __stdcall FsFindFirst(char* path, WIN32_FIND_DATAA* FindData) {
        if (!vfs_loaded) { BuildVFS(); vfs_loaded = true; }
        current_find_items.clear();
        current_find_index = 0;

        VfsNode* current = FindNode(path);
        if (!current || current->children.empty()) {
            SetLastError(ERROR_NO_MORE_FILES);
            return INVALID_HANDLE_VALUE;
        }

        for (auto const& [key, val] : current->children) {
            current_find_items.push_back({val.name, val.is_dir, val.size_real, val.time_write});
        }

        memset(FindData, 0, sizeof(WIN32_FIND_DATAA));
        lstrcpynA(FindData->cFileName, current_find_items[0].name.c_str(), MAX_PATH);
        FindData->ftLastWriteTime = current_find_items[0].ft;
        
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
        FindData->ftLastWriteTime = current_find_items[current_find_index].ft;
        
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

    // Извлечение файлов
    __declspec(dllexport) int __stdcall FsGetFile(char* RemoteName, char* LocalName, int CopyFlags, RemoteInfoStruct* ri) {
        VfsNode* node = FindNode(RemoteName);
        if (!node || node->is_dir) return FS_FILE_NOTFOUND;

        std::ifstream file(node->archive_path, std::ios::binary);
        if (!file.is_open()) return FS_FILE_READERROR;

        file.seekg(node->offset, std::ios::beg);
        std::vector<u8> comp_data(node->size_compr);
        file.read(reinterpret_cast<char*>(comp_data.data()), node->size_compr);

        std::ofstream out(LocalName, std::ios::binary);
        if (!out.is_open()) return FS_FILE_WRITEERROR;

        if (node->size_real == node->size_compr) {
            out.write(reinterpret_cast<char*>(comp_data.data()), node->size_real);
        } else {
            // Пробуем LZO
            std::vector<u8> decomp_data(node->size_real);
            lzo_uint out_len = node->size_real;
            int r = lzo1x_decompress_safe(comp_data.data(), comp_data.size(), decomp_data.data(), &out_len, NULL);
            if (r == LZO_E_OK) {
                out.write(reinterpret_cast<char*>(decomp_data.data()), out_len);
            } else {
                // Если не LZO, пробуем LzHuf
                std::vector<u8> dec;
                if (DecompressLzHuf(comp_data.data(), (u32)comp_data.size(), dec)) {
                    out.write(reinterpret_cast<char*>(dec.data()), dec.size());
                } else {
                    return FS_FILE_READERROR;
                }
            }
        }
        return FS_FILE_OK;
    }
}
