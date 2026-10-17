#include "vulkan_runtime_probe.h"
#include <jni.h>
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace {
bool hasExtension(const std::vector<VkExtensionProperties>& exts,const char* wanted){
  for(const auto& ext:exts) if(std::strcmp(ext.extensionName,wanted)==0)return true;
  return false;
}
bool sampled(VkPhysicalDevice gpu,VkFormat format){
  VkFormatProperties p{};
  vkGetPhysicalDeviceFormatProperties(gpu,format,&p);
  return (p.optimalTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)!=0;
}
std::string version(std::uint32_t v){
  std::ostringstream s;
  s<<VK_API_VERSION_MAJOR(v)<<"."<<VK_API_VERSION_MINOR(v)<<"."<<VK_API_VERSION_PATCH(v);
  return s.str();
}
std::string yes(bool value){return value?"si":"no";}
}
extern "C" JNIEXPORT jstring JNICALL
Java_org_example_botwlab_MainActivity_nativeVulkanRuntimeProbe(JNIEnv* env,jclass){
  std::ostringstream out;
  out<<"DIAGNOSTICO VULKAN PARA RECOMPILACION SWITCH\n";
  // Use only Vulkan queries and a temporary logical device. No shader/game code.
  auto enumVer=reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
      vkGetInstanceProcAddr(VK_NULL_HANDLE,"vkEnumerateInstanceVersion"));
  std::uint32_t instanceSupported=VK_API_VERSION_1_0;
  if(enumVer)enumVer(&instanceSupported);
  const std::uint32_t instanceRequested=
      instanceSupported>=VK_API_VERSION_1_3?VK_API_VERSION_1_3:
      instanceSupported>=VK_API_VERSION_1_1?VK_API_VERSION_1_1:VK_API_VERSION_1_0;
  VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  application.pApplicationName="Switch GPU compatibility audit";
  application.apiVersion=instanceRequested;
  VkInstanceCreateInfo makeInstance{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  makeInstance.pApplicationInfo=&application;
  VkInstance instance=VK_NULL_HANDLE;
  const VkResult createResult=vkCreateInstance(&makeInstance,nullptr,&instance);
  if(createResult!=VK_SUCCESS){
    out<<"No se pudo iniciar Vulkan: "<<static_cast<int>(createResult);
    return env->NewStringUTF(out.str().c_str());
  }
  std::uint32_t count=0;
  VkResult enumerated=vkEnumeratePhysicalDevices(instance,&count,nullptr);
  if(enumerated!=VK_SUCCESS || !count || count>16){
    out<<"No hay GPUs Vulkan disponibles (resultado "<<static_cast<int>(enumerated)<<").";
    vkDestroyInstance(instance,nullptr);
    return env->NewStringUTF(out.str().c_str());
  }
  std::vector<VkPhysicalDevice> gpus(count);
  enumerated=vkEnumeratePhysicalDevices(instance,&count,gpus.data());
  if(enumerated!=VK_SUCCESS){
    out<<"No se pudo enumerar Vulkan: "<<static_cast<int>(enumerated);
    vkDestroyInstance(instance,nullptr);
    return env->NewStringUTF(out.str().c_str());
  }
  VkPhysicalDevice gpu=gpus[0];
  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(gpu,&props);
  switchgpu::Capabilities caps;
  caps.major=VK_API_VERSION_MAJOR(props.apiVersion);
  caps.minor=VK_API_VERSION_MINOR(props.apiVersion);
  out<<"GPU: "<<props.deviceName<<"\nVulkan GPU: "<<version(props.apiVersion)
     <<"\nVulkan loader: "<<version(instanceSupported)<<"\n";
  std::uint32_t queues=0;
  vkGetPhysicalDeviceQueueFamilyProperties(gpu,&queues,nullptr);
  std::vector<VkQueueFamilyProperties> qf(queues);
  if(queues)vkGetPhysicalDeviceQueueFamilyProperties(gpu,&queues,qf.data());
  std::uint32_t graphicsIndex=queues;
  for(std::uint32_t i=0;i<queues;i++){
    if(qf[i].queueCount && (qf[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)){
      graphicsIndex=i;break;
    }
  }
  caps.graphicsQueue=graphicsIndex<queues;
  VkPhysicalDeviceFeatures legacy{};
  vkGetPhysicalDeviceFeatures(gpu,&legacy);
  VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
  const bool featureQuery=instanceRequested>=VK_API_VERSION_1_1;
  auto getFeatures=reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
      vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceFeatures2"));
  if(featureQuery && getFeatures){
    if(caps.major>1 || (caps.major==1 && caps.minor>=3))
      features.pNext=&features13;
    getFeatures(gpu,&features);
  }
  caps.dynamicRendering=featureQuery && getFeatures && features13.dynamicRendering==VK_TRUE
                        && switchgpu::versionAtLeast(caps,1,3);
  caps.astc=legacy.textureCompressionASTC_LDR && sampled(gpu,VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
  caps.bc=legacy.textureCompressionBC && sampled(gpu,VK_FORMAT_BC1_RGBA_UNORM_BLOCK);
  std::uint32_t numExts=0;
  std::vector<VkExtensionProperties> exts;
  if(vkEnumerateDeviceExtensionProperties(gpu,nullptr,&numExts,nullptr)==VK_SUCCESS && numExts<=2048){
    exts.resize(numExts);
    if(vkEnumerateDeviceExtensionProperties(gpu,nullptr,&numExts,exts.data())!=VK_SUCCESS)
      exts.clear();
  }
  caps.indexUint8=hasExtension(exts,"VK_EXT_index_type_uint8");
  caps.depthClipControl=hasExtension(exts,"VK_EXT_depth_clip_control");
  VkPhysicalDeviceSubgroupSizeControlProperties subgroup{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES};
  VkPhysicalDeviceProperties2 properties2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
  properties2.pNext=&subgroup;
  auto getProps=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
      vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceProperties2"));
  if(getProps && featureQuery && switchgpu::versionAtLeast(caps,1,3)){
    getProps(gpu,&properties2);
    caps.subgroup32=features13.subgroupSizeControl &&
        subgroup.minSubgroupSize<=32 && subgroup.maxSubgroupSize>=32;
    out<<"Subgroup min/max: "<<subgroup.minSubgroupSize<<"/"<<subgroup.maxSubgroupSize<<"\n";
  }else out<<"Subgroup min/max: no consultable\n";
  out<<"Vulkan 1.3 o superior: "<<yes(switchgpu::versionAtLeast(caps,1,3))<<"\n"
     <<"Cola grafica: "<<yes(caps.graphicsQueue)<<"\n"
     <<"dynamicRendering: "<<yes(caps.dynamicRendering)<<"\n"
     <<"ASTC nativo: "<<yes(caps.astc)<<"\n"
     <<"BC1 nativo: "<<yes(caps.bc)<<"\n"
     <<"Indices uint8 (extension): "<<yes(caps.indexUint8)<<"\n"
     <<"Depth clip control (extension): "<<yes(caps.depthClipControl)<<"\n"
     <<"Subgroup de 32 seleccionable: "<<yes(caps.subgroup32)<<"\n";
  VkFormatProperties depth{};
  vkGetPhysicalDeviceFormatProperties(gpu,VK_FORMAT_D24_UNORM_S8_UINT,&depth);
  out<<"D24/S8 depth attachment: "
     <<yes((depth.optimalTilingFeatures&VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)!=0)<<"\n";
  VkPhysicalDeviceMemoryProperties mem{};
  vkGetPhysicalDeviceMemoryProperties(gpu,&mem);
  std::uint64_t localBytes=0;
  for(std::uint32_t i=0;i<mem.memoryHeapCount;i++){
    if(mem.memoryHeaps[i].flags&VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
      localBytes+=mem.memoryHeaps[i].size;
  }
  out<<"Heaps local(es) reportados: "<<(localBytes/(1024*1024))<<" MiB"
     <<" (no equivalen necesariamente a RAM libre)\n";
  // Important: vkCreateDevice is attempted only with a physical GPU advertising
  // a 1.3 device and 1.3 loader; do not request unsupported features.
  if(caps.graphicsQueue && caps.dynamicRendering && instanceRequested>=VK_API_VERSION_1_3){
    const float priority=1.0f;
    VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    q.queueFamilyIndex=graphicsIndex;q.queueCount=1;q.pQueuePriorities=&priority;
    VkPhysicalDeviceVulkan13Features enabled{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    enabled.dynamicRendering=VK_TRUE;
    VkDeviceCreateInfo create{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    create.queueCreateInfoCount=1;create.pQueueCreateInfos=&q;
    create.pNext=&enabled;
    VkDevice device=VK_NULL_HANDLE;
    VkResult status=vkCreateDevice(gpu,&create,nullptr,&device);
    caps.createdDevice=(status==VK_SUCCESS && device!=VK_NULL_HANDLE);
    out<<"Crear dispositivo Vulkan con dynamicRendering: "
       <<(caps.createdDevice?"CORRECTO":"FALLO ("+std::to_string(static_cast<int>(status))+")")<<"\n";
    if(device)vkDestroyDevice(device,nullptr);
  }else{
    out<<"Crear dispositivo Vulkan con dynamicRendering: NO intentado (faltan requisitos)\n";
  }
  out<<"\n"<<switchgpu::conclusion(caps)
     <<"\n\nASTC, BC e indices uint8 pueden tener rutas alternativas de conversion "
       "en proyectos de referencia. Esta prueba NO carga ni ejecuta BOTW, "
       "NO verifica NVN, shaders completos, memoria disponible o FPS.";
  vkDestroyInstance(instance,nullptr);
  return env->NewStringUTF(out.str().c_str());
}
