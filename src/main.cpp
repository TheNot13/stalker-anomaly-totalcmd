#include <windows.h>

// Базовая структура Total Commander
typedef struct {
    int size;
    DWORD PluginInterfaceVersionLow;
    DWORD PluginInterfaceVersionHi;
    char DefaultIniName[MAX_PATH];
} tfsDefaultParamStruct;

// Экспортируем функции, чтобы Total Commander увидел плагин
extern "C" {

    __declspec(dllexport) int __stdcall FsInit(int PluginNr, tfsDefaultParamStruct* pDefaultParam) {
        // Инициализация плагина
        return 0;
    }

    __declspec(dllexport) HANDLE __stdcall FsFindFirst(char* path, WIN32_FIND_DATAA* FindData) {
        // Заглушка: TC спросит "что в папке?", мы ответим "ничего нет"
        SetLastError(ERROR_NO_MORE_FILES);
        return INVALID_HANDLE_VALUE;
    }

    __declspec(dllexport) BOOL __stdcall FsFindNext(HANDLE Hdl, WIN32_FIND_DATAA* FindData) {
        return FALSE;
    }

    __declspec(dllexport) int __stdcall FsFindClose(HANDLE Hdl) {
        return 0;
    }

    __declspec(dllexport) void __stdcall FsGetDefRootName(char* DefRootName, int maxlen) {
        // Как диск будет называться в сетевом окружении TC
        lstrcpynA(DefRootName, "Anomaly DB", maxlen);
    }
}
