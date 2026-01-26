# RSJFW Vulkan Layer Build Configuration

# Fetch vkroots (header-only Vulkan layer framework)
FetchContent_Declare(
    vkroots
    GIT_REPOSITORY https://github.com/Joshua-Ashton/vkroots
    GIT_TAG main
)
FetchContent_MakeAvailable(vkroots)

# Create the Vulkan layer shared library
add_library(VkLayer_rsjfw_fixes SHARED
    layer.cpp
)

target_include_directories(VkLayer_rsjfw_fixes PRIVATE
    ${vkroots_SOURCE_DIR}
)

target_link_libraries(VkLayer_rsjfw_fixes PRIVATE
    Vulkan::Vulkan
    dl
)

# Configure the JSON manifest
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/VkLayer_rsjfw_fixes.json.in
    ${CMAKE_CURRENT_BINARY_DIR}/VkLayer_rsjfw_fixes.json
    COPYONLY
)

# Install the layer library and manifest
install(TARGETS VkLayer_rsjfw_fixes DESTINATION lib/rsjfw/layer)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/VkLayer_rsjfw_fixes.json DESTINATION lib/rsjfw/layer)
