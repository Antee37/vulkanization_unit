#include "vkGfx.h"
#include <assert.h>
#include <vector>

namespace vk {
	static Gfx* w32handler = 0;

    LRESULT WINAPI WndProcX(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg)
		{
		case WM_SIZE:
			if (wParam != SIZE_MINIMIZED)
			{
				if (w32handler)
					w32handler->Resize((UINT)LOWORD(lParam), (UINT)HIWORD(lParam));
			}
			return 0;
		case WM_SYSCOMMAND:
			if ((wParam & 0xfff0) == SC_KEYMENU)
				return 0;
			break;
		case WM_DESTROY:
			::PostQuitMessage(0);
			return 0;
		}
		return ::DefWindowProc(hWnd, msg, wParam, lParam);
	}

    bool Gfx::IsQuitRequest() {
        return msg.message == WM_QUIT;
    }

    bool Gfx::PollEventsAndRender() {
        if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            return false;
        }
        return true;
    }

    void Gfx::Resize(unsigned int w, unsigned int h) {
        // TODO
    }


	bool Gfx::Init() {
		wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProcX, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"VRg fovea based rendering example", NULL };
		::RegisterClassEx(&wc);
		hwnd = ::CreateWindow(wc.lpszClassName, L"VRg Vulkan fovea based rendering example", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);

		::ShowWindow(hwnd, SW_SHOWDEFAULT);
		::UpdateWindow(hwnd);

		ZeroMemory(&msg, sizeof(msg));

        if (!CreateDevice(hwnd))
		{
			CleanupVulkan();
			::UnregisterClass(wc.lpszClassName, wc.hInstance);
			return false;
		}
		Resize(1280, 800);

		viewplane = new GViewPlane(m_pd3dDevice);

		w32handler = this;

		return true;
	}

    void Gfx::Destroy() {
        CleanupDevice();
        ::DestroyWindow(hwnd);
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
    }

    void Gfx::CleanupDevice() {
        err = vkDeviceWaitIdle(m_device);
        check_vk_result(err);
        CleanupVulkanWindow();
        CleanupVulkan();
    }

    bool Gfx::CreateDevice(HWND hwnd) {
        // vulkan specific
        uint32_t extensions_count = 2;
        const char* ext1 = "VK_KHR_surface";
        const char* ext = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;

        const char* extensions[2] = { ext1, ext };// glfwGetRequiredInstanceExtensions(&extensions_count);
        SetupVulkan(extensions, extensions_count);

        // Create Window Surface
        VkSurfaceKHR surface;
        // TODO what is needed in structs
        VkWin32SurfaceCreateInfoKHR info;
        info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        info.hwnd = hwnd;
        info.hinstance = GetModuleHandle(0);

        VkResult err = vkCreateWin32SurfaceKHR(m_instance, &info, 0, &surface);
        check_vk_result(err);

        //  Create Framebuffers
        int w, h;
        RECT rect;
        GetWindowRect(hwnd, &rect);
        h = rect.bottom - rect.top;
        w = rect.right - rect.left;

        Vulkan_Window* wd = &m_mainWindowData;
        SetupVulkanWindow(wd, surface, w, h);

        Vulkan_InitInfo init_info = {}; // TODO: init info and m_ values duplicate -> unify
        init_info.Instance = m_instance;
        init_info.PhysicalDevice = m_physicalDevice;
        init_info.Device = m_device;
        init_info.QueueFamily = m_queueFamily;
        init_info.Queue = m_queue;
        init_info.PipelineCache = m_pipelineCache;
        init_info.DescriptorPool = m_descriptorPool;
        init_info.Allocator = m_allocator;
        init_info.MinImageCount = m_minImageCount;
        init_info.ImageCount = wd->ImageCount;
        init_info.CheckVkResultFn = check_vk_result; // TODO static
        Vulkan_Init(&init_info, wd->RenderPass);


        // end
        return true; // TODO
    }

    void Gfx::SetupVulkan(const char** extensions, uint32_t extensions_count)
    {
        VkResult err;

        // Create Vulkan Instance
        {
            VkInstanceCreateInfo create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            create_info.enabledExtensionCount = extensions_count;
            create_info.ppEnabledExtensionNames = extensions;

#ifdef IMGUI_VULKAN_DEBUG_REPORT
            // Enabling multiple validation layers grouped as LunarG standard validation
            const char* layers[] = { "VK_LAYER_LUNARG_standard_validation" };
            create_info.enabledLayerCount = 1;
            create_info.ppEnabledLayerNames = layers;

            // Enable debug report extension (we need additional storage, so we duplicate the user array to add our new extension to it)
            const char** extensions_ext = (const char**)malloc(sizeof(const char*) * (extensions_count + 1));
            memcpy(extensions_ext, extensions, extensions_count * sizeof(const char*));
            extensions_ext[extensions_count] = "VK_EXT_debug_report";
            create_info.enabledExtensionCount = extensions_count + 1;
            create_info.ppEnabledExtensionNames = extensions_ext;

            // Create Vulkan Instance
            err = vkCreateInstance(&create_info, g_Allocator, &g_Instance);
            check_vk_result(err);
            free(extensions_ext);

            // Get the function pointer (required for any extensions)
            auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(g_Instance, "vkCreateDebugReportCallbackEXT");
            IM_ASSERT(vkCreateDebugReportCallbackEXT != NULL);

            // Setup the debug report callback
            VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
            debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
            debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
            debug_report_ci.pfnCallback = debug_report;
            debug_report_ci.pUserData = NULL;
            err = vkCreateDebugReportCallbackEXT(g_Instance, &debug_report_ci, g_Allocator, &g_DebugReport);
            check_vk_result(err);
#else
            // Create Vulkan Instance without any debug feature
            err = vkCreateInstance(&create_info, m_allocator, &m_instance);
            check_vk_result(err);
#endif
        }

        // Select GPU
        {
            uint32_t gpu_count;
            err = vkEnumeratePhysicalDevices(m_instance, &gpu_count, NULL);
            check_vk_result(err);
            assert(gpu_count > 0);

            VkPhysicalDevice* gpus = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * gpu_count);
            err = vkEnumeratePhysicalDevices(m_instance, &gpu_count, gpus);
            check_vk_result(err);

            // If a number >1 of GPUs got reported, you should find the best fit GPU for your purpose
            // e.g. VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU if available, or with the greatest memory available, etc.
            // for sake of simplicity we'll just take the first one, assuming it has a graphics queue family.
            m_physicalDevice = gpus[0];
            free(gpus);
        }

        // Select graphics queue family
        {
            uint32_t count;
            vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &count, NULL);
            VkQueueFamilyProperties* queues = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * count);
            vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &count, queues);
            for (uint32_t i = 0; i < count; i++)
                if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    m_queueFamily = i;
                    break;
                }
            free(queues);
            assert(m_queueFamily != (uint32_t)-1);
        }

        // Create Logical Device (with 1 queue)
        {
            int device_extension_count = 1;
            const char* device_extensions[] = { "VK_KHR_swapchain" };
            const float queue_priority[] = { 1.0f };
            VkDeviceQueueCreateInfo queue_info[1] = {};
            queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_info[0].queueFamilyIndex = m_queueFamily;
            queue_info[0].queueCount = 1;
            queue_info[0].pQueuePriorities = queue_priority;
            VkDeviceCreateInfo create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
            create_info.pQueueCreateInfos = queue_info;
            create_info.enabledExtensionCount = device_extension_count;
            create_info.ppEnabledExtensionNames = device_extensions;
            err = vkCreateDevice(m_physicalDevice, &create_info, m_allocator, &m_device);
            check_vk_result(err);
            vkGetDeviceQueue(m_device, m_queueFamily, 0, &m_queue);
        }

        // Create Descriptor Pool
        {
            VkDescriptorPoolSize pool_sizes[] =
            {
                { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
                { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
                { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
            };
            VkDescriptorPoolCreateInfo pool_info = {};
            pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            pool_info.maxSets = 1000 * sizeof(pool_sizes); // TODO why 1000
            pool_info.poolSizeCount = (uint32_t)sizeof(pool_sizes);
            pool_info.pPoolSizes = pool_sizes;
            err = vkCreateDescriptorPool(m_device, &pool_info, m_allocator, &m_descriptorPool);
            check_vk_result(err);
        }
    }

	void Gfx::SetupVulkanWindow(Gfx::Vulkan_Window* wd, VkSurfaceKHR surface, int width, int height) {
        wd->Surface = surface;

        // Check for WSI support
        VkBool32 res;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, m_queueFamily, wd->Surface, &res);
        if (res != VK_TRUE)
        {
            fprintf(stderr, "Error no WSI support on physical device 0\n");
            exit(-1);
        }

        // Select Surface Format
        const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
        const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
        wd->SurfaceFormat = SelectSurfaceFormat(m_physicalDevice, wd->Surface, requestSurfaceImageFormat, (size_t)sizeof(requestSurfaceImageFormat), requestSurfaceColorSpace);

        // Select Present Mode
#ifdef IMGUI_UNLIMITED_FRAME_RATE
        VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
#else
        VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
#endif
        wd->PresentMode = SelectPresentMode(m_physicalDevice, wd->Surface, &present_modes[0], sizeof(present_modes));
        //printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

        // Create SwapChain, RenderPass, Framebuffer, etc.
        assert(m_minImageCount >= 2);
        CreateVulkanWindow(m_instance, m_physicalDevice, m_device, wd, m_queueFamily, m_allocator, width, height, m_minImageCount);
	}

    VkSurfaceFormatKHR Gfx::SelectSurfaceFormat(VkPhysicalDevice physical_device, VkSurfaceKHR surface, const VkFormat* request_formats, int request_formats_count, VkColorSpaceKHR request_color_space) {
        assert(request_formats != NULL);
        assert(request_formats_count > 0);

        // Per Spec Format and View Format are expected to be the same unless VK_IMAGE_CREATE_MUTABLE_BIT was set at image creation
        // Assuming that the default behavior is without setting this bit, there is no need for separate Swapchain image and image view format
        // Additionally several new color spaces were introduced with Vulkan Spec v1.0.40,
        // hence we must make sure that a format with the mostly available color space, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, is found and used.
        uint32_t avail_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &avail_count, NULL);
        std::vector<VkSurfaceFormatKHR> avail_format;
        avail_format.resize((int)avail_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &avail_count, avail_format.data);

        // First check if only one format, VK_FORMAT_UNDEFINED, is available, which would imply that any format is available
        if (avail_count == 1)
        {
            if (avail_format[0].format == VK_FORMAT_UNDEFINED)
            {
                VkSurfaceFormatKHR ret;
                ret.format = request_formats[0];
                ret.colorSpace = request_color_space;
                return ret;
            }
            else
            {
                // No point in searching another format
                return avail_format[0];
            }
        }
        else
        {
            // Request several formats, the first found will be used
            for (int request_i = 0; request_i < request_formats_count; request_i++)
                for (uint32_t avail_i = 0; avail_i < avail_count; avail_i++)
                    if (avail_format[avail_i].format == request_formats[request_i] && avail_format[avail_i].colorSpace == request_color_space)
                        return avail_format[avail_i];

            // If none of the requested image formats could be found, use the first available
            return avail_format[0];
        }
    }

    VkPresentModeKHR Gfx::SelectPresentMode(VkPhysicalDevice physical_device, VkSurfaceKHR surface, const VkPresentModeKHR* request_modes, int request_modes_count)
    {
        assert(request_modes != NULL);
        assert(request_modes_count > 0);

        // Request a certain mode and confirm that it is available. If not use VK_PRESENT_MODE_FIFO_KHR which is mandatory
        uint32_t avail_count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &avail_count, NULL);
        std::vector<VkPresentModeKHR> avail_modes;
        avail_modes.resize((int)avail_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &avail_count, avail_modes.data);
        //for (uint32_t avail_i = 0; avail_i < avail_count; avail_i++)
        //    printf("[vulkan] avail_modes[%d] = %d\n", avail_i, avail_modes[avail_i]);

        for (int request_i = 0; request_i < request_modes_count; request_i++)
            for (uint32_t avail_i = 0; avail_i < avail_count; avail_i++)
                if (request_modes[request_i] == avail_modes[avail_i])
                    return request_modes[request_i];

        return VK_PRESENT_MODE_FIFO_KHR; // Always available
    }

    void Gfx::CreateVulkanWindow(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device, Vulkan_Window* wd, uint32_t queue_family, const VkAllocationCallbacks* allocator, int width, int height, uint32_t min_image_count)
    {
        (void)instance;
        CreateWindowSwapChain(physical_device, device, wd, allocator, width, height, min_image_count);
        CreateWindowCommandBuffers(physical_device, device, wd, queue_family, allocator);
    }

    void Gfx::check_vk_result(VkResult err)
    {
        Vulkan_InitInfo* v = &m_vulkanInitInfo;
        if (v->CheckVkResultFn)
            v->CheckVkResultFn(err);
    }

    int GetMinImageCountFromPresentMode(VkPresentModeKHR present_mode)
    {
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
            return 3;
        if (present_mode == VK_PRESENT_MODE_FIFO_KHR || present_mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR)
            return 2;
        if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
            return 1;
        assert(0);
        return 1;
    }

    void Gfx::CreateWindowSwapChain(VkPhysicalDevice physical_device, VkDevice device, Vulkan_Window* wd, const VkAllocationCallbacks* allocator, int w, int h, uint32_t min_image_count)
    {
        VkResult err;
        VkSwapchainKHR old_swapchain = wd->Swapchain;
        err = vkDeviceWaitIdle(device);
        check_vk_result(err);

        // We don't use ImGui_ImplVulkanH_DestroyWindow() because we want to preserve the old swapchain to create the new one.
        // Destroy old Framebuffer
        for (uint32_t i = 0; i < wd->ImageCount; i++)
        {
            DestroyFrame(device, &wd->Frames[i], allocator);
            DestroyFrameSemaphores(device, &wd->FrameSemaphores[i], allocator);
        }
        if (wd->Frames) {
            delete wd->Frames; wd->Frames = 0;
        }
        if (wd->FrameSemaphores) {
            delete wd->FrameSemaphores; wd->FrameSemaphores = 0;
        }
        wd->Frames = NULL;
        wd->FrameSemaphores = NULL;
        wd->ImageCount = 0;
        if (wd->RenderPass)
            vkDestroyRenderPass(device, wd->RenderPass, allocator);

        // If min image count was not specified, request different count of images dependent on selected present mode
        if (min_image_count == 0)
            min_image_count = GetMinImageCountFromPresentMode(wd->PresentMode);

        // Create Swapchain
        {
            VkSwapchainCreateInfoKHR info = {};
            info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            info.surface = wd->Surface;
            info.minImageCount = min_image_count;
            info.imageFormat = wd->SurfaceFormat.format;
            info.imageColorSpace = wd->SurfaceFormat.colorSpace;
            info.imageArrayLayers = 1;
            info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;           // Assume that graphics family == present family
            info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
            info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            info.presentMode = wd->PresentMode;
            info.clipped = VK_TRUE;
            info.oldSwapchain = old_swapchain;
            VkSurfaceCapabilitiesKHR cap;
            err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, wd->Surface, &cap);
            check_vk_result(err);
            if (info.minImageCount < cap.minImageCount)
                info.minImageCount = cap.minImageCount;
            else if (cap.maxImageCount != 0 && info.minImageCount > cap.maxImageCount)
                info.minImageCount = cap.maxImageCount;

            if (cap.currentExtent.width == 0xffffffff)
            {
                info.imageExtent.width = wd->Width = w;
                info.imageExtent.height = wd->Height = h;
            }
            else
            {
                info.imageExtent.width = wd->Width = cap.currentExtent.width;
                info.imageExtent.height = wd->Height = cap.currentExtent.height;
            }
            err = vkCreateSwapchainKHR(device, &info, allocator, &wd->Swapchain);
            check_vk_result(err);
            err = vkGetSwapchainImagesKHR(device, wd->Swapchain, &wd->ImageCount, NULL);
            check_vk_result(err);
            VkImage backbuffers[16] = {};
            assert(wd->ImageCount >= min_image_count);
            assert(wd->ImageCount < sizeof(backbuffers));
            err = vkGetSwapchainImagesKHR(device, wd->Swapchain, &wd->ImageCount, backbuffers);
            check_vk_result(err);

            assert(wd->Frames == NULL);
            wd->Frames = new Vulkan_Frame[wd->ImageCount];
            wd->FrameSemaphores = new Vulkan_FrameSemaphores[wd->ImageCount];
            memset(wd->Frames, 0, sizeof(wd->Frames[0]) * wd->ImageCount);
            memset(wd->FrameSemaphores, 0, sizeof(wd->FrameSemaphores[0]) * wd->ImageCount);
            for (uint32_t i = 0; i < wd->ImageCount; i++)
                wd->Frames[i].Backbuffer = backbuffers[i];
        }
        if (old_swapchain)
            vkDestroySwapchainKHR(device, old_swapchain, allocator);

        // Create the Render Pass
        {
            VkAttachmentDescription attachment = {};
            attachment.format = wd->SurfaceFormat.format;
            attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp = wd->ClearEnable ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            VkAttachmentReference color_attachment = {};
            color_attachment.attachment = 0;
            color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &color_attachment;
            VkSubpassDependency dependency = {};
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask = 0;
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            VkRenderPassCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            info.attachmentCount = 1;
            info.pAttachments = &attachment;
            info.subpassCount = 1;
            info.pSubpasses = &subpass;
            info.dependencyCount = 1;
            info.pDependencies = &dependency;
            err = vkCreateRenderPass(device, &info, allocator, &wd->RenderPass);
            check_vk_result(err);
        }

        // Create The Image Views
        {
            VkImageViewCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            info.format = wd->SurfaceFormat.format;
            info.components.r = VK_COMPONENT_SWIZZLE_R;
            info.components.g = VK_COMPONENT_SWIZZLE_G;
            info.components.b = VK_COMPONENT_SWIZZLE_B;
            info.components.a = VK_COMPONENT_SWIZZLE_A;
            VkImageSubresourceRange image_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            info.subresourceRange = image_range;
            for (uint32_t i = 0; i < wd->ImageCount; i++)
            {
                Vulkan_Frame* fd = &wd->Frames[i];
                info.image = fd->Backbuffer;
                err = vkCreateImageView(device, &info, allocator, &fd->BackbufferView);
                check_vk_result(err);
            }
        }

        // Create Framebuffer
        {
            VkImageView attachment[1];
            VkFramebufferCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            info.renderPass = wd->RenderPass;
            info.attachmentCount = 1;
            info.pAttachments = attachment;
            info.width = wd->Width;
            info.height = wd->Height;
            info.layers = 1;
            for (uint32_t i = 0; i < wd->ImageCount; i++)
            {
                Vulkan_Frame* fd = &wd->Frames[i];
                attachment[0] = fd->BackbufferView;
                err = vkCreateFramebuffer(device, &info, allocator, &fd->Framebuffer);
                check_vk_result(err);
            }
        }
    }

    void Gfx::CreateWindowCommandBuffers(VkPhysicalDevice physical_device, VkDevice device, Vulkan_Window* wd, uint32_t queue_family, const VkAllocationCallbacks* allocator)
    {
        assert(physical_device != VK_NULL_HANDLE && device != VK_NULL_HANDLE);
        (void)physical_device;
        (void)allocator;

        // Create Command Buffers
        VkResult err;
        for (uint32_t i = 0; i < wd->ImageCount; i++)
        {
            Vulkan_Frame* fd = &wd->Frames[i];
            Vulkan_FrameSemaphores* fsd = &wd->FrameSemaphores[i];
            {
                VkCommandPoolCreateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
                info.queueFamilyIndex = queue_family;
                err = vkCreateCommandPool(device, &info, allocator, &fd->CommandPool);
                check_vk_result(err);
            }
            {
                VkCommandBufferAllocateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                info.commandPool = fd->CommandPool;
                info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                info.commandBufferCount = 1;
                err = vkAllocateCommandBuffers(device, &info, &fd->CommandBuffer);
                check_vk_result(err);
            }
            {
                VkFenceCreateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
                err = vkCreateFence(device, &info, allocator, &fd->Fence);
                check_vk_result(err);
            }
            {
                VkSemaphoreCreateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                err = vkCreateSemaphore(device, &info, allocator, &fsd->ImageAcquiredSemaphore);
                check_vk_result(err);
                err = vkCreateSemaphore(device, &info, allocator, &fsd->RenderCompleteSemaphore);
                check_vk_result(err);
            }
        }
    }

    void Gfx::DestroyFrame(VkDevice device, Vulkan_Frame* fd, const VkAllocationCallbacks* allocator)
    {
        vkDestroyFence(device, fd->Fence, allocator);
        vkFreeCommandBuffers(device, fd->CommandPool, 1, &fd->CommandBuffer);
        vkDestroyCommandPool(device, fd->CommandPool, allocator);
        fd->Fence = VK_NULL_HANDLE;
        fd->CommandBuffer = VK_NULL_HANDLE;
        fd->CommandPool = VK_NULL_HANDLE;

        vkDestroyImageView(device, fd->BackbufferView, allocator);
        vkDestroyFramebuffer(device, fd->Framebuffer, allocator);
    }

    void Gfx::DestroyFrameSemaphores(VkDevice device, Vulkan_FrameSemaphores* fsd, const VkAllocationCallbacks* allocator)
    {
        vkDestroySemaphore(device, fsd->ImageAcquiredSemaphore, allocator);
        vkDestroySemaphore(device, fsd->RenderCompleteSemaphore, allocator);
        fsd->ImageAcquiredSemaphore = fsd->RenderCompleteSemaphore = VK_NULL_HANDLE;
    }

    void Gfx::CleanupVulkan()
    {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, m_allocator);

#ifdef IMGUI_VULKAN_DEBUG_REPORT
        // Remove the debug report callback
        auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(g_Instance, "vkDestroyDebugReportCallbackEXT");
        vkDestroyDebugReportCallbackEXT(g_Instance, g_DebugReport, g_Allocator);
#endif // IMGUI_VULKAN_DEBUG_REPORT

        vkDestroyDevice(m_device, m_allocator);
        vkDestroyInstance(m_instance, m_allocator);
    }

    void Gfx::DestroyVulkanWindow(VkInstance instance, VkDevice device, Vulkan_Window* wd, const VkAllocationCallbacks* allocator)
    {
        vkDeviceWaitIdle(device); // FIXME: We could wait on the Queue if we had the queue in wd-> (otherwise VulkanH functions can't use globals)
        //vkQueueWaitIdle(g_Queue);

        for (uint32_t i = 0; i < wd->ImageCount; i++)
        {
            DestroyFrame(device, &wd->Frames[i], allocator);
            DestroyFrameSemaphores(device, &wd->FrameSemaphores[i], allocator);
        }
        if (wd->Frames) {
            delete wd->Frames;
            wd->Frames = 0;
        }
        if (wd->FrameSemaphores) {
            delete wd->FrameSemaphores;
            wd->FrameSemaphores = 0;
        }
        wd->Frames = NULL;
        wd->FrameSemaphores = NULL;
        vkDestroyRenderPass(device, wd->RenderPass, allocator);
        vkDestroySwapchainKHR(device, wd->Swapchain, allocator);
        vkDestroySurfaceKHR(instance, wd->Surface, allocator);

        *wd = Vulkan_Window();
    }


    void Gfx::CleanupVulkanWindow()
    {
        DestroyVulkanWindow(m_instance, m_device, &m_mainWindowData, m_allocator);
    }

    bool Gfx::Vulkan_Init(Vulkan_InitInfo* info, VkRenderPass render_pass)
    {
        
        assert(info->Instance != VK_NULL_HANDLE);
        assert(info->PhysicalDevice != VK_NULL_HANDLE);
        assert(info->Device != VK_NULL_HANDLE);
        assert(info->Queue != VK_NULL_HANDLE);
        assert(info->DescriptorPool != VK_NULL_HANDLE);
        assert(info->MinImageCount >= 2);
        assert(info->ImageCount >= info->MinImageCount);
        assert(render_pass != VK_NULL_HANDLE);

        m_vulkanInitInfo = *info;
        m_renderPass = render_pass;
        CreateDeviceObjects();

        return true;
    }

    bool Gfx::CreateDeviceObjects()
    {
        Vulkan_InitInfo* v = &m_vulkanInitInfo;
        VkResult err;
        VkShaderModule vert_module;
        VkShaderModule frag_module;

        // Create The Shader Modules:
        {
            VkShaderModuleCreateInfo vert_info = {};
            vert_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            vert_info.codeSize = sizeof(__glsl_shader_vert_spv);
            vert_info.pCode = (uint32_t*)__glsl_shader_vert_spv;
            err = vkCreateShaderModule(v->Device, &vert_info, v->Allocator, &vert_module);
            check_vk_result(err);
            VkShaderModuleCreateInfo frag_info = {};
            frag_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            frag_info.codeSize = sizeof(__glsl_shader_frag_spv);
            frag_info.pCode = (uint32_t*)__glsl_shader_frag_spv;
            err = vkCreateShaderModule(v->Device, &frag_info, v->Allocator, &frag_module);
            check_vk_result(err);
        }

        if (!m_fontSampler)
        {
            VkSamplerCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            info.magFilter = VK_FILTER_LINEAR;
            info.minFilter = VK_FILTER_LINEAR;
            info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            info.minLod = -1000;
            info.maxLod = 1000;
            info.maxAnisotropy = 1.0f;
            err = vkCreateSampler(v->Device, &info, v->Allocator, &g_FontSampler);
            check_vk_result(err);
        }

        if (!m_descriptorSetLayout)
        {
            VkSampler sampler[1] = { g_FontSampler };
            VkDescriptorSetLayoutBinding binding[1] = {};
            binding[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding[0].descriptorCount = 1;
            binding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            binding[0].pImmutableSamplers = sampler;
            VkDescriptorSetLayoutCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            info.bindingCount = 1;
            info.pBindings = binding;
            err = vkCreateDescriptorSetLayout(v->Device, &info, v->Allocator, &m_descriptorSetLayout);
            check_vk_result(err);
        }

        // Create Descriptor Set:
        {
            VkDescriptorSetAllocateInfo alloc_info = {};
            alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            alloc_info.descriptorPool = v->DescriptorPool;
            alloc_info.descriptorSetCount = 1;
            alloc_info.pSetLayouts = &m_descriptorSetLayout;
            err = vkAllocateDescriptorSets(v->Device, &alloc_info, &m_descriptorSet);
            check_vk_result(err);
        }

        if (!m_pipelineLayout)
        {
            // Constants: we are using 'vec2 offset' and 'vec2 scale' instead of a full 3d projection matrix
            VkPushConstantRange push_constants[1] = {};
            push_constants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            push_constants[0].offset = sizeof(float) * 0;
            push_constants[0].size = sizeof(float) * 4;
            VkDescriptorSetLayout set_layout[1] = { m_descriptorSetLayout };
            VkPipelineLayoutCreateInfo layout_info = {};
            layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            layout_info.setLayoutCount = 1;
            layout_info.pSetLayouts = set_layout;
            layout_info.pushConstantRangeCount = 1;
            layout_info.pPushConstantRanges = push_constants;
            err = vkCreatePipelineLayout(v->Device, &layout_info, v->Allocator, &m_pipelineLayout);
            check_vk_result(err);
        }

        VkPipelineShaderStageCreateInfo stage[2] = {};
        stage[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stage[0].module = vert_module;
        stage[0].pName = "main";
        stage[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stage[1].module = frag_module;
        stage[1].pName = "main";

        VkVertexInputBindingDescription binding_desc[1] = {};
        binding_desc[0].stride = sizeof(ImDrawVert);
        binding_desc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attribute_desc[3] = {};
        attribute_desc[0].location = 0;
        attribute_desc[0].binding = binding_desc[0].binding;
        attribute_desc[0].format = VK_FORMAT_R32G32_SFLOAT;
        attribute_desc[0].offset = IM_OFFSETOF(ImDrawVert, pos);
        attribute_desc[1].location = 1;
        attribute_desc[1].binding = binding_desc[0].binding;
        attribute_desc[1].format = VK_FORMAT_R32G32_SFLOAT;
        attribute_desc[1].offset = IM_OFFSETOF(ImDrawVert, uv);
        attribute_desc[2].location = 2;
        attribute_desc[2].binding = binding_desc[0].binding;
        attribute_desc[2].format = VK_FORMAT_R8G8B8A8_UNORM;
        attribute_desc[2].offset = IM_OFFSETOF(ImDrawVert, col);

        VkPipelineVertexInputStateCreateInfo vertex_info = {};
        vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_info.vertexBindingDescriptionCount = 1;
        vertex_info.pVertexBindingDescriptions = binding_desc;
        vertex_info.vertexAttributeDescriptionCount = 3;
        vertex_info.pVertexAttributeDescriptions = attribute_desc;

        VkPipelineInputAssemblyStateCreateInfo ia_info = {};
        ia_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewport_info = {};
        viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_info.viewportCount = 1;
        viewport_info.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo raster_info = {};
        raster_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster_info.polygonMode = VK_POLYGON_MODE_FILL;
        raster_info.cullMode = VK_CULL_MODE_NONE;
        raster_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster_info.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo ms_info = {};
        ms_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        if (v->MSAASamples != 0)
            ms_info.rasterizationSamples = v->MSAASamples;
        else
            ms_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState color_attachment[1] = {};
        color_attachment[0].blendEnable = VK_TRUE;
        color_attachment[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_attachment[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_attachment[0].colorBlendOp = VK_BLEND_OP_ADD;
        color_attachment[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_attachment[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_attachment[0].alphaBlendOp = VK_BLEND_OP_ADD;
        color_attachment[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineDepthStencilStateCreateInfo depth_info = {};
        depth_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

        VkPipelineColorBlendStateCreateInfo blend_info = {};
        blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend_info.attachmentCount = 1;
        blend_info.pAttachments = color_attachment;

        VkDynamicState dynamic_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamic_state = {};
        dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = (uint32_t)sizeof(dynamic_states);
        dynamic_state.pDynamicStates = dynamic_states;

        VkGraphicsPipelineCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        info.flags = m_pipelineCreateFlags;
        info.stageCount = 2;
        info.pStages = stage;
        info.pVertexInputState = &vertex_info;
        info.pInputAssemblyState = &ia_info;
        info.pViewportState = &viewport_info;
        info.pRasterizationState = &raster_info;
        info.pMultisampleState = &ms_info;
        info.pDepthStencilState = &depth_info;
        info.pColorBlendState = &blend_info;
        info.pDynamicState = &dynamic_state;
        info.layout = m_pipelineLayout;
        info.renderPass = m_renderPass;
        err = vkCreateGraphicsPipelines(v->Device, v->PipelineCache, 1, &info, v->Allocator, &m_pipeline);
        check_vk_result(err);

        vkDestroyShaderModule(v->Device, vert_module, v->Allocator);
        vkDestroyShaderModule(v->Device, frag_module, v->Allocator);

        return true;
    }




}