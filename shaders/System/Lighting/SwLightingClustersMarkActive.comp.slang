import System.Lighting.SwLighting;
import Data.SwConstants;

[[vk::binding(0, 0)]]
public uniform Texture2D<float> depthImage;

[[vk::push_constant]]
SwLighting::ClustersMarkActivePC clustersMarkActivePc;

[numthreads(SwConstants::MAX_2D_WORKGROUP_THREADS, SwConstants::MAX_2D_WORKGROUP_THREADS, 1)]
void main(SwLighting::CSInput in) {
    // TODO implement active cluster marking from the depth buffer
}
