#ifndef CKAN_EXPORT_H
#define CKAN_EXPORT_H

// libckan 静态库导出宏。
// 静态库无需 dllimport/export 修饰，但保留宏以便将来可切换为动态库。
#if defined(_WIN32) && defined(CKAN_BUILD_SHARED)
#  if defined(CKAN_BUILDING_LIB)
#    define CKAN_API __declspec(dllexport)
#  else
#    define CKAN_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && defined(CKAN_BUILDING_LIB)
#  define CKAN_API __attribute__((visibility("default")))
#else
#  define CKAN_API
#endif

#endif // CKAN_EXPORT_H