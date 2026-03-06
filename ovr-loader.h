#pragma once

#include <windows.h>
#include <stdbool.h>

// OVR LipSync API Tipleri
typedef int ovrLipSyncResult;
#define ovrLipSyncSuccess 0

typedef enum {
    ovrLipSyncViseme_sil,
    ovrLipSyncViseme_PP,
    ovrLipSyncViseme_FF,
    ovrLipSyncViseme_TH,
    ovrLipSyncViseme_DD,
    ovrLipSyncViseme_kk,
    ovrLipSyncViseme_CH,
    ovrLipSyncViseme_SS,
    ovrLipSyncViseme_nn,
    ovrLipSyncViseme_RR,
    ovrLipSyncViseme_aa,
    ovrLipSyncViseme_E,
    ovrLipSyncViseme_ih,
    ovrLipSyncViseme_oh,
    ovrLipSyncViseme_ou,
    ovrLipSyncViseme_Count
} ovrLipSyncViseme;

typedef enum {
    ovrLipSyncLaughterCategory_Count
} ovrLipSyncLaughterCategory;

typedef struct {
  int frameNumber; 
  int frameDelay;
  float* visemes; 
  unsigned int visemesLength;

  float laughterScore;
  float* laughterCategories;
  unsigned int laughterCategoriesLength;
} ovrLipSyncFrame;

typedef unsigned int ovrLipSyncContext;

typedef enum {
    ovrLipSyncAudioDataType_S16_Mono,
    ovrLipSyncAudioDataType_S16_Stereo,
    ovrLipSyncAudioDataType_F32_Mono,
    ovrLipSyncAudioDataType_F32_Stereo
} ovrLipSyncAudioDataType;


// OVR LipSync Fonksiyon İşaretçileri
typedef ovrLipSyncResult (*PFN_ovrLipSync_Initialize)(int sampleRate, int bufferSize);
typedef void (*PFN_ovrLipSync_Shutdown)();
typedef ovrLipSyncResult (*PFN_ovrLipSync_CreateContext)(unsigned int* context, int contextProvider);
typedef ovrLipSyncResult (*PFN_ovrLipSync_CreateContextEx)(unsigned int* context, int contextProvider, int sampleRate, bool enableAcceleration);
typedef ovrLipSyncResult (*PFN_ovrLipSync_DestroyContext)(unsigned int context);
typedef ovrLipSyncResult (*PFN_ovrLipSync_ResetContext)(unsigned int context);
typedef ovrLipSyncResult (*PFN_ovrLipSync_SendAudioBuffer)(unsigned int context, const void* audioBuffer, int audioBufferSamples, ovrLipSyncAudioDataType dataType);
typedef ovrLipSyncResult (*PFN_ovrLipSync_ProcessFrame)(unsigned int context, const void* audioBuffer, int audioBufferSamples, ovrLipSyncAudioDataType dataType, ovrLipSyncFrame* frame);
typedef ovrLipSyncResult (*PFN_ovrLipSync_ProcessFrameEx)(unsigned int context, const void* audioBuffer, int audioBufferSamples, ovrLipSyncAudioDataType dataType, ovrLipSyncFrame* frame);


// OVR Dinamik Yükleyici Sınıfı
class OVRDynamicLoader {
public:
    OVRDynamicLoader();
    ~OVRDynamicLoader();

    bool Load(const char* dllPath);
    void Unload();
    bool IsLoaded() const { return m_hModule != NULL; }

    // API
    PFN_ovrLipSync_Initialize Initialize;
    PFN_ovrLipSync_Shutdown Shutdown;
    PFN_ovrLipSync_CreateContext CreateContext;
    PFN_ovrLipSync_CreateContextEx CreateContextEx;
    PFN_ovrLipSync_DestroyContext DestroyContext;
    PFN_ovrLipSync_ResetContext ResetContext;
    PFN_ovrLipSync_SendAudioBuffer SendAudioBuffer;
    PFN_ovrLipSync_ProcessFrame ProcessFrame;
    PFN_ovrLipSync_ProcessFrameEx ProcessFrameEx;

private:
    HMODULE m_hModule;
};
