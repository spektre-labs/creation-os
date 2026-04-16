/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "creation_os_native_m4.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(__APPLE__)
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#endif

bool cos_living_weights_metal(float *logits, const uint8_t *reputation, int vocab, float scale)
{
#if !defined(__APPLE__)
    (void)logits;
    (void)reputation;
    (void)vocab;
    (void)scale;
    return false;
#else
    if (!logits || !reputation || vocab <= 0)
        return false;

    @autoreleasepool {
        id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
        if (!dev)
            return false;

        NSError *err = nil;
        id<MTLLibrary> lib = nil;

        const char *path = getenv("CREATION_OS_METALLIB");
        NSURL *url = nil;
        if (path && path[0]) {
            url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path]];
        } else {
            /* Default: ./native_m4/creation_os_lw.metallib next to CWD */
            url = [NSURL fileURLWithPath:@"native_m4/creation_os_lw.metallib"];
        }
        const char *fp = [[url path] UTF8String];
        if (access(fp, R_OK) != 0)
            return false;
        lib = [dev newLibraryWithURL:url error:&err];
        if (!lib) {
            return false;
        }

        id<MTLFunction> fn = [lib newFunctionWithName:@"creation_os_living_weights_apply"];
        if (!fn)
            return false;

        id<MTLComputePipelineState> pso = [dev newComputePipelineStateWithFunction:fn error:&err];
        if (!pso) {
            fprintf(stderr, "native-m4: pipeline failed: %s\n",
                    err ? [[err localizedDescription] UTF8String] : "unknown");
            return false;
        }

        id<MTLCommandQueue> q = [dev newCommandQueue];
        if (!q)
            return false;

        const NSUInteger n = (NSUInteger)vocab;
        id<MTLBuffer> repBuf =
            [dev newBufferWithBytes:reputation length:n options:MTLResourceStorageModeShared];
        id<MTLBuffer> logBuf =
            [dev newBufferWithBytesNoCopy:logits
                                   length:n * sizeof(float)
                                  options:MTLResourceStorageModeShared
                              deallocator:nil];
        float sc = scale;
        id<MTLBuffer> scBuf = [dev newBufferWithBytes:&sc length:sizeof(sc) options:MTLResourceStorageModeShared];
        if (!repBuf || !logBuf || !scBuf)
            return false;

        id<MTLCommandBuffer> cb = [q commandBuffer];
        id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
        [enc setComputePipelineState:pso];
        [enc setBuffer:repBuf offset:0 atIndex:0];
        [enc setBuffer:logBuf offset:0 atIndex:1];
        [enc setBuffer:scBuf offset:0 atIndex:2];

        MTLSize grid = MTLSizeMake(n, 1, 1);
        NSUInteger maxW = pso.maxTotalThreadsPerThreadgroup;
        MTLSize tg = MTLSizeMake((maxW > 256) ? 256 : maxW, 1, 1);
        [enc dispatchThreads:grid threadsPerThreadgroup:tg];
        [enc endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
        return true;
    }
#endif
}
