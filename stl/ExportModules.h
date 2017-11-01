#pragma once

#ifndef BUILDING_COREMODULE
#define BUILDING_COREMODULE 1
#endif

#if UNITY_WIN && UNITY_STANDALONE && !UNITY_WINRT && 0

#if BUILDING_COREMODULE
#define EXPORT_COREMODULE __declspec(dllexport)
#else
#define EXPORT_COREMODULE __declspec(dllimport)
#endif

#define EXPORT_MODULE __declspec(dllexport)

#elif UNITY_OSX && UNITY_STANDALONE && 0

#if BUILDING_COREMODULE
#define EXPORT_COREMODULE __attribute__((visibility("default")))
#else
#define EXPORT_COREMODULE
#endif

#define EXPORT_MODULE __attribute__((visibility("default")))

#else

#define EXPORT_COREMODULE 
#define EXPORT_MODULE

#endif
