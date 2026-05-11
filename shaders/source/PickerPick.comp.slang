import Picker;

[[vk::binding(0, 0)]]
public uniform Texture2D<uint2> pickerImage;

[[vk::push_constant]]
PickerPickPushConstants pickerPickPc;

[numthreads(1)]
void main(CSInput in) {
    uint2 read = pickerImage.Load(int3(pickerPickPc.pickerBuffer->coords, 0));
    pickerPickPc.pickerBuffer->read = uint2(read.x, read.y);
}
