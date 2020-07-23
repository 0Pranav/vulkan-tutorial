// Vulkan Tutorial.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_RADIANS
#define STB_IMAGE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#define GLM_ENABLE_EXPERIMENTAL

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include "stb/stb_image.h"
#include "tol/tiny_obj_loader.h"


#include <iostream>
#include <vector>
#include <optional>
#include <algorithm>
#include <fstream>
#include <set>
#include <array>
#include <chrono>

constexpr uint32_t WIDTH	= 800;
constexpr uint32_t HEIGHT	= 800;
constexpr int MAX_FRAMES_IN_FLIGHT = 2;
size_t currentFrame = 0;

const std::vector<const char*> g_ValidationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> g_DeviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool g_EnableValidationLayers = true;
#endif // NDEBUG

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;
	
	bool IsComplete() {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct SwapchainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 textureCoords;

	static VkVertexInputBindingDescription GetBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescription()
	{
		std::array<VkVertexInputAttributeDescription, 3> result{};

		VkVertexInputAttributeDescription positionAttribute{};
		positionAttribute.binding	= 0;
		positionAttribute.location	= 0;
		positionAttribute.format	= VK_FORMAT_R32G32B32_SFLOAT;
		positionAttribute.offset	= offsetof(Vertex, position);
		result[0] = positionAttribute;

		VkVertexInputAttributeDescription colorAttribute{};
		colorAttribute.binding	= 0;
		colorAttribute.location = 1;
		colorAttribute.format	= VK_FORMAT_R32G32B32_SFLOAT;
		colorAttribute.offset	= offsetof(Vertex, normal);
		result[1] = colorAttribute;

		VkVertexInputAttributeDescription textureCoordAttribute{};
		textureCoordAttribute.binding	= 0;
		textureCoordAttribute.location	= 2;
		textureCoordAttribute.format	= VK_FORMAT_R32G32_SFLOAT;
		textureCoordAttribute.offset	= offsetof(Vertex, textureCoords);
		result[2] = textureCoordAttribute;
		return result;
	}

	bool operator==(const Vertex& other) const {
		return position == other.position && normal == other.normal && textureCoords == other.textureCoords;
	}
};

struct Camera
{
	glm::vec3 position	= glm::vec3(2.0f, 2.0f, 2.0f);
	glm::vec3 front		= glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 up		= glm::vec3(0.0f, 1.0f, 0.0f);
	float speed			= 10.0;
	float yaw			= 0.0;
	float pitch			= 0.0;
};

namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.position) ^
				(hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.textureCoords) << 1);
		}
	};
}

struct UniformBufferObject
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 projection;
};

std::vector<Vertex> g_Vertices;
std::unordered_map<Vertex, uint32_t> g_UniqueVertices{};
std::vector<uint32_t> g_Indices;

const std::string MODEL_PATH = "models/backpack.obj";
const std::string TEXTURE_PATH = "textures/diffuse.jpg";



// Proxy function that finds the real CreateDebugUtilsMessengerEXT function 
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

// Proxy function that finds the real DestroyDebugUtilsMessengerEXT function 
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

class Application
{
public:
	void Run()
	{
		InitWindow();
		InitVulkan();
		MainLoop();
		Cleanup();
	}
private:

	void InitWindow()
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		//glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
			
		m_Window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", NULL, NULL);
		glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		
		glfwSetWindowUserPointer(m_Window, this);
		glfwSetCursorPosCallback(m_Window, MouseCallback);
		glfwSetFramebufferSizeCallback(m_Window, FramebufferResizeCallback);
	}

	static void MouseCallback(GLFWwindow* window, double x, double y)
	{
		auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		app->ProcessMouseMovement(x, y);
	}

	static void FramebufferResizeCallback(GLFWwindow* window, int width, int height)
	{
		auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		app->m_FramebufferResized = true;
	}

	void InitVulkan()
	{
		CreateInstance();
		SetupDebugMessenger();
		CreateSurface();
		PickPhysicalDevice();
		CreateLogicalDevice();
		CreateSwapchain();
		CreateImageViews();
		CreateRenderPass();
		CreateDescriptiorSetLayout();
		CreateGraphicsPipeline();
		CreateCommandPool();
		CreateColorResources();
		CreateDepthResources();
		CreateFrameBuffers();
		CreateTextureImage();
		CreateTextureImageView();
		CreateTextureSampler();
		LoadModel();
		CreateVertexBuffer();
		CreateIndexBuffer();
		CreateUniformBuffers();
		CreateDescriptorPool();
		CreateDescriptorSets();
		//CreateCommandBuffers();
		AllocateCommandBuffers();
		CreateSemaphores();
	}

	// Fill VkDebugUtilsMessengerCreateInfoEXT struct
	void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
	{
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = DebugCallbak;
		createInfo.pUserData = nullptr;
	}

	// Create VKInstance
	void CreateInstance()
	{
		if (g_EnableValidationLayers && !CheckValidationLayerSupport())
		{
			throw std::runtime_error("validation layer requested, but not available!");
		}

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "NONE";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		auto extensions = GetRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		if (g_EnableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(g_ValidationLayers.size());
			createInfo.ppEnabledLayerNames = g_ValidationLayers.data();
			PopulateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS) {
			throw std::runtime_error("failed to create instance!");
		}
	}

	// Create windows surface
	void CreateSurface()
	{
		if (glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface) != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface!");
		}
	}

	// Setup callback for debug layers
	void SetupDebugMessenger()
	{
		if (!g_EnableValidationLayers) return;
		VkDebugUtilsMessengerCreateInfoEXT createInfo{};
		PopulateDebugMessengerCreateInfo(createInfo);
		
		if (CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS) {
			throw std::runtime_error("Failed to set up debug messenger!");
		}
	}

	// Check validation layers against requireed g_ValidationLayers list
	bool CheckValidationLayerSupport()
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (auto& layerName : g_ValidationLayers)
		{
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers )
				if (strcmp(layerProperties.layerName, layerName) == 0)
				{
					layerFound = true;
					break;
				}
			
			if (!layerFound) return false;
		}
		return true;
	}

	// Set member variable m_Device to preferred device, based on IsDeviceSuitable
	void PickPhysicalDevice()
	{
		uint32_t physicalDeviceCount;
		vkEnumeratePhysicalDevices(m_Instance, &physicalDeviceCount, nullptr);

		if (physicalDeviceCount == 0)
		{
			throw std::runtime_error("No device with Vulkan support found");
		}

		std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
		vkEnumeratePhysicalDevices(m_Instance, &physicalDeviceCount, physicalDevices.data());

		for (const auto& device : physicalDevices)
		{
			if (IsDeviceSuitable(device))
			{
				m_PhysicalDevice = device;
				m_MsaaSamples = GetMaximumSamples();
				break;
			}
		}

		if (m_PhysicalDevice == VK_NULL_HANDLE)
		{
			throw std::runtime_error("No suitatble device found");
		}
	}

	// Return Queue families supported by device
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device)
	{
		QueueFamilyIndices indices;
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				indices.graphicsFamily = i;

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);

			if (presentSupport) {
				indices.presentFamily = i;
			}

			if (indices.IsComplete())
			{
				break;
			}
			i++;
		}

		return indices;
	}

	// Return capabilities, surface formats, present modes for device
	SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device) {
		SwapchainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface, &details.capabilities);
		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, nullptr);

		if (formatCount != 0) {
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, nullptr);

		if (presentModeCount != 0) {
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	/*
	Choose properties for creating swap chains
	*/

	VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		for (const auto& format : availableFormats)
		{
			if (format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && format.format == VK_FORMAT_B8G8R8A8_SRGB)
			{
				return format;
			}
		}
		
		return availableFormats[0];
	}

	VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& availableModes)
	{
		for (const auto& mode : availableModes)
		{
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
				return mode;
		}
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capability)
	{
		if (capability.currentExtent.width != UINT32_MAX)
		{
			return capability.currentExtent;
		}
		int width, height;
		glfwGetWindowSize(m_Window, &width, &height);
		VkExtent2D actualExtent = { 
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = std::max(capability.minImageExtent.width, std::min(capability.maxImageExtent.width, actualExtent.width));
		actualExtent.width = std::max(capability.minImageExtent.height, std::min(capability.maxImageExtent.width, actualExtent.width));

		return actualExtent;
	}

	// Report suitability of device
	bool IsDeviceSuitable(VkPhysicalDevice device) {
		VkPhysicalDeviceProperties prop;
		vkGetPhysicalDeviceProperties(device, &prop);
		VkPhysicalDeviceFeatures feat;
		vkGetPhysicalDeviceFeatures(device, &feat);
		QueueFamilyIndices indices = FindQueueFamilies(device);
		VkPhysicalDeviceFeatures supportedFeatures;
		vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

		bool extensionsSupported = CheckDeviceExtensionSupport(device);

		bool swapChainAdequate = false;
		if (extensionsSupported) {
			SwapchainSupportDetails swapChainSupport = QuerySwapchainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		return indices.IsComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
	}

	// Check for required device extension support
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device) {
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(g_DeviceExtensions.begin(), g_DeviceExtensions.end());

		for (const auto& extension : availableExtensions) {
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	// Get required instance extensions from glfw, add debug extension if in debug mode
	std::vector<const char*> GetRequiredExtensions()
	{
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;

		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions = std::vector(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (g_EnableValidationLayers)
		{
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}

	// Logical Device Creation
	void CreateLogicalDevice()
	{
		QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.samplerAnisotropy = VK_TRUE;
		deviceFeatures.sampleRateShading = VK_TRUE;

		VkDeviceCreateInfo deviceCreateInfo{};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
		deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());

		deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(g_DeviceExtensions.size());
		deviceCreateInfo.ppEnabledExtensionNames = g_DeviceExtensions.data();

		deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
		if (g_EnableValidationLayers) {
			deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(g_ValidationLayers.size());
			deviceCreateInfo.ppEnabledLayerNames = g_ValidationLayers.data();
		}
		else {
			deviceCreateInfo.enabledLayerCount = 0;
		}

		if (vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, nullptr, &m_Device) != VK_SUCCESS) {
			throw std::runtime_error("failed to create logical device!");
		}

		vkGetDeviceQueue(m_Device, indices.graphicsFamily.value(), 0, &m_GraphicsQueue);
		vkGetDeviceQueue(m_Device, indices.presentFamily.value(), 0, &m_PresentQueue);
	}

	void RecreateSwapchain()
	{
		int width = 0, height = 0;
		glfwGetFramebufferSize(m_Window, &width, &height);
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(m_Window, &width, &height);
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(m_Device);

		CleanupSwapchain();

		CreateSwapchain();
		CreateImageViews();
		CreateRenderPass();
		CreateGraphicsPipeline();
		CreateColorResources();
		CreateDepthResources();
		CreateFrameBuffers();
		CreateUniformBuffers();
		CreateDescriptorPool();
		CreateDescriptorSets();
		//CreateCommandBuffers();
		AllocateCommandBuffers();
	}

	void CreateSwapchain()
	{
		SwapchainSupportDetails swapChainDetails = QuerySwapchainSupport(m_PhysicalDevice);

		m_SurfaceFormat = ChooseSurfaceFormat(swapChainDetails.formats);
		m_SwapchainFormat = m_SurfaceFormat.format;
		VkPresentModeKHR presentMode = ChoosePresentMode(swapChainDetails.presentModes);
		m_Extent = ChooseSwapExtent(swapChainDetails.capabilities);

		uint32_t imageCount = swapChainDetails.capabilities.minImageCount + 1;

		if (swapChainDetails.capabilities.maxImageCount > 0 and swapChainDetails.capabilities.maxImageCount < imageCount)
			imageCount = swapChainDetails.capabilities.maxImageCount;

		VkSwapchainCreateInfoKHR swapchainCreateInfo{};
		swapchainCreateInfo.sType			= VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchainCreateInfo.surface			= m_Surface;
		swapchainCreateInfo.minImageCount	= imageCount;
		swapchainCreateInfo.imageFormat		= m_SurfaceFormat.format;
		swapchainCreateInfo.imageColorSpace = m_SurfaceFormat.colorSpace;
		swapchainCreateInfo.presentMode		= presentMode;
		swapchainCreateInfo.imageExtent		= m_Extent;

		swapchainCreateInfo.imageArrayLayers = 1;
		swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);
		uint32_t qFamilyIndices[] = { indices.graphicsFamily.value() , indices.presentFamily.value() };

		if (indices.presentFamily != indices.graphicsFamily)
		{
			swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT ;
			swapchainCreateInfo.queueFamilyIndexCount = 2;
			swapchainCreateInfo.pQueueFamilyIndices = qFamilyIndices;
		}
		else
		{
			swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		swapchainCreateInfo.preTransform = swapChainDetails.capabilities.currentTransform;
		swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

		swapchainCreateInfo.clipped = VK_TRUE;

		swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(m_Device, &swapchainCreateInfo, nullptr, &m_Swapchain) != VK_SUCCESS)
		{
			throw std::runtime_error("Swapchain Creation failed");
		}

		vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr);
		m_SwapchainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, m_SwapchainImages.data());
	}

	void CreateImageViews()
	{
		m_SwapchainImageViews.resize(m_SwapchainImages.size());

		for (size_t i = 0; i < m_SwapchainImageViews.size(); i++)
		{
			m_SwapchainImageViews[i] = CreateImageView(m_SwapchainImages[i], VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, 1);
		}
	}

	void CreateRenderPass()
	{
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format			= m_SwapchainFormat;
		colorAttachment.samples			= m_MsaaSamples;

		colorAttachment.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;

		colorAttachment.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription depthAttachment{};
		depthAttachment.format	= findDepthFormat();
		depthAttachment.samples = m_MsaaSamples;

		depthAttachment.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		depthAttachment.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp			= VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;

		VkAttachmentDescription colorAttachmentResolve{};
		colorAttachmentResolve.format = m_SwapchainFormat;
		colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;


		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment	= 0;
		colorAttachmentRef.layout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		
		VkAttachmentReference depthAttachmentRef{};
		depthAttachmentRef.attachment	= 1;
		depthAttachmentRef.layout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorAttachmentResolveRef{};
		colorAttachmentResolveRef.attachment = 2;
		colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
		
		subpass.colorAttachmentCount	= 1;
		subpass.pColorAttachments		= &colorAttachmentRef;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;
		subpass.pResolveAttachments		= &colorAttachmentResolveRef;

		VkSubpassDependency dependency{};
		dependency.srcSubpass		= VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass		= 0;
		dependency.srcStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask	= 0;
		dependency.dstStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		
		std::array<VkAttachmentDescription, 3> attachments = { colorAttachment, depthAttachment, colorAttachmentResolve };
		VkRenderPassCreateInfo renderPass{};
		renderPass.sType			= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPass.attachmentCount	= static_cast<uint32_t>(attachments.size());
		renderPass.pAttachments		= attachments.data();
		renderPass.subpassCount		= 1;
		renderPass.pSubpasses		= &subpass;
		renderPass.dependencyCount	= 1;
		renderPass.pDependencies	= &dependency;

		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
		clearValues[1].depthStencil = { 1.0f, 0 };


		if (vkCreateRenderPass(m_Device, &renderPass, nullptr, &m_RenderPass) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create renderpass");
		}
	}

	void CreateDescriptiorSetLayout()
	{
		VkDescriptorSetLayoutBinding uboLayoutBinding{};
		uboLayoutBinding.binding			= 0;
		uboLayoutBinding.descriptorType		= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount	= 1;
		uboLayoutBinding.stageFlags			= VK_SHADER_STAGE_VERTEX_BIT;
		uboLayoutBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutBinding samplerBinding{};
		samplerBinding.binding				= 1;
		samplerBinding.descriptorCount		= 1;
		samplerBinding.descriptorType		= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerBinding.stageFlags			= VK_SHADER_STAGE_FRAGMENT_BIT;
		samplerBinding.pImmutableSamplers	= nullptr;

		std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerBinding };

		VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
		layoutCreateInfo.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutCreateInfo.bindingCount	= static_cast<uint32_t>(bindings.size());
		layoutCreateInfo.pBindings		= bindings.data();
		
		if (vkCreateDescriptorSetLayout(m_Device, &layoutCreateInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create descriptor set layout.");
		}

	}

	void CreateGraphicsPipeline()
	{
		auto vertShaderCode = ReadFile("shaders/vert.spv");
		auto fragShaderCode = ReadFile("shaders/frag.spv");
		
		VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
		VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage	= VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module	= vertShaderModule;
		vertShaderStageInfo.pName	= "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage	= VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module	= fragShaderModule;
		fragShaderStageInfo.pName	= "main";

		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

		auto bindingInfo	= Vertex::GetBindingDescription();
		auto attributeInfo	= Vertex::GetAttributeDescription();
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount	= 1;
		vertexInputInfo.pVertexBindingDescriptions		= &bindingInfo;
		vertexInputInfo.vertexAttributeDescriptionCount = attributeInfo.size();
		vertexInputInfo.pVertexAttributeDescriptions	= attributeInfo.data(); 

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport{};
		viewport.x			= 0.0f;
		viewport.y			= 0.0f;
		viewport.width		= (float)m_Extent.width;
		viewport.height		= (float)m_Extent.height;
		viewport.minDepth	= 0.0f;
		viewport.maxDepth	= 1.0f;

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = m_Extent;
		
		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType			= VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports	= &viewport;
		viewportState.scissorCount	= 1;
		viewportState.pScissors		= &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType =						VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable =			VK_FALSE; 
		rasterizer.rasterizerDiscardEnable =	VK_FALSE;
		rasterizer.polygonMode =				VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth =					1.0f;
		rasterizer.cullMode =					VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace =					VK_FRONT_FACE_COUNTER_CLOCKWISE;

		rasterizer.depthBiasEnable =			VK_FALSE;
		rasterizer.depthBiasConstantFactor =	0.0f; // Optional
		rasterizer.depthBiasClamp =				0.0f; // Optional
		rasterizer.depthBiasSlopeFactor =		0.0f; // Optional


		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType =					VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable =		VK_TRUE;
		multisampling.rasterizationSamples =	m_MsaaSamples;
		multisampling.minSampleShading =		1.0f; // Optional
		multisampling.pSampleMask =				nullptr; // Optional
		multisampling.alphaToCoverageEnable =	VK_FALSE; // Optional
		multisampling.alphaToOneEnable =		VK_FALSE; // Optional

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask =		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable =			VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor =	VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstColorBlendFactor =	VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.colorBlendOp =			VK_BLEND_OP_ADD; // Optional
		colorBlendAttachment.srcAlphaBlendFactor =	VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstAlphaBlendFactor =	VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.alphaBlendOp =			VK_BLEND_OP_ADD; // Optional

		colorBlendAttachment.blendEnable =			VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor =	VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor =	VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp =			VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor =	VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor =	VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp =			VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType =				VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable =		VK_FALSE;
		colorBlending.logicOp =				VK_LOGIC_OP_COPY; // Optional
		colorBlending.attachmentCount =		1;
		colorBlending.pAttachments =		&colorBlendAttachment;
		colorBlending.blendConstants[0] =	0.0f; // Optional
		colorBlending.blendConstants[1] =	0.0f; // Optional
		colorBlending.blendConstants[2] =	0.0f; // Optional
		colorBlending.blendConstants[3] =	0.0f; // Optional

		VkDynamicState dynamicStates[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_LINE_WIDTH
		};

		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType =				VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount =	2;
		dynamicState.pDynamicStates =		dynamicStates;

		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.minDepthBounds = 0.0f; // Optional
		depthStencil.maxDepthBounds = 1.0f; // Optional
		depthStencil.stencilTestEnable = VK_FALSE;
		depthStencil.front = {}; // Optional
		depthStencil.back= {}; // Optional

		VkPushConstantRange vpcr{};
		vpcr.offset = 0;
		vpcr.size = sizeof(glm::vec4);
		vpcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType					= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount			= 1; // Optional
		pipelineLayoutInfo.pSetLayouts				= &m_DescriptorSetLayout; // Optional
		pipelineLayoutInfo.pushConstantRangeCount	= 0; // Optional
		pipelineLayoutInfo.pPushConstantRanges		= nullptr; // Optional

		if (vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create pipeline layout!");
		}

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;

		pipelineInfo.pVertexInputState =	&vertexInputInfo;
		pipelineInfo.pInputAssemblyState =	&inputAssembly;
		pipelineInfo.pViewportState =		&viewportState;
		pipelineInfo.pRasterizationState =	&rasterizer;
		pipelineInfo.pMultisampleState =	&multisampling;
		pipelineInfo.pDepthStencilState =	&depthStencil; // Optional
		pipelineInfo.pColorBlendState =		&colorBlending;
		pipelineInfo.pDynamicState =		nullptr; // Optional

		pipelineInfo.layout =				m_PipelineLayout;

		pipelineInfo.renderPass =			m_RenderPass;
		pipelineInfo.subpass =				0;

		// Used for derived pipelines
		pipelineInfo.basePipelineHandle =	VK_NULL_HANDLE; // Optional
		pipelineInfo.basePipelineIndex =	-1; // Optional

		if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create Graphics Pipeline");
		}
	}

	void CreateFrameBuffers()
	{
		m_SwapchainFramebuffers.resize(m_SwapchainImageViews.size());
		
		for (size_t i = 0; i < m_SwapchainImageViews.size(); i++) {
			std::array<VkImageView, 3> attachments = {
				m_ColorImageView,
				m_DepthImageView,
				m_SwapchainImageViews[i]
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType =				VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass =		m_RenderPass;
			framebufferInfo.attachmentCount =	static_cast<uint32_t>(attachments.size());
			framebufferInfo.pAttachments =		attachments.data();
			framebufferInfo.width =				m_Extent.width;
			framebufferInfo.height =			m_Extent.height;
			framebufferInfo.layers =			1;

			if (vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr, &m_SwapchainFramebuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create framebuffer!");
			}
		}
	}

	void CreateCommandPool()
	{
		QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(m_PhysicalDevice);

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create command pool!");
		}

	}

	void CreateColorResources()
	{
		VkFormat colorFormat = m_SwapchainFormat;
		CreateImage(m_Extent.width, m_Extent.height, 1, m_MsaaSamples, colorFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_ColorImage, m_ColorImageMemory);
		m_ColorImageView = CreateImageView(m_ColorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
	}

	void CreateDepthResources()
	{
		VkFormat depthFormat = findDepthFormat();
		CreateImage(m_Extent.width, m_Extent.height, 1, m_MsaaSamples, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_DepthImage, m_DepthImageMemory);
		m_DepthImageView = CreateImageView(m_DepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
		TransitionImageLayout(m_DepthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
	}

	bool HasStencilComponent(VkFormat format) {
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	VkFormat findDepthFormat() {
		return FindSupportedFormat(
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}

	VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
		for (VkFormat format : candidates) {
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, format, &props);

			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
				return format;
			}
			else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
				return format;
			}
		}

		throw std::runtime_error("failed to find supported format!");
	}

	void CreateTextureImage()
	{
		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		VkDeviceSize imageSize = static_cast<uint64_t>(texWidth) * static_cast<uint64_t>(texHeight) * 4; // casting to uint64_t to make sure there's no loss of data during multiplication

		m_MipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texHeight, texWidth))));

		if (!pixels) {
			std::cout << stbi_failure_reason() << std::endl;
			throw std::runtime_error("failed to load texture image!");
		}

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(m_Device, stagingBufferMemory, 0, imageSize, 0, &data);
		memcpy(data, pixels, static_cast<size_t>(imageSize));
		vkUnmapMemory(m_Device, stagingBufferMemory);

		stbi_image_free(pixels);

		CreateImage(texWidth, texHeight, m_MipLevels, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT| VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_TextureImage, m_TextureImageMemory);

		TransitionImageLayout(m_TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_MipLevels);
		CopyBufferToImage(stagingBuffer, m_TextureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		GenerateMipmaps(m_TextureImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, m_MipLevels);

		vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
		vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
	}

	void GenerateMipmaps(VkImage image,VkFormat imageFormat, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels)
	{
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, imageFormat, &formatProperties);
		if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
			throw std::runtime_error("texture image format does not support linear blitting!");
		}

		VkCommandBuffer buffer = BeginSingleTimeCommand();

		VkImageMemoryBarrier barrier{};
		barrier.sType							= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image							= image;
		barrier.srcQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount		= 1;
		barrier.subresourceRange.levelCount		= 1;

		int32_t mipWidth = texWidth;
		int32_t mipHeight = texHeight;

		for (uint32_t i = 1; i < mipLevels; i++) {
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout		= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout		= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask	= VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask	= VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(buffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			VkImageBlit blit{};
			blit.srcOffsets[0]					= { 0, 0, 0 };
			blit.srcOffsets[1]					= { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.baseArrayLayer	= 0;
			blit.srcSubresource.layerCount		= 1;
			blit.srcSubresource.mipLevel		= i - 1;

			blit.dstOffsets[0]					= { 0, 0, 0 };
			blit.dstOffsets[1]					= { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.baseArrayLayer	= 0;
			blit.dstSubresource.layerCount		= 1;
			blit.dstSubresource.mipLevel		= i;

			vkCmdBlitImage(buffer,
				image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(buffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		barrier.subresourceRange.baseMipLevel = mipLevels - 1;
		barrier.oldLayout		= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout		= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask	= VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask	= VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(buffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		EndSingleTimeCommand(buffer);
	}


	void CreateTextureImageView()
	{
		m_TextureImageView = CreateImageView(m_TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, m_MipLevels);
	}

	void CreateTextureSampler()
	{
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType					= VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter				= VK_FILTER_LINEAR;
		samplerInfo.minFilter				= VK_FILTER_LINEAR;
		samplerInfo.addressModeU			= VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV			= VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW			= VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.borderColor				= VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		samplerInfo.anisotropyEnable		= VK_TRUE;
		samplerInfo.maxAnisotropy			= 16.0f;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable			= VK_FALSE;
		samplerInfo.compareOp				= VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode				= VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias				= 0.0f;
		samplerInfo.minLod					= 0.0f;
		samplerInfo.maxLod					= static_cast<float>(m_MipLevels);

		if (vkCreateSampler(m_Device, &samplerInfo, nullptr, &m_TextureSampler) != VK_SUCCESS)
		{
			throw std::runtime_error("Could not create texture sampler");
		}

	}
	VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect, uint32_t mipLevels)
	{
		VkImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.sType							= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image							= image;
		imageViewCreateInfo.viewType						= VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format							= format;
		imageViewCreateInfo.subresourceRange.aspectMask		= aspect;
		imageViewCreateInfo.subresourceRange.baseMipLevel	= 0;
		imageViewCreateInfo.subresourceRange.levelCount		= mipLevels;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount		= 1;

		VkImageView imageView;
		if (vkCreateImageView(m_Device, &imageViewCreateInfo, nullptr, &imageView) != VK_SUCCESS) {
			throw std::runtime_error("failed to create texture image view!");
		}

		return imageView;
	}

	void CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
	{
		VkImageCreateInfo imageInfo{};
		imageInfo.sType			= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType		= VK_IMAGE_TYPE_2D;
		imageInfo.mipLevels		= mipLevels;
		imageInfo.arrayLayers	= 1;
		imageInfo.extent.height = height;
		imageInfo.extent.width	= width;
		imageInfo.extent.depth	= 1;
		imageInfo.format		= format;
		imageInfo.tiling		= tiling;
		imageInfo.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.usage			= usage;
		imageInfo.samples		= numSamples;
		imageInfo.flags			= 0;

		if (vkCreateImage(m_Device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image!");
		}

		VkMemoryRequirements memReq;
		vkGetImageMemoryRequirements(m_Device, image, &memReq);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memReq.size;
		allocInfo.memoryTypeIndex = FindMemoryType(memReq.memoryTypeBits, properties);

		if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate image memory!");
		}

		vkBindImageMemory(m_Device, image, imageMemory, 0);
	}

	void LoadModel()
	{
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn, err;

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str())) {
			throw std::runtime_error(warn + err);
		}

		for (const auto& shape : shapes) {
			for (const auto& index : shape.mesh.indices) {
				Vertex vertex{};

				vertex.position = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};

				vertex.textureCoords = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					attrib.texcoords[2 * index.texcoord_index + 1]
				};

				vertex.normal = { 
					attrib.normals[3 * index.normal_index + 0],
					attrib.normals[3 * index.normal_index + 1],
					attrib.normals[3 * index.normal_index + 2],
				};

				if (g_UniqueVertices.count(vertex) == 0) {
					g_UniqueVertices[vertex] = static_cast<uint32_t>(g_Vertices.size());
					g_Vertices.push_back(vertex);
				}

				g_Indices.push_back(g_UniqueVertices[vertex]);
			}
		}
	}

	void CreateVertexBuffer()
	{
		VkDeviceSize bufferSize = sizeof(g_Vertices[0]) * g_Vertices.size();

		VkBuffer		stagingBuffer;
		VkDeviceMemory	stagingBufferMemory;

		CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &data);
			memcpy(data, g_Vertices.data(), (size_t)bufferSize);
		vkUnmapMemory(m_Device, stagingBufferMemory);

		CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_VertexBuffer, m_VertexBufferMemory);

		CopyBuffer(stagingBuffer, m_VertexBuffer, bufferSize);

		vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
		vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
	}

	void CreateIndexBuffer()
	{
		VkDeviceSize bufferSize = sizeof(g_Indices[0]) * g_Indices.size();

		VkBuffer		stagingBuffer;
		VkDeviceMemory	stagingBufferMemory;

		CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, g_Indices.data(), (size_t)bufferSize);
		vkUnmapMemory(m_Device, stagingBufferMemory);
		
		CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_IndexBuffer, m_IndexBufferMemory);
		CopyBuffer(stagingBuffer, m_IndexBuffer, bufferSize);
		vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
		vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
	}

	void CreateUniformBuffers()
	{
		VkDeviceSize bufferSize = sizeof(UniformBufferObject);
		
		m_UniformBuffers.resize(m_SwapchainImages.size());
		m_UniformBufferMemories.resize(m_SwapchainImages.size());

		for (size_t i = 0; i < m_SwapchainImages.size(); i++)
		{
			CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, m_UniformBuffers[i], m_UniformBufferMemories[i]);

		}
	}

	void CreateDescriptorPool()
	{
		std::array<VkDescriptorPoolSize, 2> poolSizes{};
		poolSizes[0].descriptorCount	= static_cast<uint32_t>(m_SwapchainImages.size());
		poolSizes[0].type				= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[1].descriptorCount	= static_cast<uint32_t>(m_SwapchainImages.size());
		poolSizes[1].type				= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount	= static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes		= poolSizes.data();
		poolInfo.maxSets		= static_cast<uint32_t>(m_SwapchainImages.size());

		if (vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &m_DescriptorPool))
		{
			throw std::runtime_error("Failed to create descriptor pool");
		}
	}

	void CreateDescriptorSets()
	{
		std::vector<VkDescriptorSetLayout> layouts(m_SwapchainImages.size(), m_DescriptorSetLayout);
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_DescriptorPool;
		allocInfo.descriptorSetCount = static_cast<uint32_t>(m_SwapchainImages.size());
		allocInfo.pSetLayouts = layouts.data();

		m_DescriptorSets.resize(m_SwapchainImages.size());
		if (vkAllocateDescriptorSets(m_Device, &allocInfo, m_DescriptorSets.data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate descriptor sets!");
		}
		for (size_t i = 0; i < m_SwapchainImages.size(); i++)
		{
			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = m_UniformBuffers[i];
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(UniformBufferObject);

			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = m_TextureImageView;
			imageInfo.sampler = m_TextureSampler;

			std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
			descriptorWrites[0].sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet				= m_DescriptorSets[i];
			descriptorWrites[0].dstBinding			= 0;
			descriptorWrites[0].dstArrayElement		= 0;
			descriptorWrites[0].descriptorType		= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrites[0].descriptorCount		= 1;
			descriptorWrites[0].pBufferInfo			= &bufferInfo;

			descriptorWrites[1].sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[1].dstSet				= m_DescriptorSets[i];
			descriptorWrites[1].dstBinding			= 1;
			descriptorWrites[1].dstArrayElement		= 0;
			descriptorWrites[1].descriptorType		= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[1].descriptorCount		= 1;
			descriptorWrites[1].pImageInfo			= &imageInfo;

			vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
		}
	}

	VkCommandBuffer BeginSingleTimeCommand()
	{
		VkCommandBufferAllocateInfo commandInfo{};
		commandInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandInfo.commandPool = m_CommandPool;
		commandInfo.commandBufferCount = 1;

		VkCommandBuffer copyCommandBuffer;
		if (vkAllocateCommandBuffers(m_Device, &commandInfo, &copyCommandBuffer) != VK_SUCCESS) throw std::runtime_error("Copy command buffer could not be allocated");

		VkCommandBufferBeginInfo copyBufferBegin{};
		copyBufferBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		copyBufferBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(copyCommandBuffer, &copyBufferBegin);
		
		return copyCommandBuffer;
	}

	void EndSingleTimeCommand(VkCommandBuffer& buffer)
	{

		vkEndCommandBuffer(buffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &buffer;

		vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(m_GraphicsQueue);
		vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &buffer);
	}

	void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels) {
		VkCommandBuffer commandBuffer = BeginSingleTimeCommand();
		VkImageMemoryBarrier barrier{};

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;


		if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) 
		{
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

			if (HasStencilComponent(format)) 
			{
				barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
			}
		}
		else 
		{
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}
		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) 
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage			= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage	= VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) 
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) 
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		}
		else {
			throw std::invalid_argument("unsupported layout transition!");
		}
		barrier.sType							= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout						= oldLayout;
		barrier.newLayout						= newLayout;
		barrier.image							= image;
		barrier.subresourceRange.baseArrayLayer	= 0;
		barrier.subresourceRange.baseMipLevel	= 0;
		barrier.subresourceRange.layerCount		= 1;
		barrier.subresourceRange.levelCount		= mipLevels;

		barrier.srcQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;

		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);


		EndSingleTimeCommand(commandBuffer);
	}

	void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
	{
		VkCommandBuffer commandBuffer = BeginSingleTimeCommand();

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = {
			width,
			height,
			1
		};

		vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		EndSingleTimeCommand(commandBuffer);
	}

	void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) 
	{
		VkCommandBuffer copyCommandBuffer = BeginSingleTimeCommand();

		VkBufferCopy copyRegion{};
		copyRegion.size = size;

		vkCmdCopyBuffer(copyCommandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
		EndSingleTimeCommand(copyCommandBuffer);
	}

	void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory)
	{

		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType		= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.pNext		= nullptr;
		bufferInfo.size			= size;
		bufferInfo.usage		= usage;
		bufferInfo.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(m_Device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
		{
			throw std::exception("failed to create vertex buffer");
		}

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(m_Device, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType				= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize	= memRequirements.size;
		allocInfo.memoryTypeIndex	= FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate vertex buffer memory!");
		}

		vkBindBufferMemory(m_Device, buffer, memory, 0);
	}

	uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags flags)
	{
		VkPhysicalDeviceMemoryProperties memProp;
		vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProp);

		for (uint32_t i = 0; i < memProp.memoryTypeCount; i++)
		{
			if (typeFilter & (1 << i) && (memProp.memoryTypes[i].propertyFlags & flags) == flags)
			{
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	}

	void AllocateCommandBuffers()
	{
		m_CommandBuffers.resize(m_SwapchainImages.size());
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = m_CommandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = m_SwapchainImages.size();

		if (vkAllocateCommandBuffers(m_Device, &allocInfo, m_CommandBuffers.data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate command buffers!");
		}
	}

	void RecordCommandBuffers(int currentImage)
	{
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType				= VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags				= 0; // Optional
			beginInfo.pInheritanceInfo	= nullptr; // Optional

			if (vkBeginCommandBuffer(m_CommandBuffers[currentImage], &beginInfo) != VK_SUCCESS) {
				throw std::runtime_error("failed to begin recording command buffer!");
			}

			VkRenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType		= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass	= m_RenderPass;
			renderPassInfo.framebuffer	= m_SwapchainFramebuffers[currentImage];

			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = m_Extent;

			std::array<VkClearValue, 2> clearValues{};
			clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
			clearValues[1].depthStencil = { 1.0f, 0 };

			renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
			renderPassInfo.pClearValues = clearValues.data();

			vkCmdBeginRenderPass(m_CommandBuffers[currentImage], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(m_CommandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);

			VkBuffer buffers[]		= { m_VertexBuffer };
			VkDeviceSize offsets[]	= { 0 };
			vkCmdBindVertexBuffers(m_CommandBuffers[currentImage], 0, 1, buffers, offsets);

			vkCmdBindIndexBuffer(m_CommandBuffers[currentImage], m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);

			vkCmdBindDescriptorSets(m_CommandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_DescriptorSets[currentImage], 0, nullptr);

			vkCmdDrawIndexed(m_CommandBuffers[currentImage], static_cast<uint32_t>(g_Indices.size()), 1, 0, 0, 0);

			vkCmdEndRenderPass(m_CommandBuffers[currentImage]);
			if (vkEndCommandBuffer(m_CommandBuffers[currentImage]) != VK_SUCCESS) {
				throw std::runtime_error("failed to record command buffer!");
			}
	}

	void CreateSemaphores()
	{
		m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
		m_ImagesInFlight.resize(m_SwapchainImages.size(), VK_NULL_HANDLE);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			if (vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS) {

				throw std::runtime_error("failed to create semaphores for a frame!");
			}
		}
	}

	VkShaderModule CreateShaderModule(const std::vector<char>& code) {
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType	= VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode	= reinterpret_cast<const uint32_t*>(code.data());

		VkShaderModule shaderModule;
		if (vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
			throw std::runtime_error("failed to create shader module!");
		}

		return shaderModule;
	}

	static std::vector<char> ReadFile(const std::string& filename) {
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			throw std::runtime_error("failed to open file!");
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();

		return buffer;
	}

	void MainLoop()
	{

		while (!glfwWindowShouldClose(m_Window))
		{
			glfwPollEvents();
			ProcessInput();
			DrawFrame();
		}
		vkDeviceWaitIdle(m_Device);
	}
	
	void ProcessInput()
	{
		float currentFrame = glfwGetTime();
		m_DeltaTime = currentFrame - m_LastFrame;
		m_LastFrame = currentFrame;
		if (glfwGetKey(m_Window, GLFW_KEY_W) == GLFW_PRESS)
			m_Camera.position += (m_DeltaTime * m_Camera.speed * m_Camera.front);
		if (glfwGetKey(m_Window, GLFW_KEY_S) == GLFW_PRESS)
			m_Camera.position -= (m_DeltaTime * m_Camera.speed * m_Camera.front);
		if (glfwGetKey(m_Window, GLFW_KEY_A) == GLFW_PRESS)
			m_Camera.position -= m_DeltaTime * m_Camera.speed * glm::normalize(glm::cross(m_Camera.front, m_Camera.up));
		if (glfwGetKey(m_Window, GLFW_KEY_D) == GLFW_PRESS)
			m_Camera.position += m_DeltaTime * m_Camera.speed * glm::normalize(glm::cross(m_Camera.front, m_Camera.up));
	}

	void ProcessMouseMovement(double xpos, double ypos)
	{
		if (m_FirstMouse)
		{
			m_LastX = xpos;
			m_LastY = ypos;
			m_FirstMouse = false;
		}

		float xoffset = xpos - m_LastX;
		float yoffset = m_LastY - ypos;
		m_LastX = xpos;
		m_LastY = ypos;

		float sensitivity = 0.1f;
		xoffset *= sensitivity;
		yoffset *= sensitivity;

		m_Camera.yaw += xoffset;
		m_Camera.pitch += yoffset;

		if (m_Camera.pitch > 89.0f)
			m_Camera.pitch = 89.0f;
		if (m_Camera.pitch < -89.0f)
			m_Camera.pitch = -89.0f;

		glm::vec3 direction;
		direction.x = cos(glm::radians(m_Camera.yaw)) * cos(glm::radians(m_Camera.pitch));
		direction.y = sin(glm::radians(m_Camera.pitch));
		direction.z = sin(glm::radians(m_Camera.yaw)) * cos(glm::radians(m_Camera.pitch));
		m_Camera.front = glm::normalize(direction);
	}

	void DrawFrame()
	{
		vkWaitForFences(m_Device, 1, &m_InFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
		uint32_t imageIndex;

		VkResult result = vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, m_ImageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			RecreateSwapchain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		if (m_ImagesInFlight[imageIndex] != VK_NULL_HANDLE) {
			vkWaitForFences(m_Device, 1, &m_ImagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
		}
		// Mark the image as now being in use by this frame
		m_ImagesInFlight[imageIndex] = m_InFlightFences[currentFrame];

		UpdateUniformBuffers(imageIndex);
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[]	= { m_ImageAvailableSemaphores[currentFrame] };
		VkSemaphore signalSemaphores[]	= { m_RenderFinishedSemaphores[currentFrame] };
		RecordCommandBuffers(imageIndex);
		VkPipelineStageFlags waitStages[]	= { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount		= 1;
		submitInfo.pWaitSemaphores			= waitSemaphores;
		submitInfo.pWaitDstStageMask		= waitStages;
		submitInfo.commandBufferCount		= 1;
		submitInfo.pCommandBuffers			= &m_CommandBuffers[imageIndex];
		submitInfo.signalSemaphoreCount		= 1;
		submitInfo.pSignalSemaphores		= signalSemaphores;

		vkResetFences(m_Device, 1, &m_InFlightFences[currentFrame]);
		if (vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_InFlightFences[currentFrame]) != VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		VkSwapchainKHR swapChains[] = { m_Swapchain };
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount	= 1;
		presentInfo.pWaitSemaphores		= signalSemaphores;
		presentInfo.swapchainCount		= 1;
		presentInfo.pSwapchains			= swapChains;
		presentInfo.pImageIndices		= &imageIndex;
		presentInfo.pResults			= nullptr; 

		result = vkQueuePresentKHR(m_PresentQueue, &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR ||
			result == VK_SUBOPTIMAL_KHR ||
			m_FramebufferResized) 
		{
			m_FramebufferResized = false;
			RecreateSwapchain();
		}
		else if (result != VK_SUCCESS) 
		{
			throw std::runtime_error("failed to present swap chain image!");
		}
		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}
	
	void UpdateUniformBuffers(uint32_t currentImage)
	{
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		UniformBufferObject ubo{};
		ubo.model = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
		glm::vec3 lookAt = m_Camera.front + m_Camera.position;
		ubo.view = glm::lookAt(m_Camera.position,  lookAt, m_Camera.up);
		std::cout << "Look at" << lookAt.x << "," << lookAt.y << "," << lookAt.z << std::endl;
		ubo.projection = glm::perspective(glm::radians(45.0f), m_Extent.width / (float) m_Extent.height, 0.1f, 100.0f);
		ubo.projection[1][1] *= -1;

		void* data;
		vkMapMemory(m_Device, m_UniformBufferMemories[currentImage], 0, sizeof(ubo), 0, &data);
		memcpy(data, &ubo, sizeof(ubo));
		vkUnmapMemory(m_Device, m_UniformBufferMemories[currentImage]);
	}

	void CleanupSwapchain()
	{

		vkDestroyImageView(m_Device, m_ColorImageView, nullptr);
		vkDestroyImage(m_Device, m_ColorImage, nullptr);
		vkFreeMemory(m_Device, m_ColorImageMemory, nullptr);

		vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
		vkDestroyImage(m_Device, m_DepthImage, nullptr);
		vkFreeMemory(m_Device, m_DepthImageMemory, nullptr);

		for (auto framebuffer : m_SwapchainFramebuffers) 
		{
			vkDestroyFramebuffer(m_Device, framebuffer, nullptr);
		}

		vkFreeCommandBuffers(m_Device, m_CommandPool, static_cast<uint32_t>(m_CommandBuffers.size()), m_CommandBuffers.data());

		vkDestroyPipeline(m_Device, m_Pipeline, nullptr);
		vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
		vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);

		for (auto imageView : m_SwapchainImageViews) {
			vkDestroyImageView(m_Device, imageView, nullptr);
		}

		vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);

		for (size_t i = 0; i < m_SwapchainImages.size(); i++) {
			vkDestroyBuffer(m_Device, m_UniformBuffers[i], nullptr);
			vkFreeMemory(m_Device, m_UniformBufferMemories[i], nullptr);
		}
		
		vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
	}


	void Cleanup()
	{
		CleanupSwapchain();

		vkDestroySampler(m_Device, m_TextureSampler, nullptr);
		vkDestroyImageView(m_Device, m_TextureImageView, nullptr);
		vkDestroyImage(m_Device, m_TextureImage, nullptr);
		vkFreeMemory(m_Device, m_TextureImageMemory, nullptr);
		vkDestroyDescriptorSetLayout(m_Device, m_DescriptorSetLayout, nullptr);
		vkDestroyBuffer(m_Device, m_VertexBuffer, nullptr);
		vkFreeMemory(m_Device, m_VertexBufferMemory, nullptr);
		vkDestroyBuffer(m_Device, m_IndexBuffer, nullptr);
		vkFreeMemory(m_Device, m_IndexBufferMemory, nullptr);


		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
		{
			vkDestroySemaphore(m_Device, m_RenderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(m_Device, m_ImageAvailableSemaphores[i], nullptr);
			vkDestroyFence(m_Device, m_InFlightFences[i], nullptr);
		}

		vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);

		vkDestroyDevice(m_Device, NULL);

		if (g_EnableValidationLayers)
		{
			DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
		}

		vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
		vkDestroyInstance(m_Instance, NULL);

		glfwDestroyWindow(m_Window);
		glfwTerminate();
	}



	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallbak(
		VkDebugUtilsMessageSeverityFlagBitsEXT		messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT				messageTypes,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData
	)
	{
		std::cerr << "Validation Layer: " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
	}

	VkSampleCountFlagBits GetMaximumSamples()
	{
		VkPhysicalDeviceProperties prop;
		vkGetPhysicalDeviceProperties(m_PhysicalDevice, &prop);
		auto count = prop.limits.framebufferColorSampleCounts & prop.limits.framebufferDepthSampleCounts;
		if (count & VK_SAMPLE_COUNT_64_BIT) return VK_SAMPLE_COUNT_64_BIT;
		if (count & VK_SAMPLE_COUNT_32_BIT) return VK_SAMPLE_COUNT_32_BIT;
		if (count & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
		if (count & VK_SAMPLE_COUNT_8_BIT) return VK_SAMPLE_COUNT_8_BIT;
		if (count & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
		if (count & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;
		return VK_SAMPLE_COUNT_1_BIT;
	}

private:
	GLFWwindow*						m_Window;
	
	VkInstance						m_Instance;
	
	VkPhysicalDevice				m_PhysicalDevice = VK_NULL_HANDLE;
	VkDevice						m_Device = VK_NULL_HANDLE;
	
	VkQueue							m_GraphicsQueue, m_PresentQueue;
	
	VkSurfaceKHR					m_Surface;

	VkSwapchainKHR					m_Swapchain;
	std::vector<VkImage>			m_SwapchainImages;
	std::vector<VkImageView>		m_SwapchainImageViews;
	std::vector<VkFramebuffer>		m_SwapchainFramebuffers;

	VkSurfaceFormatKHR				m_SurfaceFormat;
	VkExtent2D						m_Extent;
	VkFormat						m_SwapchainFormat;

	VkRenderPass					m_RenderPass;
	VkDescriptorSetLayout			m_DescriptorSetLayout;
	VkPipelineLayout				m_PipelineLayout;
	VkPipeline						m_Pipeline;

	VkCommandPool					m_CommandPool;
	std::vector<VkCommandBuffer>	m_CommandBuffers;
	VkDescriptorPool				m_DescriptorPool;
	std::vector<VkDescriptorSet>	m_DescriptorSets;

	std::vector<VkSemaphore>		m_ImageAvailableSemaphores , m_RenderFinishedSemaphores;
	std::vector<VkFence>			m_InFlightFences, m_ImagesInFlight;

	VkDebugUtilsMessengerEXT		m_DebugMessenger;

	VkBuffer						m_VertexBuffer, m_IndexBuffer;
	uint32_t						m_MipLevels;
	VkImage							m_TextureImage, m_DepthImage, m_ColorImage;
	VkImageView						m_TextureImageView, m_DepthImageView, m_ColorImageView;
	VkSampler						m_TextureSampler;
	VkDeviceMemory					m_VertexBufferMemory, m_IndexBufferMemory, m_TextureImageMemory, m_DepthImageMemory, m_ColorImageMemory;
	std::vector<VkBuffer>			m_UniformBuffers;
	std::vector<VkDeviceMemory>		m_UniformBufferMemories;

	bool							m_FramebufferResized = false;

	VkSampleCountFlagBits			m_MsaaSamples;

	glm::vec4						m_TintColor = glm::vec4(1.0, 1.0, 1.0, 1.0);

	Camera							m_Camera{};
	float							m_DeltaTime, m_LastFrame;
	bool							m_FirstMouse = true;
	float							m_LastX, m_LastY;

};
int main() {

	Application app;
	try
	{
		app.Run();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
