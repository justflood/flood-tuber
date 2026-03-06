#include "ovr-loader.h"
#include <obs-module.h>
#include <windows.h>

OVRDynamicLoader::OVRDynamicLoader() : 
    m_hModule(NULL),
    Initialize(NULL),
    Shutdown(NULL),
    CreateContext(NULL),
    CreateContextEx(NULL),
    DestroyContext(NULL),
    ResetContext(NULL),
    SendAudioBuffer(NULL),
    ProcessFrame(NULL),
    ProcessFrameEx(NULL) 
{
}

OVRDynamicLoader::~OVRDynamicLoader() {
    Unload();
}

bool OVRDynamicLoader::Load(const char* dllPath) {
    if (m_hModule) return true; // Already loaded

    m_hModule = LoadLibraryA(dllPath);
    if (!m_hModule) {
        DWORD err = GetLastError();
        blog(LOG_WARNING, "[Flood-Tuber] OVRDynamicLoader: LoadLibraryA failed for '%s'. Error Code: %lu", dllPath, err);
        return false;
    }

    // Load functions
    Initialize = (PFN_ovrLipSync_Initialize)GetProcAddress(m_hModule, "ovrLipSyncDll_Initialize");
    Shutdown = (PFN_ovrLipSync_Shutdown)GetProcAddress(m_hModule, "ovrLipSyncDll_Shutdown");
    CreateContext = (PFN_ovrLipSync_CreateContext)GetProcAddress(m_hModule, "ovrLipSyncDll_CreateContext");
    CreateContextEx = (PFN_ovrLipSync_CreateContextEx)GetProcAddress(m_hModule, "ovrLipSyncDll_CreateContextEx");
    DestroyContext = (PFN_ovrLipSync_DestroyContext)GetProcAddress(m_hModule, "ovrLipSyncDll_DestroyContext");
    ResetContext = (PFN_ovrLipSync_ResetContext)GetProcAddress(m_hModule, "ovrLipSyncDll_ResetContext");
    SendAudioBuffer = (PFN_ovrLipSync_SendAudioBuffer)GetProcAddress(m_hModule, "ovrLipSyncDll_SendAudioBuffer");
    ProcessFrame = (PFN_ovrLipSync_ProcessFrame)GetProcAddress(m_hModule, "ovrLipSyncDll_ProcessFrame");
    ProcessFrameEx = (PFN_ovrLipSync_ProcessFrameEx)GetProcAddress(m_hModule, "ovrLipSyncDll_ProcessFrameEx");

    // Check if critical functions are present
    if (!Initialize || !Shutdown || !CreateContext || !DestroyContext || !ProcessFrameEx) {
        blog(LOG_WARNING, "[Flood-Tuber] OVRDynamicLoader: One or more critical functions are missing in '%s'.", dllPath);
        blog(LOG_WARNING, "[Flood-Tuber] Init: %p, Shutdown: %p, Create: %p, Destroy: %p, ProcessEx: %p", 
             (void*)Initialize, (void*)Shutdown, (void*)CreateContext, (void*)DestroyContext, (void*)ProcessFrameEx);
        Unload();
        return false;
    }

    return true;
}

void OVRDynamicLoader::Unload() {
    if (m_hModule) {
        FreeLibrary(m_hModule);
        m_hModule = NULL;
    }
    
    Initialize = NULL;
    Shutdown = NULL;
    CreateContext = NULL;
    CreateContextEx = NULL;
    DestroyContext = NULL;
    ResetContext = NULL;
    SendAudioBuffer = NULL;
    ProcessFrame = NULL;
    ProcessFrameEx = NULL;
}
