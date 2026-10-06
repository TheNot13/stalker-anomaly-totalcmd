#include <windows.h>
#include <string>
#include <fstream>
#include <vector>
#include <algorithm>

typedef unsigned char u8;
typedef unsigned int u32;
#define IC inline
#define xr_malloc malloc
#define xr_free free
#define xr_realloc realloc

// --- ОРИГИНАЛЬНЫЙ ДЕКОДЕР LZHUF ИЗ ДВИЖКА X-RAY ---
#define N 4096 
#define F 60 
#define THRESHOLD 2
#define NIL N 
#define N_CHAR (256 - THRESHOLD + F) 
#define T (N_CHAR * 2 - 1) 
#define R (T - 1) 
#define MAX_FREQ 0x4000 

u8 text_buf[N + F];
unsigned freq[T + 1]; 
int prnt[T + N_CHAR + 1]; 
int son[T]; 

u8 d_code[256] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
    0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
    0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,
    0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
    0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,
    0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,
    0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,
    0x0C,0x0C,0x0C,0x0C,0x0D,0x0D,0x0D,0x0D,0x0E,0x0E,0x0E,0x0E,0x0F,0x0F,0x0F,0x0F,
    0x10,0x10,0x10,0x10,0x11,0x11,0x11,0x11,0x12,0x12,0x12,0x12,0x13,0x13,0x13,0x13,
    0x14,0x14,0x14,0x14,0x15,0x15,0x15,0x15,0x16,0x16,0x16,0x16,0x17,0x17,0x17,0x17,
    0x18,0x18,0x19,0x19,0x1A,0x1A,0x1B,0x1B,0x1C,0x1C,0x1D,0x1D,0x1E,0x1E,0x1F,0x1F,
    0x20,0x20,0x21,0x21,0x22,0x22,0x23,0x23,0x24,0x24,0x25,0x25,0x26,0x26,0x27,0x27,
    0x28,0x28,0x29,0x29,0x2A,0x2A,0x2B,0x2B,0x2C,0x2C,0x2D,0x2D,0x2E,0x2E,0x2F,0x2F,
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F
};

u8 d_len[256] = {
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8
};

class LZfs {
public:
    unsigned getbuf;
    unsigned getlen;
    u8* in_start;
    u8* in_end;
    u8* in_iterator;
    u8* out_start;
    u8* out_end;
    u8* out_iterator;

    IC int _getb() {
        if (in_iterator == in_end) return EOF;
        return *in_iterator++;
    }

    IC void _putb(int c) {
        if (out_iterator == out_end) {
            u32 out_size = u32(out_end - out_start);
            out_start = (u8*)xr_realloc(out_start, out_size + 1024);
            out_iterator = out_start + out_size;
            out_end = out_iterator + 1024;
        }
        *out_iterator++ = (u8)(c & 0xFF);
    }

    LZfs() {
        in_start = in_end = in_iterator = 0;
        out_start = out_end = out_iterator = 0;
    }

    IC void Init_Input(u8* _start, u8* _end) {
        in_start = _start; in_end = _end; in_iterator = in_start;
        getbuf = getlen = 0;
    }

    IC void Init_Output(int _rsize) {
        out_start = (u8*)xr_malloc(_rsize);
        out_end = out_start + _rsize;
        out_iterator = out_start;
    }

    IC u32 OutSize() { return u32(out_iterator - out_start); }
    IC u8* OutPointer() { return out_start; }

    IC int GetBit(void) {
        unsigned i;
        while (getlen <= 8) {
            if ((int)(i = _getb()) < 0) i = 0;
            getbuf |= i << (8 - getlen);
            getlen += 8;
        }
        i = getbuf;
        getbuf <<= 1;
        getlen--;
        return (int)((i & 0x8000) >> 15);
    }

    IC int GetByte(void) {
        unsigned i;
        while (getlen <= 8) {
            if ((int)(i = _getb()) < 0) i = 0;
            getbuf |= i << (8 - getlen);
            getlen += 8;
        }
        i = getbuf;
        getbuf <<= 8;
        getlen -= 8;
        return (int)((i & 0xff00) >> 8);
    }
};

LZfs fs;

void StartHuff(void) {
    int i, j;
    for (i = 0; i < N_CHAR; i++) {
        freq[i] = 1;
        son[i] = i + T;
        prnt[i + T] = i;
    }
    i = 0; j = N_CHAR;
    while (j <= R) {
        freq[j] = freq[i] + freq[i + 1];
        son[j] = i;
        prnt[i] = prnt[i + 1] = j;
        i += 2; j++;
    }
    freq[T] = 0xffff;
    prnt[R] = 0;
}

void reconst(void) {
    int i, j, k;
    unsigned f, l;
    j = 0;
    for (i = 0; i < T; i++) {
        if (son[i] >= T) {
            freq[j] = (freq[i] + 1) / 2;
            son[j] = son[i];
            j++;
        }
    }
    for (i = 0, j = N_CHAR; j < T; i += 2, j++) {
        k = i + 1;
        f = freq[j] = freq[i] + freq[k];
        for (k = j - 1; f < freq[k]; k--);
        k++;
        l = (j - k) * sizeof(unsigned);
        memmove(&freq[k + 1], &freq[k], l);
        freq[k] = f;
        memmove(&son[k + 1], &son[k], l);
        son[k] = i;
    }
    for (i = 0; i < T; i++) {
        if ((k = son[i]) >= T) {
            prnt[k] = i;
        } else {
            prnt[k] = prnt[k + 1] = i;
        }
    }
}

void update(int c) {
    int i, j, k, l;
    if (freq[R] == MAX_FREQ) reconst();
    c = prnt[c + T];
    do {
        k = ++freq[c];
        if ((unsigned)k > freq[l = c + 1]) {
            while ((unsigned)k > freq[++l]);
            l--;
            freq[c] = freq[l];
            freq[l] = k;
            i = son[c];
            prnt[i] = l;
            if (i < T) prnt[i + 1] = l;
            j = son[l];
            son[l] = i;
            prnt[j] = c;
            if (j < T) prnt[j + 1] = c;
            son[c] = j;
            c = l;
        }
    } while ((c = prnt[c]) != 0);
}

int DecodeChar(void) {
    unsigned c = son[R];
    while (c < T) {
        c += fs.GetBit();
        c = son[c];
    }
    c -= T;
    update(c);
    return (int)c;
}

int DecodePosition(void) {
    unsigned i, j, c;
    i = fs.GetByte();
    c = (unsigned)d_code[i] << 6;
    j = d_len[i];
    j -= 2;
    while (j--) i = (i << 1) + fs.GetBit();
    return (int)(c | (i & 0x3f));
}

void Decode(void) {
    int i, j, k, r, c;
    unsigned int count;
    unsigned int textsize = (fs._getb());
    textsize |= (fs._getb() << 8);
    textsize |= (fs._getb() << 16);
    textsize |= (fs._getb() << 24);
    if (textsize == 0) return;

    fs.Init_Output(textsize);
    StartHuff();
    for (i = 0; i < N - F; i++) text_buf[i] = 0x20;
    r = N - F;
    
    for (count = 0; count < textsize;) {
        c = DecodeChar();
        if (c < 256) {
            fs._putb(c);
            text_buf[r++] = (unsigned char)c;
            r &= (N - 1);
            count++;
        } else {
            i = (r - DecodePosition() - 1) & (N - 1);
            j = c - 255 + THRESHOLD;
            for (k = 0; k < j; k++) {
                c = text_buf[(i + k) & (N - 1)];
                fs._putb(c);
                text_buf[r++] = (unsigned char)c;
                r &= (N - 1);
                count++;
            }
        }
    }
}
// --- ЛОГИКА ПЛАГИНА (ВИРТУАЛЬНОЕ ДЕРЕВО) ---
#include <map>
#include <sstream>

struct VfsNode {
    std::string name;
    bool is_dir;
    uint32_t size_real;
    uint32_t offset;
    std::map<std::string, VfsNode> children;
};

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

// Функция разбивки пути "configs\weapons\w_ak.ltx" на куски
std::vector<std::string> SplitPath(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string item;
    while (std::getline(ss, item, '\\')) {
        if (!item.empty()) parts.push_back(item);
    }
    return parts;
}

void ReadFAT() {
    vfs_root.children.clear();
    
    char result[MAX_PATH];
    GetPrivateProfileStringA("Settings", "DbPath", "", result, MAX_PATH, GetIniPath().c_str());
    std::string db_path(result);
    
    std::ifstream file(db_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        vfs_root.children["ERROR_FILE_NOT_FOUND.txt"] = {"ERROR_FILE_NOT_FOUND.txt", false, 0, 0};
        return;
    }
    
    uint32_t file_size = static_cast<uint32_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    uint32_t offset = 0;

    while (offset < file_size) {
        uint32_t type = 0, size = 0;
        if (!file.read(reinterpret_cast<char*>(&type), 4)) break;
        if (!file.read(reinterpret_cast<char*>(&size), 4)) break;
        offset += 8;

        uint32_t chunk_id = type & 0x7FFFFFFF;

        if (chunk_id == 1) { // Это FAT!
            std::vector<uint8_t> chunk_data(size);
            file.read(reinterpret_cast<char*>(chunk_data.data()), size);

            fs.Init_Input(chunk_data.data(), chunk_data.data() + size);
            Decode();
            
            u8* decomp_fat = fs.OutPointer();
            uint32_t decomp_size = fs.OutSize();

            uint32_t ptr = 0;
            // Убрали лимит в 200 файлов, теперь читаем ВСЁ архивы!
            while (ptr < decomp_size) {
                if (ptr + 2 > decomp_size) break;
                uint16_t item_size = *(uint16_t*)(decomp_fat + ptr); ptr += 2;
                if (ptr - 2 + item_size > decomp_size) break;
                
                uint32_t size_real = *(uint32_t*)(decomp_fat + ptr); ptr += 4;
                uint32_t size_compr = *(uint32_t*)(decomp_fat + ptr); ptr += 4;
                uint32_t crc = *(uint32_t*)(decomp_fat + ptr); ptr += 4;
                
                int name_length = item_size - 16;
                std::string name((char*)(decomp_fat + ptr), name_length);
                ptr += name_length;
                
                uint32_t file_ptr = *(uint32_t*)(decomp_fat + ptr); ptr += 4;

                // Строим дерево!
                std::vector<std::string> parts = SplitPath(name);
                VfsNode* current = &vfs_root;
                
                for (size_t i = 0; i < parts.size(); ++i) {
                    const std::string& part = parts[i];
                    if (current->children.find(part) == current->children.end()) {
                        VfsNode node;
                        node.name = part;
                        node.is_dir = (i < parts.size() - 1); 
                        node.size_real = 0;
                        node.offset = 0;
                        current->children[part] = node;
                    }
                    current = &current->children[part];
                }
                
                // Финальный узел - это сам файл
                current->is_dir = false;
                current->size_real = size_real;
                current->offset = file_ptr; 
            }
            free(decomp_fat);
            break;
        } else {
            file.seekg(size, std::ios::cur);
            offset += size;
        }
    }
}

// --- WFX API (Работа с Total Commander) ---
typedef struct { int size; DWORD vLow; DWORD vHi; char ini[MAX_PATH]; } tfsDefaultParamStruct;

extern "C" {
    __declspec(dllexport) int __stdcall FsInit(int PluginNr, tfsDefaultParamStruct* pDefaultParam) { return 0; }

    __declspec(dllexport) HANDLE __stdcall FsFindFirst(char* path, WIN32_FIND_DATAA* FindData) {
        // Читаем архив только один раз при входе
        if (!vfs_loaded) {
            ReadFAT();
            vfs_loaded = true;
        }

        current_find_items.clear();
        current_find_index = 0;

        // Ищем папку, в которую зашел пользователь
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

        // Если папка пустая или не найдена
        if (!found || current->children.empty()) {
            SetLastError(ERROR_NO_MORE_FILES);
            return INVALID_HANDLE_VALUE;
        }

        // Выгружаем содержимое папки для Total Commander
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
