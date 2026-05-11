module RenderItem;

public struct RenderItem
{
    public uint indexCount;
    public uint renderInstanceCount;
    public uint firstIndex;
    public uint vertexOffset;
    public uint firstRenderInstance;
    public uint materialIndex;
    public uint nodeTransformIndex;
    public uint modelIndex;
    public uint firstInstance;
    public uint boundsIndex;
};

public struct RenderInstance {
    public uint renderItemsIndex;
    public uint mainInstanceIndex;
}
