import SwPick;

uint2 main(SwPick::FSInput in) : SV_Target
{
    return uint2(in.mModelIndex + 1, in.mInstanceIndex + 1);
}
