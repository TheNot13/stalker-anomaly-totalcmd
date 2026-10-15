#pragma once
#include <windows.h>
#include <ole2.h>
#include <oaidl.h>
#include <propidl.h>

typedef UINT32 UInt32;
typedef INT32 Int32;
typedef UINT64 UInt64;
typedef INT64 Int64;

struct ISequentialInStream : public IUnknown
{
    virtual HRESULT __stdcall Read(void *data, UInt32 size, UInt32 *processedSize) = 0;
};

struct ISequentialOutStream : public IUnknown
{
    virtual HRESULT __stdcall Write(const void *data, UInt32 size, UInt32 *processedSize) = 0;
};

struct IInStream : public ISequentialInStream
{
    virtual HRESULT __stdcall Seek(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition) = 0;
};

struct IArchiveOpenCallback : public IUnknown
{
    virtual HRESULT __stdcall SetTotal(const UInt64 *files, const UInt64 *bytes) = 0;
    virtual HRESULT __stdcall SetCompleted(const UInt64 *files, const UInt64 *bytes) = 0;
};

struct IProgress : public IUnknown
{
    virtual HRESULT __stdcall SetTotal(UInt64 total) = 0;
    virtual HRESULT __stdcall SetCompleted(const UInt64 *completeValue) = 0;
};

struct IArchiveExtractCallback : public IProgress
{
    virtual HRESULT __stdcall GetStream(UInt32 index, ISequentialOutStream **outStream, Int32 askExtractMode) = 0;
    virtual HRESULT __stdcall PrepareOperation(Int32 askExtractMode) = 0;
    virtual HRESULT __stdcall SetOperationResult(Int32 resultEOperationResult) = 0;
};

struct IInArchive : public IUnknown
{
    virtual HRESULT __stdcall Open(IInStream *stream, const UInt64 *maxCheckStartPosition, IArchiveOpenCallback *openArchiveCallback) = 0;
    virtual HRESULT __stdcall Close() = 0;
    virtual HRESULT __stdcall GetNumberOfItems(UInt32 *numItems) = 0;
    virtual HRESULT __stdcall GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value) = 0;
    virtual HRESULT __stdcall Extract(const UInt32 *indices, UInt32 numItems, Int32 testMode, IArchiveExtractCallback *extractCallback) = 0;
    virtual HRESULT __stdcall GetArchiveProperty(PROPID propID, PROPVARIANT *value) = 0;
    virtual HRESULT __stdcall GetNumberOfProperties(UInt32 *numProperties) = 0;
    virtual HRESULT __stdcall GetPropertyInfo(UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType) = 0;
    virtual HRESULT __stdcall GetNumberOfArchiveProperties(UInt32 *numProperties) = 0;
    virtual HRESULT __stdcall GetArchivePropertyInfo(UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType) = 0;
};

// GUID интерфейса IInArchive в 7-Zip
static const GUID IID_IInArchive = 
{ 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x06, 0x00, 0x60, 0x00, 0x00 } };

// Уникальный CLSID для нашего обработчика X-Ray DB
// {6B5F7F9D-1C98-4F18-A795-3E71E7A339B1}
static const GUID CLSID_CAnomalyDb = 
{ 0x6b5f7f9d, 0x1c98, 0x4f18, { 0xa7, 0x95, 0x3e, 0x71, 0xe7, 0xa3, 0x39, 0xb1 } };
