#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>
#include "7z_sdk.h"
#include "lzhuf.h"
#include "minilzo.h"

struct ArchiveItem {
    std::string name;
    uint32_t size_real = 0;
    uint32_t size_compr = 0;
    uint32_t offset = 0;
    uint32_t crc = 0;
    bool is_dir = false;
};

inline HRESULT ReadExact(IInStream *stream, void *data, UInt32 size) {
    UInt32 processed = 0;
    HRESULT res = stream->Read(data, size, &processed);
    if (FAILED(res)) return res;
    if (processed != size) return S_FALSE;
    return S_OK;
}

class CAnomalyArchive : public IInArchive {
private:
    LONG _refCount = 1;
    IInStream* _stream = nullptr;
    std::vector<ArchiveItem> _items;

public:
    CAnomalyArchive() {}
    virtual ~CAnomalyArchive() { Close(); }

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void **ppvObject) override {
        if (!ppvObject) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IInArchive) {
            *ppvObject = static_cast<IInArchive*>(this);
            AddRef();
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHOD_(ULONG, AddRef)() override {
        return InterlockedIncrement(&_refCount);
    }

    STDMETHOD_(ULONG, Release)() override {
        LONG res = InterlockedDecrement(&_refCount);
        if (res == 0) delete this;
        return res;
    }

    // IInArchive
    STDMETHOD(Open)(IInStream *stream, const UInt64 *maxCheckStartPosition, IArchiveOpenCallback *openArchiveCallback) override {
        Close();
        if (!stream) return E_INVALIDARG;

        UInt64 fileSize = 0;
        if (FAILED(stream->Seek(0, STREAM_SEEK_END, &fileSize))) return S_FALSE;
        if (FAILED(stream->Seek(0, STREAM_SEEK_SET, NULL))) return S_FALSE;
        if (fileSize < 16) return S_FALSE;

        UInt64 offset = 0;
        std::string entry_point_prefix = "";
        bool fat_found = false;

        while (offset + 8 <= fileSize) {
            uint32_t type = 0, size = 0;
            if (ReadExact(stream, &type, 4) != S_OK) break;
            if (ReadExact(stream, &size, 4) != S_OK) break;
            offset += 8;

            uint32_t chunk_id = type & 0x7FFFFFFF;
            bool is_comp = (type & 0x80000000) != 0;

            if (size > (fileSize - offset)) break;

            if (chunk_id == 666 || chunk_id == 29) {
                // Считываем префикс gamedata\ из INI-заголовка
                std::string ini_data(size, '\0');
                if (ReadExact(stream, ini_data.data(), size) != S_OK) break;
                offset += size;

                size_t pos = ini_data.find("entry_point");
                if (pos != std::string::npos) {
                    size_t eq_pos = ini_data.find('=', pos);
                    if (eq_pos != std::string::npos) {
                        size_t start = ini_data.find_first_not_of(" \t", eq_pos + 1);
                        size_t end = ini_data.find_first_of("\r\n", start);
                        if (start != std::string::npos && end != std::string::npos) {
                            std::string ep = ini_data.substr(start, end - start);
                            size_t d2 = ep.rfind('$');
                            if (d2 != std::string::npos && d2 + 1 < ep.length()) {
                                entry_point_prefix = ep.substr(d2 + 1);
                            } else {
                                entry_point_prefix = ep;
                            }
                            if (!entry_point_prefix.empty() && (entry_point_prefix[0] == '\\' || entry_point_prefix[0] == '/')) {
                                entry_point_prefix = entry_point_prefix.substr(1);
                            }
                            std::replace(entry_point_prefix.begin(), entry_point_prefix.end(), '/', '\\');
                            if (!entry_point_prefix.empty() && entry_point_prefix.back() != '\\') {
                                entry_point_prefix += "\\";
                            }
                        }
                    }
                }
            } else if (chunk_id == 1) {
                // FAT таблица архива
                std::vector<u8> chunk_data(size);
                if (ReadExact(stream, chunk_data.data(), size) != S_OK) break;
                offset += size;

                std::vector<u8> decomp_fat;
                if (!is_comp) {
                    decomp_fat = std::move(chunk_data);
                } else {
                    DecompressLzHuf(chunk_data.data(), size, decomp_fat);
                }

                if (decomp_fat.empty()) break;

                uint32_t ptr = 0;
                uint32_t decomp_size = (uint32_t)decomp_fat.size();

                while (ptr + 2 <= decomp_size) {
                    uint16_t item_size = *(uint16_t*)(decomp_fat.data() + ptr);
                    ptr += 2;

                    if (item_size < 16 || (ptr - 2 + item_size) > decomp_size) break;

                    uint32_t size_real = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;
                    uint32_t size_compr = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;
                    uint32_t crc = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;

                    int name_length = item_size - 16;
                    if (name_length <= 0 || name_length > 260) break;

                    std::string name((char*)(decomp_fat.data() + ptr), name_length);
                    ptr += name_length;
                    uint32_t file_ptr = *(uint32_t*)(decomp_fat.data() + ptr); ptr += 4;

                    size_t null_pos = name.find('\0');
                    if (null_pos != std::string::npos) name = name.substr(0, null_pos);

                    bool valid = !name.empty();
                    for (unsigned char c : name) {
                        if (c < 32 || c > 126) { valid = false; break; }
                    }

                    if (valid) {
                        ArchiveItem item;
                        item.name = entry_point_prefix + name;
                        item.size_real = size_real;
                        item.size_compr = size_compr;
                        item.offset = file_ptr;
                        item.crc = crc;
                        item.is_dir = (!item.name.empty() && (item.name.back() == '\\' || item.name.back() == '/'));
                        if (item.is_dir) item.name.pop_back();

                        _items.push_back(item);
                    }
                }

                fat_found = true;
                break;
            } else {
                if (FAILED(stream->Seek(size, STREAM_SEEK_CUR, NULL))) break;
                offset += size;
            }
        }

        if (!fat_found || _items.empty()) {
            Close();
            return S_FALSE; // Не наш формат, отдаем 7-Zip'у сигнал пропустить
        }

        _stream = stream;
        _stream->AddRef();
        return S_OK;
    }

    STDMETHOD(Close)() override {
        _items.clear();
        if (_stream) {
            _stream->Release();
            _stream = nullptr;
        }
        return S_OK;
    }

    STDMETHOD(GetNumberOfItems)(UInt32 *numItems) override {
        if (!numItems) return E_POINTER;
        *numItems = (UInt32)_items.size();
        return S_OK;
    }

    STDMETHOD(GetProperty)(UInt32 index, PROPID propID, PROPVARIANT *value) override {
        if (index >= _items.size() || !value) return E_INVALIDARG;
        PropVariantInit(value);
        const ArchiveItem &item = _items[index];

        switch (propID) {
        case 3: // kpidPath
        {
            int wlen = MultiByteToWideChar(CP_ACP, 0, item.name.c_str(), -1, NULL, 0);
            if (wlen > 0) {
                value->vt = VT_BSTR;
                value->bstrVal = SysAllocStringLen(NULL, wlen - 1);
                MultiByteToWideChar(CP_ACP, 0, item.name.c_str(), -1, value->bstrVal, wlen);
            }
            break;
        }
        case 6: // kpidIsDir
            value->vt = VT_BOOL;
            value->boolVal = item.is_dir ? VARIANT_TRUE : VARIANT_FALSE;
            break;
        case 7: // kpidSize
            value->vt = VT_UI8;
            value->uhVal.QuadPart = item.size_real;
            break;
        case 8: // kpidPackSize
            value->vt = VT_UI8;
            value->uhVal.QuadPart = item.size_compr;
            break;
        case 19: // kpidCRC
            value->vt = VT_UI4;
            value->ulVal = item.crc;
            break;
        }
        return S_OK;
    }

    STDMETHOD(Extract)(const UInt32 *indices, UInt32 numItems, Int32 testMode, IArchiveExtractCallback *extractCallback) override {
        if (!_stream) return E_FAIL;
        bool allFiles = (numItems == (UInt32)(Int32)-1);
        UInt32 count = allFiles ? (UInt32)_items.size() : numItems;

        UInt64 totalUnpackSize = 0;
        for (UInt32 i = 0; i < count; i++) {
            UInt32 index = allFiles ? i : indices[i];
            if (index < _items.size() && !_items[index].is_dir)
                totalUnpackSize += _items[index].size_real;
        }
        extractCallback->SetTotal(totalUnpackSize);

        UInt64 currentCompleted = 0;

        for (UInt32 i = 0; i < count; i++) {
            UInt32 index = allFiles ? i : indices[i];
            if (index >= _items.size()) continue;

            const ArchiveItem &item = _items[index];
            extractCallback->SetCompleted(&currentCompleted);

            ISequentialOutStream *outStream = nullptr;
            Int32 askMode = testMode ? 1 : 0;
            HRESULT res = extractCallback->GetStream(index, &outStream, askMode);
            if (FAILED(res)) return res;

            if (item.is_dir) {
                extractCallback->PrepareOperation(askMode);
                if (outStream) outStream->Release();
                extractCallback->SetOperationResult(0); // kOK
                continue;
            }

            if (!outStream && !testMode) {
                extractCallback->PrepareOperation(askMode);
                extractCallback->SetOperationResult(0);
                continue;
            }

            extractCallback->PrepareOperation(askMode);
            Int32 opRes = 0; // kOK

            if (_stream->Seek(item.offset, STREAM_SEEK_SET, NULL) != S_OK) {
                opRes = 2; // kDataError
            } else {
                std::vector<u8> comp_data(item.size_compr);
                if (ReadExact(_stream, comp_data.data(), item.size_compr) != S_OK) {
                    opRes = 2;
                } else if (!testMode && outStream) {
                    if (item.size_real == item.size_compr) {
                        UInt32 processed = 0;
                        outStream->Write(comp_data.data(), item.size_real, &processed);
                    } else {
                        // Пробуем распаковать через LZO
                        std::vector<u8> decomp_data(item.size_real);
                        lzo_uint out_len = item.size_real;
                        int r = lzo1x_decompress_safe(comp_data.data(), (lzo_uint)comp_data.size(), decomp_data.data(), &out_len, NULL);
                        if (r == LZO_E_OK) {
                            UInt32 processed = 0;
                            outStream->Write(decomp_data.data(), out_len, &processed);
                        } else {
                            // Пробуем LzHuf
                            std::vector<u8> dec;
                            if (DecompressLzHuf(comp_data.data(), (u32)comp_data.size(), dec)) {
                                UInt32 processed = 0;
                                outStream->Write(dec.data(), (UInt32)dec.size(), &processed);
                            } else {
                                opRes = 2; // kDataError
                            }
                        }
                    }
                }
            }

            if (outStream) outStream->Release();
            currentCompleted += item.size_real;
            extractCallback->SetOperationResult(opRes);
        }
        return S_OK;
    }

    STDMETHOD(GetArchiveProperty)(PROPID, PROPVARIANT *value) override {
        if (!value) return E_INVALIDARG;
        PropVariantInit(value);
        return S_OK;
    }

    STDMETHOD(GetNumberOfProperties)(UInt32 *numProperties) override {
        if (!numProperties) return E_POINTER;
        *numProperties = 0;
        return S_OK;
    }

    STDMETHOD(GetPropertyInfo)(UInt32, BSTR *, PROPID *, VARTYPE *) override { return E_NOTIMPL; }
    STDMETHOD(GetNumberOfArchiveProperties)(UInt32 *numProperties) override {
        if (!numProperties) return E_POINTER;
        *numProperties = 0;
        return S_OK;
    }
    STDMETHOD(GetArchivePropertyInfo)(UInt32, BSTR *, PROPID *, VARTYPE *) override { return E_NOTIMPL; }
};

// --- ЭКСПОРТ ДЛЯ 7-ZIP ---
BOOL WINAPI DllMain(HINSTANCE, DWORD fdwReason, LPVOID) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        lzo_init();
    }
    return TRUE;
}

extern "C" __declspec(dllexport) HRESULT __stdcall CreateObject(const GUID *clsid, const GUID *iid, void **outObject) {
    if (!outObject) return E_POINTER;
    *outObject = nullptr;
    if (clsid && *clsid != CLSID_CAnomalyDb) return CLASS_E_CLASSNOTAVAILABLE;

    CAnomalyArchive *archive = new CAnomalyArchive();
    HRESULT res = archive->QueryInterface(*iid, outObject);
    if (FAILED(res)) delete archive;
    return res;
}

extern "C" __declspec(dllexport) HRESULT __stdcall GetNumberOfFormats(UINT32 *numFormats) {
    if (!numFormats) return E_POINTER;
    *numFormats = 1;
    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT __stdcall GetHandlerProperty2(UINT32 formatIndex, PROPID propID, PROPVARIANT *value) {
    if (formatIndex != 0 || !value) return E_INVALIDARG;
    PropVariantInit(value);

    switch (propID) {
    case 0: // kName
        value->vt = VT_BSTR;
        value->bstrVal = SysAllocString(L"XRayDb");
        return S_OK;
    case 1: // kClassID
        value->vt = VT_BSTR;
        value->bstrVal = SysAllocStringByteLen((const char*)&CLSID_CAnomalyDb, sizeof(GUID));
        return S_OK;
    case 2: // kExtension
        value->vt = VT_BSTR;
        value->bstrVal = SysAllocString(L"xdb db db0 db1 db2 db3 db4 xdb0 xdb1 xdb2 xdb3 xdb4");
        return S_OK;
    case 3: // kAddExtension
        value->vt = VT_BSTR;
        value->bstrVal = SysAllocString(L"");
        return S_OK;
    case 4: // kUpdate
        value->vt = VT_BOOL;
        value->boolVal = VARIANT_FALSE;
        return S_OK;
    case 5: // kKeepName
        value->vt = VT_BOOL;
        value->boolVal = VARIANT_FALSE;
        return S_OK;
    }
    return S_OK;
}
