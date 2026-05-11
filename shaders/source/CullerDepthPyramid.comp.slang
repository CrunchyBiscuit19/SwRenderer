import Culler;

static const uint MAX_2D_WORKGROUP_THREADS = 32;

[[vk::binding(0, 0)]]
public uniform Texture2D<float> depthFullImage;
[[vk::binding(1, 0)]]
public uniform Texture2D<float> depthPyramidRead[];
[[vk::binding(2, 0)]]
public uniform RWTexture2D<float> depthPyramidWrite[];

[[vk::push_constant]]
CullerDepthPyramidPushConstants cullerDepthPyramidPc;

float findMinDepth(uint2 readPos, uint2 readExtent, Texture2D<float> readImage) {
    // Calculate texel positions surrounding the current read position
    uint3 topLeft = uint3(readPos, 0) + uint3(-1, 1, 0);
    uint3 topCenter = uint3(readPos, 0) + uint3(0, 1, 0);
    uint3 topRight = uint3(readPos, 0) + uint3(1, 1, 0);
    uint3 midLeft = uint3(readPos, 0) + uint3(1, 0, 0);
    uint3 midCentre = uint3(readPos, 0) + uint3(0, 0, 0);
    uint3 midRight = uint3(readPos, 0) + uint3(-1, 0, 0);
    uint3 bottomLeft = uint3(readPos, 0) + uint3(1, -1, 0);
    uint3 bottomCenter = uint3(readPos, 0) + uint3(0, -1, 0);
    uint3 bottomRight = uint3(readPos, 0) + uint3(-1, -1, 0);

    // Read Level Extent is POT
    float minDepth =
        min(min(readImage.Load(topLeft), readImage.Load(topCenter)),
                         min(readImage.Load(midLeft), readImage.Load(midCentre)));

    // Read Level Extent has odd width (read extra column)
    if (readExtent.x % 2 != 0) {
        minDepth = min(minDepth, min(readImage.Load(topRight), readImage.Load(midRight)));
    }
    // Read Level Extent has odd height (read extra row)
    if (readExtent.y % 2 != 0) {
        minDepth = min(minDepth, min(readImage.Load(bottomLeft), readImage.Load(bottomCenter)));
    }
    // Read Level Extent has odd width and odd height (read extra corner)
    if (readExtent.x % 2 != 0 && readExtent.y % 2 != 0) {
        minDepth = min(minDepth, readImage.Load(bottomRight));
    }

    return minDepth;
}

[numthreads(MAX_2D_WORKGROUP_THREADS, MAX_2D_WORKGROUP_THREADS, 1)]
void main(CSInput in) {
    // Calculate final writing position
    uint readLevel = cullerDepthPyramidPc.level;
    uint writeLevel = cullerDepthPyramidPc.level + 1;
    uint2 readPos = in.dispatchThreadID.xy;
    uint2 writePos;
    uint2 readExtent;
    if (cullerDepthPyramidPc.readFromFull) {
        writePos = uint2(cullerDepthPyramidPc.depthFullRatio * readPos);
        readExtent = cullerDepthPyramidPc.depthFullExtent;
    } else {
        writePos = uint2(readPos.x >> 1, readPos.y >> 1);
        readExtent = uint2(cullerDepthPyramidPc.depthPyramidExtent.x >> readLevel, cullerDepthPyramidPc.depthPyramidExtent.y >> readLevel);
    }
    writePos.x = max(1, writePos.x);
    writePos.y = max(1, writePos.y);

    // Get minimum depth to write
    float minDepth;
    if (cullerDepthPyramidPc.readFromFull) {
        minDepth = findMinDepth(readPos, readExtent, depthFullImage);
    } else {
        minDepth = findMinDepth(readPos, readExtent, depthPyramidRead[readLevel]);
    }

    // Write into next level
    if (cullerDepthPyramidPc.readFromFull) {
        depthPyramidWrite[0].Store(writePos, minDepth);
    } else {
        depthPyramidWrite[writeLevel].Store(writePos, minDepth);
    }
}