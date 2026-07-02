import SwPick;

[[vk::binding(0, 0)]]
public uniform Texture2D<uint2> readbackImage;

[[vk::push_constant]]
SwPick::ReadbackPC readbackPc;

[numthreads(1)]
void main(SwPick::CSInput in) {
    uint2 read = readbackImage.Load(int3(readbackPc.mReadbackBuffer->mCoords, 0));
    readbackPc.mReadbackBuffer->mRead = uint2(read.x, read.y);
}
