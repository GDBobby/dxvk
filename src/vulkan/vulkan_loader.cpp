#include <tuple>

#include "vulkan_loader.h"

#include "../util/log/log.h"

#include "../util/util_string.h"
#include "../util/util_win32_compat.h"

#include "Pipe_Compressed.h"

#include <chrono>
#include <iostream>

#include "stacktrace"


//bobby pointers
PFN_vkQueueBeginDebugUtilsLabelEXT pfnQueueBegin = nullptr;
PFN_vkQueueEndDebugUtilsLabelEXT pfnQueueEnd = nullptr;
PFN_vkSetDebugUtilsObjectNameEXT pfnSetObjectName = nullptr;
PFN_vkCmdBeginDebugUtilsLabelEXT pfnBeginLabel = nullptr;
PFN_vkCmdEndDebugUtilsLabelEXT pfnEndLabel = nullptr;

namespace dxvk::vk {

  static std::pair<HMODULE, PFN_vkGetInstanceProcAddr> loadVulkanLibrary() {
    static const std::array<const char*, 2> dllNames = {{
#ifdef _WIN32
      "winevulkan.dll",
      "vulkan-1.dll",
#else
      "libvulkan.so",
      "libvulkan.so.1",
#endif
    }};

    for (auto dllName : dllNames) {
      HMODULE library = LoadLibraryA(dllName);

      if (!library)
        continue;

      auto proc = GetProcAddress(library, "vkGetInstanceProcAddr");

      if (!proc) {
        FreeLibrary(library);
        continue;
      }

      Logger::info(str::format("Vulkan: Found vkGetInstanceProcAddr in ", dllName, " @ 0x", std::hex, reinterpret_cast<uintptr_t>(proc)));
      return std::make_pair(library, reinterpret_cast<PFN_vkGetInstanceProcAddr>(proc));
    }

    Logger::err("Vulkan: vkGetInstanceProcAddr not found");
    return { };
  }

  LibraryLoader::LibraryLoader() {
    std::tie(m_library, m_getInstanceProcAddr) = loadVulkanLibrary();
  }

  LibraryLoader::LibraryLoader(PFN_vkGetInstanceProcAddr loaderProc) {
    m_getInstanceProcAddr = loaderProc;
  }

  LibraryLoader::~LibraryLoader() {
    if (m_library)
      FreeLibrary(m_library);
  }

  PFN_vkVoidFunction LibraryLoader::sym(VkInstance instance, const char* name) const {
    return m_getInstanceProcAddr(instance, name);
  }

  PFN_vkVoidFunction LibraryLoader::sym(const char* name) const {
    return sym(nullptr, name);
  }

  bool LibraryLoader::valid() const {
    return m_getInstanceProcAddr != nullptr;
  }
  
  
  InstanceLoader::InstanceLoader(const Rc<LibraryLoader>& library, bool owned, VkInstance instance)
  : m_library(library), m_instance(instance), m_owned(owned) { }
  
  
  PFN_vkVoidFunction InstanceLoader::sym(const char* name) const {
    return m_library->sym(m_instance, name);
  }

  dxvk::vk::DeviceFn* global_device = nullptr;

  DEBUG_STRUCT::DEBUG_STRUCT(std::string const& printline){
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
    printf(printline.c_str());
  }

  VkResult dxvk::vk::DeviceFn::intermediate_vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines){

    //printf("beginning of vkCreateGraphicsPipelines\n");

    static bool firstTime = true;
    static std::string fileLoc{};

    std::ios_base::openmode flags = std::ios::app;
    if(firstTime) {
      flags = std::ios::trunc;
      firstTime = false;    
      const std::chrono::time_point now{std::chrono::system_clock::now()};
 
      const auto today = std::chrono::floor<std::chrono::days>(now);
      const std::chrono::year_month_day ymd{today};
      const auto time = now - today;
      std::chrono::hh_mm_ss hms{std::chrono::floor<std::chrono::seconds>(time)};

      fileLoc = 
      std::string("vkpipe_") + 
      std::to_string(static_cast<int>(ymd.year())) + '_' + 
      std::to_string(static_cast<unsigned>(ymd.month())) + '_' + 
      std::to_string(static_cast<unsigned>(ymd.day())) + '_' +

      std::to_string(hms.hours().count()) + '_' + 
      std::to_string(hms.minutes().count()) + '_' + 
      std::to_string(hms.seconds().count());
    }

    //printf("before creating file vkCreateGraphicsPipelines\n");
    std::ofstream outpipefile{fileLoc};

    for(uint8_t i = 0; i < createInfoCount; i++){
      //printf("before compression vkCreateGraphicsPipelines - %d/%d\n", i, createInfoCount);
      //VkCompression::CompressedVkGraphicsPipelineCreateInfo createInfo(pCreateInfos[i]);
      //printf("before writing to file vkCreateGraphicsPipelines - %d\n", i);
      //createInfo.WriteToFile(outpipefile);
    }
    //printf("end of vkCreateGraphicsPipelines\n");
    return global_device->real_vkCreateGraphicsPipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
  }
  
  
  DeviceLoader::DeviceLoader(const Rc<InstanceLoader>& library, bool owned, VkDevice device)
  : m_library(library)
  , m_getDeviceProcAddr(reinterpret_cast<PFN_vkGetDeviceProcAddr>(
      m_library->sym("vkGetDeviceProcAddr"))),
    m_device(device), m_owned(owned) { 
      printf("device loader init \n");
    }
  

    void DeviceLoader::InitializeBeginLabelFunction() const {
      if(pfnBeginLabel == nullptr){
        
        printf("before setting BEGIN label\n");
        pfnBeginLabel = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(sym("vkCmdBeginDebugUtilsLabelEXT"));
        printf("set both label functions - %zu - %zu\n", reinterpret_cast<uint64_t>(pfnBeginLabel), reinterpret_cast<uint64_t>(pfnEndLabel));
      }
    }
    void DeviceLoader::InitializeEndLabelFunction() const {
      if(pfnEndLabel == nullptr){
        printf("before setting END label\n");
        pfnEndLabel = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(sym("vkCmdEndDebugUtilsLabelEXT"));
        printf("set both label functions - %zu - %zu\n", reinterpret_cast<uint64_t>(pfnBeginLabel), reinterpret_cast<uint64_t>(pfnEndLabel));
      }
    }
  
  PFN_vkVoidFunction DeviceLoader::sym(const char* name) const {
    return m_getDeviceProcAddr(m_device, name);
  }
  
  void DeviceLoader::BeginLabel(VkCommandBuffer cmdBuf, BobbyDebugUtil utilLabel) const {
    VkDebugUtilsLabelEXT util;
    util.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    util.pNext = nullptr;
    util.pLabelName = utilLabel.name.c_str();
    util.color[0] = utilLabel.color[0];
    util.color[1] = utilLabel.color[1];
    util.color[2] = utilLabel.color[2];
    util.color[3] = utilLabel.color[3];
    pfnBeginLabel(cmdBuf, &util);
  }

  void DeviceLoader::BeginLabel(VkCommandBuffer cmdBuf, const char* name, float red, float green, float blue, float alpha) const {
    VkDebugUtilsLabelEXT utilLabel{};
    utilLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    utilLabel.pNext = nullptr;
    utilLabel.color[0] = red;
    utilLabel.color[1] = green;
    utilLabel.color[2] = blue;
    utilLabel.color[3] = alpha;
    utilLabel.pLabelName = name;
    pfnBeginLabel(cmdBuf, &utilLabel);
  }
  void DeviceLoader::EndLabel(VkCommandBuffer cmdBuf) const {
    if(pfnEndLabel == nullptr) {
      
      pfnEndLabel = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(sym("vkCmdEndDebugUtilsLabelEXT"));
    }
      pfnEndLabel(cmdBuf);
  }
  
    
  
  
  LibraryFn::LibraryFn() { }
  LibraryFn::LibraryFn(PFN_vkGetInstanceProcAddr loaderProc)
  : LibraryLoader(loaderProc) { }
  LibraryFn::~LibraryFn() { }
  
  
  InstanceFn::InstanceFn(const Rc<LibraryLoader>& library, bool owned, VkInstance instance)
  : InstanceLoader(library, owned, instance) { }
  InstanceFn::~InstanceFn() {
    if (m_owned)
      this->vkDestroyInstance(m_instance, nullptr);
  }
  
  
  DeviceFn::DeviceFn(const Rc<InstanceLoader>& library, bool owned, VkDevice device)
  : DeviceLoader(library, owned, device) {
    printf("initialization of devicefn\n");
    global_device = this;


   }
  DeviceFn::~DeviceFn() {
    if (m_owned)
      this->vkDestroyDevice(m_device, nullptr);
  }
  
}