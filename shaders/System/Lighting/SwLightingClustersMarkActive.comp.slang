import System.Lighting.SwLighting;
import Data.SwConstants;

[[vk::push_constant]]
SwLighting::ClustersMarkActivePC clustersMarkActivePc;

[numthreads(SwConstants::MAX_3D_WORKGROUP_THREADS, SwConstants::MAX_3D_WORKGROUP_THREADS, SwConstants::MAX_3D_WORKGROUP_THREADS)]
void main(SwLighting::CSInput in) {
    // TODO implement active cluster marking from the depth buffer
}
