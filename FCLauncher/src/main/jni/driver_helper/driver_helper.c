//
// Created by Vera-Firefly on 17.01.2025.
//

#include <EGL/egl.h>
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
#include "glfw/include/gl.h"
#include <vulkan/vulkan.h>

void testVulkan(void *libVulkan);

void testVulkan(void *libVulkan) {
    // 获取 Vulkan 函数指针
    PFN_vkCreateInstance vkCreateInstance =
        (PFN_vkCreateInstance)dlsym(libVulkan, "vkCreateInstance");
    PFN_vkDestroyInstance vkDestroyInstance =
        (PFN_vkDestroyInstance)dlsym(libVulkan, "vkDestroyInstance");
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices =
        (PFN_vkEnumeratePhysicalDevices)dlsym(libVulkan, "vkEnumeratePhysicalDevices");
    PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties =
        (PFN_vkGetPhysicalDeviceProperties)dlsym(libVulkan, "vkGetPhysicalDeviceProperties");

    // 应用程序信息
    VkApplicationInfo appInfo = {0};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "AdrenoToolsExample";
    appInfo.applicationVersion = 1;
    appInfo.pEngineName = "AdrenoToolsExample";
    appInfo.engineVersion = 1;
    appInfo.apiVersion = VK_API_VERSION_1_0;

    // 实例创建信息
    VkInstanceCreateInfo instanceCreateInfo = {0};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;

    VkInstance instance;
    VkResult result = vkCreateInstance(&instanceCreateInfo, NULL, &instance);
    if (result != VK_SUCCESS) {
        printf("vkCreateInstance failed: %d", result);
        return;
    }

    // 查询物理设备数量
    uint32_t numDevices = 1u;
    vkEnumeratePhysicalDevices(instance, &numDevices, NULL);
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

void* loadTurnipVulkan() {
    if (!checkAdrenoGraphics())
        return NULL;

    const char* native_dir = getenv("DRIVER_PATH");
    const char* cache_dir = getenv("TMPDIR");

    if (!native_dir) 
        return NULL;

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
