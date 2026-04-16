/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "creation_os_native_m4.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(__APPLE__)
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#endif

#if defined(__APPLE__)
static NSURL *cos_metallib_url(void)
{
    const char *env = getenv("CREATION_OS_METALLIB");
    if (env && env[0])
        return [NSURL fileURLWithPath:[NSString stringWithUTF8String:env]];

    NSFileManager *fm = [NSFileManager defaultManager];
    NSArray<NSString *> *candidates = @[
        @"native_m4/creation_os_lw.metallib",
        @"./native_m4/creation_os_lw.metallib",
        @"creation_os_lw.metallib",
    ];
    for (NSString *rel in candidates) {
        NSString *p = [rel stringByStandardizingPath];
        if ([fm isReadableFileAtPath:p])
            return [NSURL fileURLWithPath:p];
    }
    return [NSURL fileURLWithPath:@"native_m4/creation_os_lw.metallib"];
}
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

        NSURL *url = cos_metallib_url();
        const char *fp = [[url path] UTF8String];
        if (access(fp, R_OK) != 0)
            return false;
        lib = [dev newLibraryWithURL:url error:&err];
        if (!lib) {
            if (getenv("CREATION_OS_METAL_DEBUG") != NULL) {
                fprintf(stderr, "native-m4: Metal library load failed (%s): %s\n", fp,
                        err ? [[err localizedDescription] UTF8String] : "unknown");
            }
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
        [cb setLabel:@"creation_os_living_weights"];
        id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
        [enc setLabel:@"creation_os_living_weights"];
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
