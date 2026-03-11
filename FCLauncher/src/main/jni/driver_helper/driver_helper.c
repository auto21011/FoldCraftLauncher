//
// Created by Vera-Firefly on 17.01.2025.
//

#ifndef TERMUX_TEST
#include <EGL/egl.h>
#endif
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <android/dlext.h>
#include "driver_helper.h"
#include "nsbypass.h"
#ifndef TERMUX_TEST
#include "glfw/include/gl.h"
#endif
#include <vulkan/vulkan.h>
#include <dirent.h>
#include <sys/stat.h>

void testVulkan(void *libVulkan);


// 假设 log_to_file 已在别处定义，其原型如下：
// void log_to_file(int priority, const char* tag, const char* format, ...);
// 此处保留 Android 日志调用，若无需可替换为 fprintf(stderr, ...)

void copyFile(const char *srcFolder, const char *dstFolder, const char *filename)
{
    char srcPath[512];
    char dstPath[512];

    // 拼接完整路径（注意缓冲区大小，可根据实际调整）
    snprintf(srcPath, sizeof(srcPath), "%s%s", srcFolder, filename);
    snprintf(dstPath, sizeof(dstPath), "%s%s", dstFolder, filename);

    // 打开源文件（二进制读模式）
    FILE *inputFile = fopen(srcPath, "rb");
    if (!inputFile)
    {
        printf("Could not open file %s!\n", srcPath);
        return;
    }

    // 获取文件大小
    fseek(inputFile, 0, SEEK_END);
    long sizeBytes = ftell(inputFile);
    fseek(inputFile, 0, SEEK_SET);

    if (sizeBytes < 0)
    {
        printf( "Failed to get size of file %s!\n", srcPath);
        fclose(inputFile);
        return;
    }

    // 分配内存缓冲区
    char *fileData = (char*)malloc(sizeBytes);
    if (!fileData)
    {
        printf( "Memory allocation failed for file %s!\n", srcPath);
        fclose(inputFile);
        return;
    }

    // 读取文件内容
    size_t readBytes = fread(fileData, 1, sizeBytes, inputFile);
    fclose(inputFile);  // 关闭源文件

    if (readBytes != (size_t)sizeBytes)
    {
        printf( "Failed to read file %s!\n", srcPath);
        free(fileData);
        return;
    }

    // 打开目标文件（二进制写模式，自动覆盖）
    FILE *outputFile = fopen(dstPath, "wb");
    if (!outputFile)
    {
        printf( "Could not write to file %s!\n", dstPath);
        free(fileData);
        return;
    }

    // 写入数据
    size_t writtenBytes = fwrite(fileData, 1, sizeBytes, outputFile);
    fclose(outputFile);
    free(fileData);

    if (writtenBytes != (size_t)sizeBytes)
    {
        printf( "Failed to write file %s!\n", dstPath);
    }
}


// 假设 copyFile 已定义，其原型为：
// void copyFile(const char *srcFolder, const char *dstFolder, const char *filename);

/**
 * 将源目录中的所有文件复制到目标目录（目标目录必须存在）。
 * @param srcDir  源目录路径，例如 "/sdcard/src"
 * @param dstDir  目标目录路径，例如 "/sdcard/dst"
 */
void copyAllFiles(const char *srcDir, const char *dstDir)
{
    if (!srcDir || !dstDir || srcDir[0] == '\0' || dstDir[0] == '\0') {
        printf( "copyAllFiles: Invalid directory path\n");
        return;
    }

    // 构建以斜杠结尾的源目录路径，以便直接与文件名拼接
    char srcFolder[512];
    char dstFolder[512];
    size_t srcLen = strlen(srcDir);
    size_t dstLen = strlen(dstDir);

    // 检查缓冲区是否足够（加上可能的斜杠和终止符）
    if (srcLen + 2 > sizeof(srcFolder) || dstLen + 2 > sizeof(dstFolder)) {
        printf( "copyAllFiles: Directory path too long\n");
        return;
    }

    // 复制源目录，并确保以 '/' 结尾
    strcpy(srcFolder, srcDir);
    if (srcLen == 0 || srcFolder[srcLen - 1] != '/') {
        strcat(srcFolder, "/");
    }

    // 复制目标目录，并确保以 '/' 结尾
    strcpy(dstFolder, dstDir);
    if (dstLen == 0 || dstFolder[dstLen - 1] != '/') {
        strcat(dstFolder, "/");
    }

    // 打开源目录
    DIR *dir = opendir(srcDir);
    if (!dir) {
        printf( "copyAllFiles: Failed to open source directory %s\n", srcDir);
        return;
    }

    struct dirent *entry;
    struct stat st;
    char fullSrcPath[1024];  // 用于 stat 的完整路径

    while ((entry = readdir(dir)) != NULL) {
        // 跳过当前目录和上级目录
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 构造完整的源文件路径，用于 stat 判断
        snprintf(fullSrcPath, sizeof(fullSrcPath), "%s%s", srcFolder, entry->d_name);
        if (stat(fullSrcPath, &st) != 0) {
            // 无法获取文件信息，可能是权限问题，继续下一个
            continue;
        }

        // 如果是普通文件，则复制
        //if (S_ISREG(st.st_mode)) {
            copyFile(srcFolder, dstFolder, entry->d_name);
        //}
        // 忽略其他类型（目录、链接等）
    }

    closedir(dir);
}

void testVulkan(void *libVulkan) {
    printf("testVulkan: %d\n", __LINE__);
    // 获取 Vulkan 函数指针
    PFN_vkCreateInstance vkCreateInstance =
        (PFN_vkCreateInstance)dlsym(libVulkan, "vkCreateInstance");
    PFN_vkDestroyInstance vkDestroyInstance =
        (PFN_vkDestroyInstance)dlsym(libVulkan, "vkDestroyInstance");
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices =
        (PFN_vkEnumeratePhysicalDevices)dlsym(libVulkan, "vkEnumeratePhysicalDevices");
    PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties =
        (PFN_vkGetPhysicalDeviceProperties)dlsym(libVulkan, "vkGetPhysicalDeviceProperties");

    printf("libVulkan:                     %p\n", libVulkan);
    printf("vkCreateInstance:              %p\n", vkCreateInstance);
    printf("vkDestroyInstance:             %p\n", vkDestroyInstance);
    printf("vkEnumeratePhysicalDevices:    %p\n", vkEnumeratePhysicalDevices);
    printf("vkGetPhysicalDeviceProperties: %p\n", vkGetPhysicalDeviceProperties);
    printf("testVulkan: %d\n", __LINE__);
    // 应用程序信息
    VkApplicationInfo appInfo = {0};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "AdrenoToolsExample";
    appInfo.applicationVersion = 1;
    appInfo.pEngineName = "AdrenoToolsExample";
    appInfo.engineVersion = 1;
    appInfo.apiVersion = VK_API_VERSION_1_0;

    // 实例创建信息
    printf("testVulkan: %d\n", __LINE__);
    VkInstanceCreateInfo instanceCreateInfo = {0};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;

    printf("testVulkan: %d\n", __LINE__);
    VkInstance instance;
    VkResult result = vkCreateInstance(&instanceCreateInfo, NULL, &instance);
    printf("testVulkan: %d\n", __LINE__);
    if (result != VK_SUCCESS) {
        printf("vkCreateInstance failed: %d", result);
        return;
    }

    // 查询物理设备数量
    uint32_t numDevices = 1u;
    vkEnumeratePhysicalDevices(instance, &numDevices, NULL);
    printf("testVulkan: %d\n", __LINE__);
    if (numDevices == 0) {
        printf( "NO VK DEVICES!\n");
    } else {
        printf( "YES VK DEVICES!\n");

        // 分配数组存储物理设备句柄
        VkPhysicalDevice *pd = (VkPhysicalDevice*)malloc(numDevices * sizeof(VkPhysicalDevice));
        if (!pd) {
            printf( "malloc failed");
            vkDestroyInstance(instance, NULL);
            return;
        }

        vkEnumeratePhysicalDevices(instance, &numDevices, pd);

        for (uint32_t i = 0u; i < numDevices; ++i) {
            VkPhysicalDeviceProperties deviceProps;
            vkGetPhysicalDeviceProperties(pd[i], &deviceProps);

            // 解析驱动版本（与 SaschaWillems 的 VulkanCapsViewer 一致）
            const uint32_t driverVersionMajor = (deviceProps.driverVersion >> 22u) & 0x3ff;
            const uint32_t driverVersionMinor = (deviceProps.driverVersion >> 12u) & 0x3ff;
            const uint32_t driverVersionRelease = (deviceProps.driverVersion) & 0xfff;

            printf( "Device %u: %s.\n"
                                "Vulkan API %u.%u.%u\n"
                                "Driver Version: %u.%u.%u (%u)\n",
                                i, deviceProps.deviceName,
                                VK_API_VERSION_MAJOR(deviceProps.apiVersion),
                                VK_API_VERSION_MINOR(deviceProps.apiVersion),
                                VK_API_VERSION_PATCH(deviceProps.apiVersion),
                                driverVersionMajor, driverVersionMinor, driverVersionRelease,
                                deviceProps.driverVersion);

        }

        free(pd);
    }

    vkDestroyInstance(instance, NULL);
}


//#define ADRENO_POSSIBLE
#ifdef ADRENO_POSSIBLE

#ifndef TERMUX_TEST
bool checkAdrenoGraphics() {
    EGLDisplay eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisplay == EGL_NO_DISPLAY || eglInitialize(eglDisplay, NULL, NULL) != EGL_TRUE) 
        return false;

    EGLint egl_attributes[] = {
        EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8,
        EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE
    };

    EGLint num_configs = 0;
    if (eglChooseConfig(eglDisplay, egl_attributes, NULL, 0, &num_configs) != EGL_TRUE || num_configs == 0) {
        eglTerminate(eglDisplay);
        return false;
    }

    EGLConfig eglConfig;
    eglChooseConfig(eglDisplay, egl_attributes, &eglConfig, 1, &num_configs);

    const EGLint egl_context_attributes[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    EGLContext context = eglCreateContext(eglDisplay, eglConfig, EGL_NO_CONTEXT, egl_context_attributes);
    if (context == EGL_NO_CONTEXT) {
        eglTerminate(eglDisplay);
        return false;
    }

    if (eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, context) != EGL_TRUE) {
        eglDestroyContext(eglDisplay, context);
        eglTerminate(eglDisplay);
        return false;
    }

    const char* vendor = (const char*)glGetString(GL_VENDOR);
    const char* renderer = (const char*)glGetString(GL_RENDERER);

    bool is_adreno = (vendor && renderer && strcmp(vendor, "Qualcomm") == 0 && strstr(renderer, "Adreno") != NULL);

    eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(eglDisplay, context);
    eglTerminate(eglDisplay);

    return is_adreno;
}
#endif

void* loadTurnipVulkan() {
#ifndef TERMUX_TEST
    if (!checkAdrenoGraphics())
        return NULL;
#endif

    const char* native_dir = getenv("DRIVER_PATH");
    const char* cache_dir = getenv("TMPDIR");
    const char* ext_lib_dir = getenv("EXT_LIB_PATH");
    const char* dynamic_lib_dir = getenv("DYNAMIC_NATIVE_LIB_PATH");

    if (!native_dir) 
        return NULL;

    if (dynamic_lib_dir && ext_lib_dir)
    {
        copyAllFiles(ext_lib_dir, dynamic_lib_dir);
        char full_path[strlen(native_dir) + strlen(dynamic_lib_dir) + 2];
        snprintf(full_path, sizeof(full_path), "%s:%s", native_dir, dynamic_lib_dir);
        
        if (!linker_ns_load(full_path))
            return NULL;
    }
    else
    if (!linker_ns_load(native_dir))
        return NULL;

    void* linkerhook = linker_ns_dlopen("liblinkerhook.so", RTLD_LOCAL | RTLD_NOW);
    if (!linkerhook)
        return NULL;

    void* turnip_driver_handle = linker_ns_dlopen("libvulkan_freedreno.so", RTLD_LOCAL | RTLD_NOW);
    if (!turnip_driver_handle) {
        printf("load libvulkan_freedreno.so failed:\n%s\n", dlerror());
        dlclose(linkerhook);
        return NULL;
    }

    void* dl_android = linker_ns_dlopen("libdl_android.so", RTLD_LOCAL | RTLD_LAZY);
    if (!dl_android) {
        dlclose(linkerhook);
        dlclose(turnip_driver_handle);
        return NULL;
    }

    void* android_get_exported_namespace = dlsym(dl_android, "android_get_exported_namespace");
    void (*linkerhookPassHandles)(void*, void*, void*) = dlsym(linkerhook, "linker_hook_set_handles");

    if (!linkerhookPassHandles || !android_get_exported_namespace) {
        dlclose(dl_android);
        dlclose(linkerhook);
        dlclose(turnip_driver_handle);
        return NULL;
    }

    linkerhookPassHandles(turnip_driver_handle, android_dlopen_ext, android_get_exported_namespace);

    void* libvulkan = linker_ns_dlopen_unique(cache_dir, "libvulkan.so", RTLD_LOCAL | RTLD_NOW);
    if (!libvulkan) {
        printf("load libvulkan.so failed:\n%s\n", dlerror());
        dlclose(dl_android);
        dlclose(linkerhook);
        dlclose(turnip_driver_handle);
        return NULL;
    }

    return libvulkan;
}

#endif

static void set_vulkan_ptr(void* ptr) {
    char envval[64];
    sprintf(envval, "%"PRIxPTR, (uintptr_t)ptr);
    setenv("VULKAN_PTR", envval, 1);
    testVulkan(ptr);
}

void load_vulkan() {
    if(getenv("VULKAN_DRIVER_SYSTEM") == NULL && android_get_device_api_level() >= 28) {
#ifdef ADRENO_POSSIBLE
        void* result = loadTurnipVulkan();
        if(result != NULL) {
            printf("AdrenoSupp: Loaded Turnip, loader address: %p\n", result);
            set_vulkan_ptr(result);
            return;
        }
#endif
    }
    printf("OSMDroid: loading vulkan regularly...\n");
    void* vulkan_ptr = dlopen("libvulkan.so", RTLD_LAZY | RTLD_LOCAL);
    printf("OSMDroid: loaded vulkan, ptr=%p\n", vulkan_ptr);
    set_vulkan_ptr(vulkan_ptr);
}
