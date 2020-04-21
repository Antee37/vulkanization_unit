#pragma once
#include <Windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

namespace vk {
    class Gfx {
    public:
        Gfx();

        bool Init();
        void Destroy();

        bool IsQuitRequest();
        bool PollEventsAndRender();
        void Resize(unsigned int w, unsigned int h);

        void PreDraw();
        void Present();

        float* clear_color = 0;

        void* GetDevice() { return 0; }

        bool VSync = false;

    private:
        bool CreateDevice(HWND hWnd);
        void CleanupDevice();
        void CreateRenderTarget(); // ? assign ? resize
        void CleanupRenderTarget();

        // vulkanization specific

        struct Vulkan_Frame
        {
            VkCommandPool       CommandPool;
            VkCommandBuffer     CommandBuffer;
            VkFence             Fence;
            VkImage             Backbuffer;
            VkImageView         BackbufferView;
            VkFramebuffer       Framebuffer;
        };

        struct Vulkan_FrameSemaphores
        {
            VkSemaphore         ImageAcquiredSemaphore;
            VkSemaphore         RenderCompleteSemaphore;
        };

        struct Vulkan_Window
        {
            int                 Width;
            int                 Height;
            VkSwapchainKHR      Swapchain;
            VkSurfaceKHR        Surface;
            VkSurfaceFormatKHR  SurfaceFormat;
            VkPresentModeKHR    PresentMode;
            VkRenderPass        RenderPass;
            bool                ClearEnable;
            VkClearValue        ClearValue;
            uint32_t            FrameIndex;             // Current frame being rendered to (0 <= FrameIndex < FrameInFlightCount)
            uint32_t            ImageCount;             // Number of simultaneous in-flight frames (returned by vkGetSwapchainImagesKHR, usually derived from min_image_count)
            uint32_t            SemaphoreIndex;         // Current set of swapchain wait semaphores we're using (needs to be distinct from per frame data)
            Vulkan_Frame* Frames;
            Vulkan_FrameSemaphores* FrameSemaphores;

            Vulkan_Window()
            {
                memset(this, 0, sizeof(*this));
                PresentMode = VK_PRESENT_MODE_MAX_ENUM_KHR;
                ClearEnable = true;
            }
        };


        VkAllocationCallbacks*   m_allocator = NULL;
        VkInstance               m_instance = VK_NULL_HANDLE;
        VkPhysicalDevice         m_physicalDevice = VK_NULL_HANDLE;
        VkDevice                 m_device = VK_NULL_HANDLE;
        uint32_t                 m_queueFamily = (uint32_t)-1;
        VkQueue                  m_queue = VK_NULL_HANDLE;
        VkDebugReportCallbackEXT m_debugReport = VK_NULL_HANDLE;
        VkPipelineCache          m_pipelineCache = VK_NULL_HANDLE;
        VkDescriptorPool         m_descriptorPool = VK_NULL_HANDLE;

        int                      m_minImageCount = 2;
        bool                     g_SwapChainRebuild = false;
        int                      g_SwapChainResizeWidth = 0;
        int                      g_SwapChainResizeHeight = 0;

        void SetupVulkan(const char** extensions, uint32_t extensions_count);
        void SetupVulkanWindow(Vulkan_Window* wd, VkSurfaceKHR surface, int width, int height);
        void CleanupVulkan();
        void CleanupVulkanWindow();
        void FrameRender(Vulkan_Window* wd);
        void FramePresent(Vulkan_Window* wd);

        VkResult err;

        // update
        struct Vulkan_InitInfo
        {
            VkInstance          Instance;
            VkPhysicalDevice    PhysicalDevice;
            VkDevice            Device;
            uint32_t            QueueFamily;
            VkQueue             Queue;
            VkPipelineCache     PipelineCache;
            VkDescriptorPool    DescriptorPool;
            uint32_t            MinImageCount;          // >= 2
            uint32_t            ImageCount;             // >= MinImageCount
            VkSampleCountFlagBits        MSAASamples;   // >= VK_SAMPLE_COUNT_1_BIT
            const VkAllocationCallbacks* Allocator;
            void                (*CheckVkResultFn)(VkResult err);
        };


        Vulkan_Window m_mainWindowData;
        Vulkan_InitInfo m_vulkanInitInfo;

        VkRenderPass             m_renderPass = VK_NULL_HANDLE;
        VkDeviceSize             m_bufferMemoryAlignment = 256;
        VkPipelineCreateFlags    m_pipelineCreateFlags = 0x00;
        VkDescriptorSetLayout    m_descriptorSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout         m_pipelineLayout = VK_NULL_HANDLE;
        VkDescriptorSet          m_descriptorSet = VK_NULL_HANDLE;
        VkPipeline               m_pipeline = VK_NULL_HANDLE;


        VkSurfaceFormatKHR SelectSurfaceFormat(VkPhysicalDevice physical_device, VkSurfaceKHR surface, const VkFormat* request_formats, int request_formats_count, VkColorSpaceKHR request_color_space);
        VkPresentModeKHR SelectPresentMode(VkPhysicalDevice physical_device, VkSurfaceKHR surface, const VkPresentModeKHR* request_modes, int request_modes_count);

        void check_vk_result(VkResult err);

        bool Vulkan_Init(Vulkan_InitInfo* info, VkRenderPass render_pass);
        bool CreateDeviceObjects();

        void CreateVulkanWindow(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device, Vulkan_Window* wd, uint32_t queue_family, const VkAllocationCallbacks* allocator, int width, int height, uint32_t min_image_count);
        void DestroyVulkanWindow(VkInstance instance, VkDevice device, Vulkan_Window* wd, const VkAllocationCallbacks* allocator);
        void CreateWindowSwapChain(VkPhysicalDevice physical_device, VkDevice device, Vulkan_Window* wd, const VkAllocationCallbacks* allocator, int w, int h, uint32_t min_image_count);
        void CreateWindowCommandBuffers(VkPhysicalDevice physical_device, VkDevice device, Vulkan_Window* wd, uint32_t queue_family, const VkAllocationCallbacks* allocator);


        void DestroyFrame(VkDevice device, Vulkan_Frame* fd, const VkAllocationCallbacks* allocator);
        void DestroyFrameSemaphores(VkDevice device, Vulkan_FrameSemaphores* fsd, const VkAllocationCallbacks* allocator);
        // win32

        HWND hwnd = 0;
        WNDCLASSEX wc;
        MSG msg;

        int win_w = 0, win_h = 0;
        float ClearColor[4] = { 1,0,0,1 };

        GViewPlane*         viewplane = 0;
        CD3D11_VIEWPORT	    m_viewport; // TODO
    };
}
