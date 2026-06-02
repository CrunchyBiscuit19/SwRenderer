#include <Data/SwAsset.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwLogger.h>
#include <Scene/SwScene.h>
#include <fmt/core.h>
#include <quill/LogMacros.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fastgltf/glm_element_traits.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <magic_enum.hpp>

SwRendererContext SwAsset::sRendererContext{};
std::uint32_t SwAsset::sLatestAssetId{0};
std::unordered_map<SwSamplerOptions, SwSampler> SwAsset::sSamplers{};

vk::Filter SwAsset::extractFilter(fastgltf::Filter filter) {
    switch (filter) {
        case fastgltf::Filter::Nearest:
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::NearestMipMapLinear:
            return vk::Filter::eNearest;

        case fastgltf::Filter::Linear:
        case fastgltf::Filter::LinearMipMapNearest:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return vk::Filter::eLinear;
    }
}

vk::SamplerMipmapMode SwAsset::extractMipmapMode(fastgltf::Filter filter) {
    switch (filter) {
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::LinearMipMapNearest:
            return vk::SamplerMipmapMode::eNearest;

        case fastgltf::Filter::NearestMipMapLinear:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return vk::SamplerMipmapMode::eLinear;
    }
}

vk::SamplerAddressMode SwAsset::extractAddressMode(fastgltf::Wrap wrap) {
    switch (wrap) {
        case fastgltf::Wrap::Repeat:
            return vk::SamplerAddressMode::eRepeat;
        case fastgltf::Wrap::ClampToEdge:
            return vk::SamplerAddressMode::eClampToEdge;
        case fastgltf::Wrap::MirroredRepeat:
            return vk::SamplerAddressMode::eMirroredRepeat;
        default:
            return vk::SamplerAddressMode::eRepeat;
    }
}

void SwAsset::init(SwRendererContext assetContext) { sRendererContext = assetContext; }

void SwAsset::cleanup() { sSamplers.clear(); }

std::string SwAsset::getNameFromFilePath(const std::filesystem::path& assetPath) { return assetPath.stem().string(); }

void SwAsset::loadRawAsset(std::filesystem::path& assetPath) {
    mName = getNameFromFilePath(assetPath);
    fastgltf::Parser parser{};
    fastgltf::Asset gltf;
    fastgltf::GltfDataBuffer data;
    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers |
                                 fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages;
    data.loadFromFile(assetPath);
    auto type = fastgltf::determineGltfFileType(&data);
    if (type == fastgltf::GltfType::Invalid) {
        LOG_ERROR(sRendererContext.mLogger->getQuillLoggerPtr(), "{} Failed to determine GLTF Container", mName);
    }
    auto load = (type == fastgltf::GltfType::glTF) ? (parser.loadGLTF(&data, assetPath.parent_path(), gltfOptions))
                                                   : (parser.loadBinaryGLTF(&data, assetPath.parent_path(), gltfOptions));
    if (load) {
        gltf = std::move(load.get());
    } else {
        LOG_ERROR(sRendererContext.mLogger->getQuillLoggerPtr(), "{} Failed to load GLTF Asset: {}", mName, fastgltf::to_underlying(load.error()));
    }
    mRawAsset = std::move(gltf);
}

void SwAsset::constructBuffers() {
    mMaterialConstantsBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        NUM_MODEL_MATERIALS * sizeof(SwMaterialConstants)
    );
    mBoundsBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        NUM_MODEL_BOUNDS * sizeof(SwBounds)
    );
    mNodeTransformsBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        NUM_MODEL_NODES * sizeof(glm::mat4)
    );
    mInstancesBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        NUM_MODEL_INSTANCES * sizeof(SwInstance::Data)
    );
};

void SwAsset::constructSamplerAndSamplerOptions() {
    vk::SamplerCreateInfo defaultSamplerCreateInfo;
    SwSamplerOptions defaultSamplerOptions(defaultSamplerCreateInfo);
    sSamplers.emplace(defaultSamplerOptions, SwSamplerFactory::createSampler(defaultSamplerCreateInfo));

    sSamplers.reserve(mRawAsset.samplers.size());
    for (fastgltf::Sampler& sampler : mRawAsset.samplers) {
        vk::SamplerCreateInfo samplerCreateInfo;
        samplerCreateInfo.pNext = nullptr;
        samplerCreateInfo.maxLod = vk::LodClampNone;
        samplerCreateInfo.minLod = 0;
        samplerCreateInfo.magFilter = extractFilter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        samplerCreateInfo.minFilter = extractFilter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));
        samplerCreateInfo.mipmapMode = extractMipmapMode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));
        samplerCreateInfo.addressModeU = extractAddressMode(sampler.wrapS);
        samplerCreateInfo.addressModeV = extractAddressMode(sampler.wrapT);
        samplerCreateInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
        SwSamplerOptions samplerOptions(samplerCreateInfo);
        sSamplers.emplace(samplerOptions, SwSamplerFactory::createSampler(samplerCreateInfo));
        mSamplerOptions.emplace_back(samplerOptions);
    }
}

void SwAsset::constructImage(std::uint32_t imageIndex, SwMaterialTexture::Type texType) {
    fastgltf::Image& rawImage = mRawAsset.images[imageIndex];

    SwColorImage2D newImage;
    std::int32_t width, height, nrChannels;
    unsigned char* data = nullptr;
    vk::Extent3D imageSize{};

    std::visit(
        fastgltf::visitor{
            // Image stored outside of GLTF / GLB file.
            [&](const fastgltf::sources::URI& filePath) {
                assert(filePath.fileByteOffset == 0);
                assert(filePath.uri.isLocalPath());
                const std::string path(filePath.uri.path().begin(), filePath.uri.path().end());
                data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
            },
            // Image is loaded directly into a std::vector. If the texture is on base64, or if we instruct it to load external image files
            // (fastgltf::Options::LoadExternalImages).
            [&](const fastgltf::sources::Vector& vector) {
                data = stbi_load_from_memory(vector.bytes.data(), static_cast<std::uint32_t>(vector.bytes.size()), &width, &height, &nrChannels, 4);
            },
            // Image embedded into the binary GLB file.
            [&](const fastgltf::sources::BufferView& view) {
                const auto& bufferView = mRawAsset.bufferViews[view.bufferViewIndex];
                auto& buffer = mRawAsset.buffers[bufferView.bufferIndex];
                std::visit(
                    fastgltf::visitor{
                        [&](const fastgltf::sources::Vector& vector) {
                            data = stbi_load_from_memory(
                                vector.bytes.data() + bufferView.byteOffset, static_cast<std::uint32_t>(bufferView.byteLength), &width, &height, &nrChannels, 4
                            );
                        },
                        [](const auto& arg) {},
                    },
                    buffer.data
                );
            },
            [](const auto& arg) {},
        },
        rawImage.data
    );

    vk::Format imageFormat;
    switch (texType) {
        case SwMaterialTexture::Type::Base:
        case SwMaterialTexture::Type::Emissive:
            imageFormat = SwMaterialTexture::SRGB_IMAGE_FORMAT;
            break;
        case SwMaterialTexture::Type::MetallicRoughness:
        case SwMaterialTexture::Type::Normal:
        case SwMaterialTexture::Type::Occlusion:
        default:
            imageFormat = SwMaterialTexture::UNORM_IMAGE_FORMAT;
            break;
    }

    if (data) {
        imageSize.width = static_cast<std::uint32_t>(width);
        imageSize.height = static_cast<std::uint32_t>(height);
        imageSize.depth = 1;
        newImage = SwImageFactory::createColorImage2D(data, imageFormat, imageSize, vk::ImageUsageFlagBits::eSampled, true);
        stbi_image_free(data);
    } else {
        throw std::runtime_error(fmt::format("{} failed to read image {}: {}", mName, imageIndex, stbi_failure_reason() ? stbi_failure_reason() : "Unknown"));
    }

    mImages[imageIndex] = std::move(newImage);
}

void SwAsset::constructMaterials() {
    mImages.resize(mRawAsset.images.size());  // resize is not a typo
    mMaterials.reserve(mRawAsset.materials.size());
    std::vector<SwMaterialConstants> materialConstants;
    materialConstants.reserve(mRawAsset.materials.size());

    for (std::uint32_t i = 0; i < mRawAsset.materials.size(); i++) {
        fastgltf::Material& material = mRawAsset.materials[i];

        std::string name = std::string(material.name);
        if (name.empty()) {
            name = fmt::format("{}", i);
        }
        name = fmt::format("{}_mat{}", mName, name);

        SwMaterialPipelineOptions pipelineOptions(material.doubleSided, material.alphaMode);

        SwMaterialConstants constants;
        constants.mBaseFactor = glm::vec4(
            material.pbrData.baseColorFactor[0], material.pbrData.baseColorFactor[1], material.pbrData.baseColorFactor[2], material.pbrData.baseColorFactor[3]
        );
        constants.mMetallicRoughnessFactor = glm::vec2(material.pbrData.metallicFactor, material.pbrData.roughnessFactor);
        constants.mEmissiveFactor = glm::vec4(material.emissiveFactor[0], material.emissiveFactor[1], material.emissiveFactor[2], 0);
        if (material.normalTexture.has_value()) {
            constants.mNormalScale = material.normalTexture.value().scale;
        }
        if (material.occlusionTexture.has_value()) {
            constants.mOcclusionStrength = material.occlusionTexture.value().strength;
        }
        materialConstants.emplace_back(constants);

        auto retrieveImage = [&](std::uint32_t imageIndex, SwMaterialTexture::Type texType) -> SwColorImage2D& {
            if (!mImages[imageIndex].has_value()) {
                constructImage(imageIndex, texType);
            }
            return mImages[imageIndex].value();
        };

        auto resolveTexture = [&](auto& texInfo, SwMaterialTexture::Type texType) -> SwMaterialTexture {
            if (texInfo.has_value()) {
                auto& tex = mRawAsset.textures[texInfo.value().textureIndex];
                SwColorImage2D& image = tex.imageIndex.has_value() ? retrieveImage(static_cast<std::uint32_t>(tex.imageIndex.value()), texType)
                                                                   : SwMaterialTexture::sDefaultWhiteTexture.getImage();
                SwSampler& sampler =
                    tex.samplerIndex.has_value() ? sSamplers[mSamplerOptions[tex.samplerIndex.value()]] : SwMaterialTexture::sDefaultWhiteTexture.getSampler();
                return SwMaterialTexture(&image, &sampler);
            }
            /*LOG_DEBUG(
                sRendererContext.mLogger->getQuillLoggerPtr(), "{} material {} {} using default.", mName, name, magic_enum::enum_name(texType).data()
            );*/
            return SwMaterialTexture::retrieveDefaultWhiteTexture();
        };

        SwMaterialTexture baseTexture = resolveTexture(material.pbrData.baseColorTexture, SwMaterialTexture::Type::Base);
        SwMaterialTexture metallicRoughnessTexture = resolveTexture(material.pbrData.metallicRoughnessTexture, SwMaterialTexture::Type::MetallicRoughness);
        SwMaterialTexture emissiveTexture = resolveTexture(material.emissiveTexture, SwMaterialTexture::Type::Emissive);
        SwMaterialTexture normalTexture = resolveTexture(material.normalTexture, SwMaterialTexture::Type::Normal);
        SwMaterialTexture occlusionTexture = resolveTexture(material.occlusionTexture, SwMaterialTexture::Type::Occlusion);

        SwMaterialResources resources(
            std::move(baseTexture), std::move(metallicRoughnessTexture), std::move(normalTexture), std::move(occlusionTexture), std::move(emissiveTexture)
        );

        mMaterials.emplace_back(name, i, pipelineOptions, constants, std::move(resources));
    }

    std::memcpy(
        SwMaterialConstants::sMaterialConstantsStagingBuffer.getMappedPtr(), materialConstants.data(), materialConstants.size() * sizeof(SwMaterialConstants)
    );

    vk::BufferCopy materialConstantsCopy{};
    materialConstantsCopy.dstOffset = 0;
    materialConstantsCopy.srcOffset = 0;
    materialConstantsCopy.size = materialConstants.size() * sizeof(SwMaterialConstants);

    sRendererContext.mImmSubmit->individualSubmit([this, materialConstantsCopy](vk::CommandBuffer cmd) {
        SwMaterialConstants::sMaterialConstantsStagingBuffer.emitBarrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead);
        mMaterialConstantsBuffer.copyFrom(cmd, SwMaterialConstants::sMaterialConstantsStagingBuffer, materialConstantsCopy);
        mMaterialConstantsBuffer.emitBarrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead);
    });
}

void SwAsset::constructMeshes() {
    mMeshes.reserve(mRawAsset.meshes.size());
    std::vector<std::uint32_t> indices;
    std::vector<SwVertex> vertices;
    std::vector<SwPrimitive> primitives;
    std::uint32_t boundsOffset = 0;

    for (fastgltf::Mesh& mesh : mRawAsset.meshes) {
        indices.clear();
        vertices.clear();
        primitives.clear();

        std::string name = fmt::format("{}{}", mName, mesh.name);

        for (auto&& p : mesh.primitives) {
            primitives.emplace_back(
                static_cast<std::uint32_t>(indices.size()),
                static_cast<std::uint32_t>(mRawAsset.accessors[p.indicesAccessor.value()].count),
                static_cast<std::uint32_t>(vertices.size()),
                p.materialIndex.has_value() ? mMaterials[p.materialIndex.value()] : mMaterials[0]
            );

            size_t vertexStartOffset = vertices.size();

            // Load indexes
            fastgltf::Accessor& indexAccessor = mRawAsset.accessors[p.indicesAccessor.value()];
            indices.reserve(indices.size() + indexAccessor.count);
            fastgltf::iterateAccessor<std::uint32_t>(mRawAsset, indexAccessor, [&](std::uint32_t index) { indices.emplace_back(index); });

            // Load vertex positions
            fastgltf::Accessor& posAccessor = mRawAsset.accessors[p.findAttribute("POSITION")->second];
            vertices.resize(vertices.size() + posAccessor.count);
            fastgltf::iterateAccessorWithIndex<glm::vec3>(mRawAsset, posAccessor, [&](glm::vec3 v, size_t pos) {
                SwVertex vertex;
                vertex.mPosition = v;
                vertices[vertexStartOffset + pos] = vertex;
            });

            // Load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(mRawAsset, mRawAsset.accessors[normals->second], [&](glm::vec3 n, size_t pos) {
                    vertices[vertexStartOffset + pos].mNormal = n;
                });
            }

            // Load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(mRawAsset, mRawAsset.accessors[uv->second], [&](glm::vec2 uv, size_t pos) {
                    vertices[vertexStartOffset + pos].mUv = glm::vec2(uv.x, uv.y);
                });
            }

            // Load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(mRawAsset, mRawAsset.accessors[colors->second], [&](glm::vec4 c, size_t pos) {
                    vertices[vertexStartOffset + pos].mColor = c;
                });
            }
        }

        std::uint32_t numVertices = vertices.size();
        std::uint32_t numIndices = indices.size();
        std::uint32_t relativeFirstBounds = boundsOffset;
        boundsOffset++;

        SwBounds bounds(glm::vec4(vertices[0].mPosition, 0.f), glm::vec4(vertices[0].mPosition, 0.f));
        for (auto& vertex : vertices) {
            bounds.mMin = glm::min(bounds.mMin, vertex.mPosition);
            bounds.mMax = glm::max(bounds.mMax, vertex.mPosition);
        }

        const vk::DeviceSize srcVertexVectorSize = vertices.size() * sizeof(SwVertex);
        const vk::DeviceSize srcIndexVectorSize = indices.size() * sizeof(std::uint32_t);
        SwAllocatedBuffer vertexBuffer = SwBufferFactory::createAllocatedBuffer(
            vk::BufferUsageFlagBits::eStorageBuffer,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            srcVertexVectorSize
        );
        SwAllocatedBuffer indexBuffer = SwBufferFactory::createAllocatedBuffer(
            vk::BufferUsageFlagBits::eIndexBuffer,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            srcIndexVectorSize
        );

        std::memcpy(static_cast<char*>(SwMesh::sMeshStagingBuffer.getMappedPtr()) + 0, vertices.data(), srcVertexVectorSize);
        std::memcpy(static_cast<char*>(SwMesh::sMeshStagingBuffer.getMappedPtr()) + srcVertexVectorSize, indices.data(), srcIndexVectorSize);

        vk::BufferCopy vertexCopy{};
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = srcVertexVectorSize;
        vk::BufferCopy indexCopy{};
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = srcVertexVectorSize;
        indexCopy.size = srcIndexVectorSize;

        mMeshes.emplace_back(
            mId, name, primitives, bounds, relativeFirstBounds, std::move(vertexBuffer), numVertices, 0, std::move(indexBuffer), numIndices, 0
        );

        SwMesh& createdMesh = mMeshes.back();
        sRendererContext.mImmSubmit->individualSubmit([&createdMesh, vertexCopy, indexCopy](vk::CommandBuffer cmd) {
            SwMesh::sMeshStagingBuffer.emitBarrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead);
            createdMesh.getVertexBuffer().copyFrom(cmd, SwMesh::sMeshStagingBuffer, vertexCopy);
            createdMesh.getIndexBuffer().copyFrom(cmd, SwMesh::sMeshStagingBuffer, indexCopy);
            createdMesh.getVertexBuffer().emitBarrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead);
            createdMesh.getIndexBuffer().emitBarrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead);
        });
    }

    const vk::DeviceSize boundsSize = mMeshes.size() * sizeof(SwBounds);
    std::vector<SwBounds> boundsVector;
    for (const auto& mesh : mMeshes) {
        boundsVector.emplace_back(mesh.getBounds());
    }
    std::memcpy(SwBounds::sBoundsStagingBuffer.getMappedPtr(), boundsVector.data(), boundsSize);

    vk::BufferCopy boundsCopy{};
    boundsCopy.dstOffset = 0;
    boundsCopy.srcOffset = 0;
    boundsCopy.size = boundsSize;

    sRendererContext.mImmSubmit->individualSubmit([this, boundsCopy](vk::CommandBuffer cmd) {
        SwBounds::sBoundsStagingBuffer.emitBarrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead);
        mBoundsBuffer.copyFrom(cmd, SwBounds::sBoundsStagingBuffer, boundsCopy);
        mBoundsBuffer.emitBarrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead);
    });
}

void SwAsset::constructNodes() {
    mNodes.reserve(mRawAsset.nodes.size());

    for (std::uint32_t i = 0; i < mRawAsset.nodes.size(); i++) {
        fastgltf::Node& rawNode = mRawAsset.nodes[i];

        std::string name = fmt::format("{}_node{}", mName, rawNode.name);
        std::uint32_t relativeNodeIndex = i;
        // First function if it's a mat4 transform, second function if it's separate transform / rotate / scale quaternion or vec3
        glm::mat4 localTransform(1.f);
        std::visit(
            fastgltf::visitor{
                [&](const fastgltf::Node::TransformMatrix& matrix) { std::memcpy(&localTransform, matrix.data(), sizeof(matrix)); },
                [&](const fastgltf::Node::TRS& transform) {
                    const glm::vec3 tl(transform.translation[0], transform.translation[1], transform.translation[2]);
                    const glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]);
                    const glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);
                    const glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                    const glm::mat4 rm = glm::toMat4(rot);
                    const glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);
                    localTransform = tm * rm * sm;
                }
            },
            rawNode.transform
        );

        std::shared_ptr<SwNode> localNode = std::make_shared<SwNode>(name, relativeNodeIndex, localTransform);
        if (rawNode.meshIndex.has_value()) {
            localNode = std::make_shared<SwMeshNode>(name, relativeNodeIndex, localTransform, mMeshes[*rawNode.meshIndex]);
        }

        mNodes.emplace_back(localNode);
    }

    // Setup hierarchy
    for (std::uint32_t i = 0; i < mRawAsset.nodes.size(); i++) {
        fastgltf::Node& rawNode = mRawAsset.nodes[i];
        std::shared_ptr<SwNode> localNode = mNodes[i];
        for (auto& nodeChildIndex : rawNode.children) {
            localNode->addChild(mNodes[nodeChildIndex]);
        }
    }

    // Find the top nodes, with no parents
    for (auto& node : mNodes) {
        if (node->getParent().lock() == nullptr) {
            mTopNodes.emplace_back(node);
            node->refreshTransform(glm::mat4{1.f});
        }
    }

    for (std::uint32_t i = 0; i < mNodes.size(); i++) {
        std::memcpy(
            static_cast<char*>(SwNode::sNodeTransformsStagingBuffer.getMappedPtr()) + i * sizeof(glm::mat4),
            glm::value_ptr(mNodes[i]->getWorldTransform()),
            sizeof(glm::mat4)
        );
    }

    vk::BufferCopy nodeTransformsCopy{};
    nodeTransformsCopy.dstOffset = 0;
    nodeTransformsCopy.srcOffset = 0;
    nodeTransformsCopy.size = mNodes.size() * sizeof(glm::mat4);

    sRendererContext.mImmSubmit->individualSubmit([this, nodeTransformsCopy](vk::CommandBuffer cmd) {
        SwNode::sNodeTransformsStagingBuffer.emitBarrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead);
        mNodeTransformsBuffer.copyFrom(cmd, SwNode::sNodeTransformsStagingBuffer, nodeTransformsCopy);
        mNodeTransformsBuffer.emitBarrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead);
    });
}

SwAsset::SwAsset(std::filesystem::path& assetPath) : mId(sLatestAssetId++) {
    loadRawAsset(assetPath);
    constructBuffers();
    constructSamplerAndSamplerOptions();
    constructMaterials();
    constructMeshes();
    constructNodes();
}

void SwAsset::generateRenderItemsAndRenderInstances() {
    if (mDelete) return;
    for (auto& topNode : mTopNodes) {
        topNode->generateRenderItemsAndRenderInstances();
    }
}

void SwAsset::createInstance(SwInstance::Data instanceData) {
    mInstances.emplace_back(mId, instanceData);
    mReloadInstancesFlag = true;
    sRendererContext.mScene->mFlags.mInstanceLoaded = true;
}

void SwAsset::createInstance(SwCamera& camera) { createInstance(SwInstance::Data(camera.getSpawnTransform())); }

void SwAsset::reloadInstances() {
    if (mInstances.empty()) {
        mReloadInstancesFlag = false;
        return;
    }

    std::uint32_t dstOffset = 0;
    for (auto& instance : mInstances) {
        std::memcpy(static_cast<char*>(mInstancesBuffer.getMappedPtr()) + dstOffset, &instance.getData(), sizeof(SwInstance::Data));
        dstOffset += sizeof(SwInstance::Data);
    }

    mReloadInstancesFlag = false;
}

void SwAsset::markDelete() {
    mDelete = true;
    for (auto& instance : mInstances) {
        instance.markDelete();
    }
}