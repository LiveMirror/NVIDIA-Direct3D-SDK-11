// Copyright (c) 2011 NVIDIA Corporation. All rights reserved.
//
// TO  THE MAXIMUM  EXTENT PERMITTED  BY APPLICABLE  LAW, THIS SOFTWARE  IS PROVIDED
// *AS IS*  AND NVIDIA AND  ITS SUPPLIERS DISCLAIM  ALL WARRANTIES,  EITHER  EXPRESS
// OR IMPLIED, INCLUDING, BUT NOT LIMITED  TO, NONINFRINGEMENT,IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL  NVIDIA 
// OR ITS SUPPLIERS BE  LIABLE  FOR  ANY  DIRECT, SPECIAL,  INCIDENTAL,  INDIRECT,  OR  
// CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION,  DAMAGES FOR LOSS 
// OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR ANY 
// OTHER PECUNIARY LOSS) ARISING OUT OF THE  USE OF OR INABILITY  TO USE THIS SOFTWARE, 
// EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
//
// Please direct any bugs or questions to SDKFeedback@nvidia.com

#define BLOCK_SIZE 256
#define LOOP_UNROLL 8

// Constants used by the compute shader
cbuffer cbUpdate
{
	float g_timestep;
	float g_softeningSquared;
	uint  g_numParticles;
	uint  g_readOffset;
	uint  g_writeOffset;
};	

// all positions, then all velocities
// We must combine into a single UAV because DirectCompute does 
// not support multiple UAVs on "downlevel" (i.e. DX10) hardware
RWStructuredBuffer<float4> particles;

// This function computes the gravitational attraction between two bodies
// at positions bi and bj.  The mass of the bodies is stored in the w 
// component
float3 
bodyBodyInteraction(float4 bi, float4 bj) 
{
    // r_ij  [3 FLOPS]
    float3 r = bi - bj;

    // distSqr = dot(r_ij, r_ij) + EPS^2  [6 FLOPS]
    float distSqr = dot(r, r);
    distSqr += g_softeningSquared;

    // invDistCube =1/distSqr^(3/2)  [4 FLOPS (2 mul, 1 sqrt, 1 inv)]
    float invDist = 1.0f / sqrt(distSqr);
	float invDistCube =  invDist * invDist * invDist;

    // s = m_j * invDistCube [1 FLOP]
    float s = bj.w * invDistCube;

    // a_i =  a_i + s * r_ij [6 FLOPS]  
    return r*s;
}

// This groupshared memory is used to cache BLOCK_SIZE body positions
// in on-chip shared memory so that we can achieve maximal reuse of 
// data loaded from global memory
groupshared float4 sharedPos[BLOCK_SIZE];

// The main gravitation function, computes the interaction between
// a body and all other bodies in the system
float3 gravitation(float4 myPos, float3 accel)
{
    unsigned int i = 0;

    // Here we unroll the loop. Using [unroll] seems to perform 
    // just as well as hand-unrolling the loop 8 times

    [unroll]
    for (unsigned int counter = 0; counter < BLOCK_SIZE; ) 
    {
        accel += bodyBodyInteraction(sharedPos[i++], myPos); 
	    counter++;
/*#if LOOP_UNROLL > 1
        accel += bodyBodyInteraction(sharedPos[i++], myPos); 
	    counter++;
#endif
#if LOOP_UNROLL > 2
        accel += bodyBodyInteraction(sharedPos[i++], myPos); 
        accel += bodyBodyInteraction(sharedPos[i++], myPos); 
	    counter += 2;
#endif
#if LOOP_UNROLL > 4
        accel += bodyBodyInteraction(sharedPos[i++], myPos); 
        accel += bodyBodyInteraction(sharedPos[i++], myPos); 
        accel += bodyBodyInteraction(sharedPos[i++], myPos); 
        accel += bodyBodyInteraction(sharedPos[i++], myPos); 
	    counter += 4;
#endif*/
    }

    return accel;
}

// Computes the total acceleration on the body with position myPos 
// caused by the gravitational attraction of all other bodies in 
// the simulation
float3 computeBodyAccel(float4 bodyPos, uint threadId, uint blockId)
{
    float3 acceleration = {0.0f, 0.0f, 0.0f};
    
    uint p = BLOCK_SIZE;
    uint n = g_numParticles;
    uint numTiles = n / p;

    for (uint tile = 0; tile < numTiles; tile++) 
    {
        sharedPos[threadId] = particles[g_readOffset + tile * p + threadId];
       
        GroupMemoryBarrierWithGroupSync();

        acceleration = gravitation(bodyPos, acceleration);
        
        GroupMemoryBarrierWithGroupSync();
    }

    return acceleration;
}

// NBodyUpdate is the compute shader entry point for the n-body simulation
// This function first computes the acceleration on all bodies in parallel,
// and then integrates the velocity and position to get the new state of
// all particles, using a simple Leapfrog-Verlet integration step.

[numthreads(BLOCK_SIZE,1,1)]
void NBodyUpdate(uint threadId        : SV_GroupIndex,
                 uint3 groupId        : SV_GroupID,
                 uint3 globalThreadId : SV_DispatchThreadID)
{
	
    float4 pos = particles[g_readOffset + globalThreadId.x]; 
    float4 vel = particles[2 * g_numParticles + globalThreadId.x]; 
	  
	// compute acceleration
	float3 accel = computeBodyAccel(pos, threadId, groupId);
	
	// Leapfrog-Verlet integration of velocity and position
	vel.xyz += accel * g_timestep;
	pos.xyz += vel   * g_timestep;
    
	particles[g_writeOffset + globalThreadId.x]      = pos;
	particles[2 * g_numParticles + globalThreadId.x] = vel;
}
