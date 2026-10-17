#include "vulkan_offscreen_test.h"
#include <jni.h>
#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace {
struct Resources {
    VkInstance instance=VK_NULL_HANDLE;
    VkDevice device=VK_NULL_HANDLE;
    VkCommandPool pool=VK_NULL_HANDLE;
    VkImage image=VK_NULL_HANDLE;
    VkDeviceMemory imageMemory=VK_NULL_HANDLE;
    VkBuffer buffer=VK_NULL_HANDLE;
    VkDeviceMemory bufferMemory=VK_NULL_HANDLE;
    VkFence fence=VK_NULL_HANDLE;
    ~Resources(){
        // This function is intended for the normal completed-submit path.
        if(device){
            if(fence)vkDestroyFence(device,fence,nullptr);
            if(pool)vkDestroyCommandPool(device,pool,nullptr);
            if(buffer)vkDestroyBuffer(device,buffer,nullptr);
            if(bufferMemory)vkFreeMemory(device,bufferMemory,nullptr);
            if(image)vkDestroyImage(device,image,nullptr);
            if(imageMemory)vkFreeMemory(device,imageMemory,nullptr);
            vkDestroyDevice(device,nullptr);
        }
        if(instance)vkDestroyInstance(instance,nullptr);
    }
};
bool version13(std::uint32_t version){
    return VK_API_VERSION_MAJOR(version)>1 ||
          (VK_API_VERSION_MAJOR(version)==1 && VK_API_VERSION_MINOR(version)>=3);
}
int memoryType(const VkPhysicalDeviceMemoryProperties& props,std::uint32_t bits,
               VkMemoryPropertyFlags required,VkMemoryPropertyFlags preferred){
    int fallback=-1;
    for(std::uint32_t i=0;i<props.memoryTypeCount;i++){
        if((bits & (1u<<i))==0)continue;
        const auto flags=props.memoryTypes[i].propertyFlags;
        if((flags&required)!=required)continue;
        if((flags&preferred)==preferred)return static_cast<int>(i);
        if(fallback<0)fallback=static_cast<int>(i);
    }
    return fallback;
}
std::string run(){
    Resources r;
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName="Switch Vulkan offscreen color test";
    app.apiVersion=VK_API_VERSION_1_3;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo=&app;
    auto status=vkCreateInstance(&ici,nullptr,&r.instance);
    if(status!=VK_SUCCESS)return "Vulkan 1.3 no pudo crear la instancia: "+std::to_string(status);
    std::uint32_t numGpu=0;
    status=vkEnumeratePhysicalDevices(r.instance,&numGpu,nullptr);
    if(status!=VK_SUCCESS || numGpu==0 || numGpu>16)return "No hay GPU Vulkan elegible.";
    std::vector<VkPhysicalDevice> gpuList(numGpu);
    status=vkEnumeratePhysicalDevices(r.instance,&numGpu,gpuList.data());
    if(status!=VK_SUCCESS)return "No se pudo enumerar la GPU.";
    VkPhysicalDevice gpu=gpuList[0];
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(gpu,&props);
    if(!version13(props.apiVersion))return "Este test requiere un dispositivo Vulkan >= 1.3.";
    VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    features.pNext=&f13;
    auto getFeatures=reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
        vkGetInstanceProcAddr(r.instance,"vkGetPhysicalDeviceFeatures2"));
    if(!getFeatures)return "Falta consulta de caracteristicas Vulkan 1.3.";
    getFeatures(gpu,&features);
    if(f13.dynamicRendering!=VK_TRUE)return "dynamicRendering no soportado.";
    const VkFormat format=VK_FORMAT_R8G8B8A8_UNORM;
    VkFormatProperties fp{};
    vkGetPhysicalDeviceFormatProperties(gpu,format,&fp);
    if((fp.optimalTilingFeatures&(VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|
       VK_FORMAT_FEATURE_TRANSFER_SRC_BIT))!=
       (VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_TRANSFER_SRC_BIT))
        return "R8G8B8A8 no admite attachment+readback en este driver.";
    std::uint32_t nq=0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu,&nq,nullptr);
    if(nq==0 || nq>64)return "No hay colas Vulkan.";
    std::vector<VkQueueFamilyProperties> queues(nq);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu,&nq,queues.data());
    std::uint32_t family=nq;
    for(std::uint32_t i=0;i<nq;i++)
        if(queues[i].queueCount && (queues[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)){
            family=i;break;
        }
    if(family==nq)return "Falta cola grafica.";
    const float priority=1.f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex=family;qci.queueCount=1;qci.pQueuePriorities=&priority;
    VkPhysicalDeviceVulkan13Features enable{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    enable.dynamicRendering=VK_TRUE;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount=1;dci.pQueueCreateInfos=&qci;dci.pNext=&enable;
    status=vkCreateDevice(gpu,&dci,nullptr,&r.device);
    if(status!=VK_SUCCESS)return "No se pudo crear el dispositivo: "+std::to_string(status);
    const auto beginRendering=reinterpret_cast<PFN_vkCmdBeginRendering>(
        vkGetDeviceProcAddr(r.device,"vkCmdBeginRendering"));
    const auto endRendering=reinterpret_cast<PFN_vkCmdEndRendering>(
        vkGetDeviceProcAddr(r.device,"vkCmdEndRendering"));
    if(!beginRendering || !endRendering)return "El driver no exporta comandos dynamicRendering.";
    VkQueue queue=VK_NULL_HANDLE;
    vkGetDeviceQueue(r.device,family,0,&queue);
    if(!queue)return "Cola grafica no disponible.";
    VkPhysicalDeviceMemoryProperties memory{};
    vkGetPhysicalDeviceMemoryProperties(gpu,&memory);

    VkImageCreateInfo imageCi{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageCi.imageType=VK_IMAGE_TYPE_2D;
    imageCi.format=format;
    imageCi.extent={offscreen::kWidth,offscreen::kHeight,1};
    imageCi.mipLevels=1;imageCi.arrayLayers=1;
    imageCi.samples=VK_SAMPLE_COUNT_1_BIT;
    imageCi.tiling=VK_IMAGE_TILING_OPTIMAL;
    imageCi.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageCi.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    status=vkCreateImage(r.device,&imageCi,nullptr,&r.image);
    if(status!=VK_SUCCESS)return "vkCreateImage fallo: "+std::to_string(status);
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(r.device,r.image,&req);
    int index=memoryType(memory,req.memoryTypeBits,0,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(index<0)return "No hay memoria apropiada para imagen Vulkan.";
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize=req.size;ai.memoryTypeIndex=static_cast<std::uint32_t>(index);
    status=vkAllocateMemory(r.device,&ai,nullptr,&r.imageMemory);
    if(status!=VK_SUCCESS)return "No se pudo reservar memoria de imagen.";
    status=vkBindImageMemory(r.device,r.image,r.imageMemory,0);
    if(status!=VK_SUCCESS)return "No se pudo enlazar memoria de imagen.";

    constexpr VkDeviceSize bufferSize=offscreen::kWidth*offscreen::kHeight*4;
    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size=bufferSize;bci.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    status=vkCreateBuffer(r.device,&bci,nullptr,&r.buffer);
    if(status!=VK_SUCCESS)return "vkCreateBuffer fallo.";
    vkGetBufferMemoryRequirements(r.device,r.buffer,&req);
    index=memoryType(memory,req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if(index<0)return "No hay memoria visible para leer el resultado de la GPU.";
    const bool coherent=(memory.memoryTypes[index].propertyFlags&
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)!=0;
    ai.allocationSize=req.size;ai.memoryTypeIndex=static_cast<std::uint32_t>(index);
    status=vkAllocateMemory(r.device,&ai,nullptr,&r.bufferMemory);
    if(status!=VK_SUCCESS)return "No se pudo reservar memoria de lectura.";
    status=vkBindBufferMemory(r.device,r.buffer,r.bufferMemory,0);
    if(status!=VK_SUCCESS)return "No se pudo enlazar el buffer de lectura.";

    VkImageViewCreateInfo viewCi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewCi.image=r.image;viewCi.viewType=VK_IMAGE_VIEW_TYPE_2D;
    viewCi.format=format;
    viewCi.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    viewCi.subresourceRange.levelCount=1;viewCi.subresourceRange.layerCount=1;
    VkImageView view=VK_NULL_HANDLE;
    status=vkCreateImageView(r.device,&viewCi,nullptr,&view);
    if(status!=VK_SUCCESS)return "No se pudo crear image view.";
    // Keep view alive until GPU work completes. The scope cleanup is explicit.
    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.queueFamilyIndex=family;
    status=vkCreateCommandPool(r.device,&pci,nullptr,&r.pool);
    if(status!=VK_SUCCESS){vkDestroyImageView(r.device,view,nullptr);return "No se pudo crear command pool.";}
    VkCommandBufferAllocateInfo cbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbi.commandPool=r.pool;cbi.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbi.commandBufferCount=1;
    VkCommandBuffer cb=VK_NULL_HANDLE;
    status=vkAllocateCommandBuffers(r.device,&cbi,&cb);
    if(status!=VK_SUCCESS){
        vkDestroyImageView(r.device,view,nullptr);return "No se pudo asignar command buffer.";
    }
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    status=vkBeginCommandBuffer(cb,&begin);
    if(status!=VK_SUCCESS){
        vkDestroyImageView(r.device,view,nullptr);return "No se pudo iniciar comandos.";
    }
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask=0;
    barrier.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.image=r.image;
    barrier.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount=1;
    barrier.subresourceRange.layerCount=1;
    vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         0,0,nullptr,0,nullptr,1,&barrier);

    VkRenderingAttachmentInfo attach{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    attach.imageView=view;
    attach.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attach.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
    attach.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    attach.clearValue.color.float32[0]=0.25f;
    attach.clearValue.color.float32[1]=0.5f;
    attach.clearValue.color.float32[2]=0.75f;
    attach.clearValue.color.float32[3]=1.0f;
    VkRenderingInfo render{VK_STRUCTURE_TYPE_RENDERING_INFO};
    render.renderArea.extent={offscreen::kWidth,offscreen::kHeight};
    render.layerCount=1;render.colorAttachmentCount=1;
    render.pColorAttachments=&attach;
    beginRendering(cb,&render);
    endRendering(cb);

    barrier.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    VkBufferImageCopy copy{};
    copy.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount=1;
    copy.imageExtent={offscreen::kWidth,offscreen::kHeight,1};
    vkCmdCopyImageToBuffer(cb,r.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,r.buffer,1,&copy);
    VkBufferMemoryBarrier readBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    readBarrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    readBarrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    readBarrier.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    readBarrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    readBarrier.buffer=r.buffer;readBarrier.size=bufferSize;
    vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,
                         0,0,nullptr,1,&readBarrier,0,nullptr);
    status=vkEndCommandBuffer(cb);
    if(status!=VK_SUCCESS){
        vkDestroyImageView(r.device,view,nullptr);return "No se pudo finalizar comandos.";
    }
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    status=vkCreateFence(r.device,&fci,nullptr,&r.fence);
    if(status!=VK_SUCCESS){
        vkDestroyImageView(r.device,view,nullptr);return "No se pudo crear GPU fence.";
    }
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount=1;submit.pCommandBuffers=&cb;
    status=vkQueueSubmit(queue,1,&submit,r.fence);
    if(status!=VK_SUCCESS){
        vkDestroyImageView(r.device,view,nullptr);return "vkQueueSubmit fallo: "+std::to_string(status);
    }
    // Small 16x16 offscreen workload. Timeout is a diagnostic, not a GPU kill.
    status=vkWaitForFences(r.device,1,&r.fence,VK_TRUE,5'000'000'000ULL);
    if(status!=VK_SUCCESS){
        // Device work might still be in flight; synchronize before destroying resources.
        // Report failure without attempting further submissions.
        const auto idle=vkDeviceWaitIdle(r.device);
        vkDestroyImageView(r.device,view,nullptr);
        return "GPU no completo a tiempo ("+std::to_string(status)+
               "), espera final: "+std::to_string(idle);
    }
    void* mapped=nullptr;
    status=vkMapMemory(r.device,r.bufferMemory,0,VK_WHOLE_SIZE,0,&mapped);
    if(status!=VK_SUCCESS || !mapped){
        vkDestroyImageView(r.device,view,nullptr);return "No se pudo leer el buffer de GPU.";
    }
    if(!coherent){
        VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        range.memory=r.bufferMemory;range.offset=0;range.size=VK_WHOLE_SIZE;
        const auto invalidated=vkInvalidateMappedMemoryRanges(r.device,1,&range);
        if(invalidated!=VK_SUCCESS){
            vkUnmapMemory(r.device,r.bufferMemory);
            vkDestroyImageView(r.device,view,nullptr);
            return "No se pudo invalidar la cache del buffer.";
        }
    }
    const bool correct=offscreen::verifyRgba(
       static_cast<const std::uint8_t*>(mapped),bufferSize);
    const auto* data=static_cast<const std::uint8_t*>(mapped);
    const auto center=(offscreen::kHeight/2*offscreen::kWidth+offscreen::kWidth/2)*4;
    std::array<int,4> rgba{data[center],data[center+1],data[center+2],data[center+3]};
    vkUnmapMemory(r.device,r.bufferMemory);
    vkDestroyImageView(r.device,view,nullptr);
    std::ostringstream report;
    report<<"PRUEBA GPU REAL: dynamicRendering + lectura\n"
          <<"GPU: "<<props.deviceName<<"\n"
          <<"Imagen Vulkan: 16x16 R8G8B8A8_UNORM\n"
          <<"Color esperado RGBA: 64, 128, 191, 255 (tolerancia 2)\n"
          <<"Color GPU centro: "<<rgba[0]<<", "<<rgba[1]<<", "<<rgba[2]<<", "<<rgba[3]<<"\n"
          <<"Enviar y sincronizar GPU: CORRECTO\n"
          <<offscreen::outcome(correct)
          <<"\n\nPrueba real de borrado/color y lectura con Vulkan 1.3. "
            "No compila shaders, no dibuja triangulos, no ejecuta NVN ni BOTW.";
    return report.str();
}
}
extern "C" JNIEXPORT jstring JNICALL
Java_org_example_botwlab_MainActivity_nativeOffscreenGpuCheck(JNIEnv* env,jclass){
    const auto result=run();
    return env->NewStringUTF(result.c_str());
}
