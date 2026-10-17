#include <jni.h>
#include <vulkan/vulkan.h>
#include <sstream>
#include <vector>

extern "C" JNIEXPORT jstring JNICALL
Java_org_example_botwlab_MainActivity_nativeVulkanInfo(JNIEnv* env, jclass) {
    std::ostringstream text;
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "Android GPU hardware test";
    app.applicationVersion = 1;
    app.pEngineName = "None";
    app.engineVersion = 1;
    app.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create.pApplicationInfo = &app;
    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&create, nullptr, &instance);
    if (result != VK_SUCCESS) {
        text << "Vulkan nativo: vkCreateInstance falló (" << static_cast<int>(result) << ")";
        return env->NewStringUTF(text.str().c_str());
    }

    uint32_t count = 0;
    result = vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (result != VK_SUCCESS || count == 0) {
        text << "Vulkan nativo: sin GPUs (" << static_cast<int>(result) << ")";
        vkDestroyInstance(instance, nullptr);
        return env->NewStringUTF(text.str().c_str());
    }

    std::vector<VkPhysicalDevice> devices(count);
    result = vkEnumeratePhysicalDevices(instance, &count, devices.data());
    if (result == VK_SUCCESS) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(devices[0], &props);
        text << "Vulkan nativo: "
             << VK_API_VERSION_MAJOR(props.apiVersion) << "."
             << VK_API_VERSION_MINOR(props.apiVersion) << "."
             << VK_API_VERSION_PATCH(props.apiVersion) << "\nGPU Vulkan: "
             << props.deviceName << "\n"
             << "Proveedor: 0x" << std::hex << props.vendorID << std::dec
             << " | dispositivos: " << count;
    } else {
        text << "Vulkan nativo: vkEnumeratePhysicalDevices falló ("
             << static_cast<int>(result) << ")";
    }
    vkDestroyInstance(instance, nullptr);
    return env->NewStringUTF(text.str().c_str());
}
