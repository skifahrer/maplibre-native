#import "MLNDisplayUtils.h"
#import "MLNFoundation_Private.h"
#import "MLNLoggingConfiguration_Private.h"
#import "MLNMapView+Metal.h"

#import <mln/gfx/context.hpp>
#import <mln/mtl/renderable_resource.hpp>
#import <mln/util/monotonic_timer.hpp>

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>

#import <Metal/Metal.hpp>

@interface MLNMapViewImplDelegate : NSObject <MTKViewDelegate>
@end

@implementation MLNMapViewImplDelegate {
  MLNMapViewMetalImpl* _impl;
}

- (instancetype)initWithImpl:(MLNMapViewMetalImpl*)impl {
  if (self = [super init]) {
    _impl = impl;
  }
  return self;
}

- (void)mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size {
}

- (void)drawInMTKView:(MTKView*)view {
  _impl->render();
}

@end

class MLNMapViewMetalRenderableResource final : public mln::mtl::RenderableResource {
public:
  MLNMapViewMetalRenderableResource(MLNMapViewMetalImpl& backend_)
      : backend(backend_), delegate([[MLNMapViewImplDelegate alloc] initWithImpl:&backend]) {}

  void bind() override {
    if (!commandQueue) {
      commandQueue = (__bridge id<MTLCommandQueue>)backend.getCommandQueue().get();
    }

    if (!commandBuffer) {
      commandBuffer = [commandQueue commandBuffer];
      commandBufferPtr = NS::RetainPtr((__bridge MTL::CommandBuffer*)commandBuffer);
      // 142 a new frame: the last one's GPU time if it has finished, no wait yet
      auto& stats = backend.getContext().renderingStats();
      stats.drawableWaitTime = 0.0;
      if (lastCommitted && lastCommitted.status == MTLCommandBufferStatusCompleted) {
        stats.gpuTime = lastCommitted.GPUEndTime - lastCommitted.GPUStartTime;
        lastCommitted = nil;
      }
    }
  }

  const mln::mtl::RendererBackend& getBackend() const override { return backend; }

  const mln::mtl::MTLCommandBufferPtr& getCommandBuffer() const override {
    return commandBufferPtr;
  }

  virtual mln::mtl::MTLBlitPassDescriptorPtr getUploadPassDescriptor() const override {
    // Create from render pass descriptor?
    return NS::TransferPtr(MTL::BlitPassDescriptor::alloc()->init());
  }

  const mln::mtl::MTLRenderPassDescriptorPtr& getRenderPassDescriptor() const override {
    if (!cachedRenderPassDescriptor) {
      // 142 MTKView blocks here until a drawable is free
      const auto waitStart = mln::util::MonotonicTimer::now();
      auto* mtlDesc = mtlView.currentRenderPassDescriptor;
      backend.getContext().renderingStats().drawableWaitTime =
          (mln::util::MonotonicTimer::now() - waitStart).count();
      cachedRenderPassDescriptor = NS::RetainPtr((__bridge MTL::RenderPassDescriptor*)mtlDesc);
    }
    return cachedRenderPassDescriptor;
  }

  void swap() override {
    id<CAMetalDrawable> currentDrawable = [mtlView currentDrawable];
    if (currentDrawable) {
      if (presentsWithTransaction) {
        [commandBuffer commit];
        [commandBuffer waitUntilScheduled];
        [currentDrawable present];
      } else {
        [commandBuffer presentDrawable:currentDrawable];
        [commandBuffer commit];
      }
    }

    if (currentDrawable) {
      lastCommitted = commandBuffer;
    }
    commandBuffer = nil;
    commandBufferPtr.reset();

    cachedRenderPassDescriptor.reset();
  }

  mln::Size framebufferSize() {
    assert(mtlView);
    return {static_cast<uint32_t>(mtlView.drawableSize.width),
            static_cast<uint32_t>(mtlView.drawableSize.height)};
  }

private:
  MLNMapViewMetalImpl& backend;
  mln::mtl::MTLCommandBufferPtr commandBufferPtr;
  mutable mln::mtl::MTLRenderPassDescriptorPtr cachedRenderPassDescriptor;

public:
  MLNMapViewImplDelegate* delegate = nil;
  MTKView* mtlView = nil;
  id<MTLCommandBuffer> commandBuffer;
  /// 142 the frame last handed to the GPU, read for its time once it has finished
  id<MTLCommandBuffer> lastCommitted;
  id<MTLCommandQueue> commandQueue;
  bool presentsWithTransaction = false;

  // Cached last-applied values comparing against MTKView's reflected state round-trips
  // through UIKit and can defeat the no-op guard under non-integer scale factors.
  CGFloat lastAppliedScaleFactor = 0;
  CGSize lastAppliedDrawableSize = CGSizeZero;

  // We count how often the context was activated/deactivated so that we can truly deactivate it
  // after the activation count drops to 0.
  NSUInteger activationCount = 0;
};

MLNMapViewMetalImpl::MLNMapViewMetalImpl(MLNMapView* nativeView_)
    : MLNMapViewImpl(nativeView_),
      mln::mtl::RendererBackend(mln::gfx::ContextMode::Unique),
      mln::gfx::Renderable({0, 0}, std::make_unique<MLNMapViewMetalRenderableResource>(*this)) {}

MLNMapViewMetalImpl::~MLNMapViewMetalImpl() = default;

void MLNMapViewMetalImpl::setOpaque(const bool opaque) {
  auto& resource = getResource<MLNMapViewMetalRenderableResource>();
  resource.mtlView.opaque = opaque;
  resource.mtlView.layer.opaque = opaque;
}

void MLNMapViewMetalImpl::setPresentsWithTransaction(const bool value) {
  auto& resource = getResource<MLNMapViewMetalRenderableResource>();
  resource.presentsWithTransaction = value;

  if (CAMetalLayer* metalLayer = MLN_OBJC_DYNAMIC_CAST(resource.mtlView.layer, CAMetalLayer)) {
    metalLayer.presentsWithTransaction = value;
  }
}

void MLNMapViewMetalImpl::display() {
  auto& resource = getResource<MLNMapViewMetalRenderableResource>();

  // Calling `display` here directly causes the stuttering bug (if
  // `presentsWithTransaction` is `YES` - see above)
  // as reported in https://github.com/mapbox/mapbox-gl-native-ios/issues/350
  //
  // Since we use `presentsWithTransaction` to synchronize with UIView
  // annotations, we now let the system handle when the view is rendered. This
  // has the potential to increase latency
  [resource.mtlView setNeedsDisplay];
}

void MLNMapViewMetalImpl::createView() {
  auto& resource = getResource<MLNMapViewMetalRenderableResource>();
  if (resource.mtlView) {
    return;
  }

  id<MTLDevice> device = (__bridge id<MTLDevice>)resource.getBackend().getDevice().get();
  const auto scaleFactor = MLNEffectiveScaleFactorForView(mapView);

  resource.mtlView = [[MTKView alloc] initWithFrame:mapView.bounds device:device];
  resource.mtlView.delegate = resource.delegate;
  resource.mtlView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  // We own drawable sizing in layoutChanged(); disable MTKView's automatic recompute so
  // setting contentScaleFactor cannot race with explicit drawableSize assignment.
  resource.mtlView.autoResizeDrawable = NO;
  resource.mtlView.contentScaleFactor = scaleFactor;
  resource.mtlView.contentMode = UIViewContentModeCenter;
  resource.mtlView.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
  resource.mtlView.depthStencilPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
  resource.mtlView.opaque = mapView.opaque;
  resource.mtlView.layer.opaque = mapView.opaque;
  resource.mtlView.enableSetNeedsDisplay = YES;
  CAMetalLayer* metalLayer = MLN_OBJC_DYNAMIC_CAST(resource.mtlView.layer, CAMetalLayer);
  metalLayer.presentsWithTransaction = resource.presentsWithTransaction;

  [mapView insertSubview:resource.mtlView atIndex:0];
}

UIView* MLNMapViewMetalImpl::getView() {
  auto& resource = getResource<MLNMapViewMetalRenderableResource>();
  return resource.mtlView;
}

void MLNMapViewMetalImpl::deleteView() {
  auto& resource = getResource<MLNMapViewMetalRenderableResource>();
  [resource.mtlView releaseDrawables];
}

void MLNMapViewMetalImpl::activate() {
  auto& resource = getResource<MLNMapViewMetalRenderableResource>();
  if (resource.activationCount++) {
    return;
  }
}

void MLNMapViewMetalImpl::deactivate() {
  auto& resource = getResource<MLNMapViewMetalRenderableResource>();
  if (--resource.activationCount) {
    return;
  }
}

UIImage* MLNMapViewMetalImpl::snapshot() {
  auto& resource = getResource<MLNMapViewMetalRenderableResource>();
  return nil;  // TODO: resource.mtlView.snapshot;
}

void MLNMapViewMetalImpl::layoutChanged() {
  // Fix: unconditionally rewriting contentScaleFactor/drawableSize every
  // layout pass caused a feedback loop under iOS 26 Smart Display Zoom (non-integer scale).
  // Also skip pre-layout/detached passes — MLNEffectiveScaleFactorForView falls back to
  // mainScreen without a window, wrong for CarPlay.
  auto& resource = getResource<MLNMapViewMetalRenderableResource>();
  const auto viewSize = mapView.bounds.size;
  if (!resource.mtlView || viewSize.width <= 0 || viewSize.height <= 0 || !mapView.window) {
    return;
  }

  const auto scaleFactor = MLNEffectiveScaleFactorForView(mapView);
  const CGSize target = CGSizeMake(std::round(viewSize.width * scaleFactor),
                                   std::round(viewSize.height * scaleFactor));

  if (scaleFactor != resource.lastAppliedScaleFactor) {
    resource.mtlView.contentScaleFactor = resource.lastAppliedScaleFactor = scaleFactor;
  }
  if (!CGSizeEqualToSize(target, resource.lastAppliedDrawableSize)) {
    resource.mtlView.drawableSize = resource.lastAppliedDrawableSize = target;
  }

  setRenderableSize({static_cast<uint32_t>(target.width), static_cast<uint32_t>(target.height)});
}

MLNBackendResource* MLNMapViewMetalImpl::getObject() {
  auto& resource = getResource<MLNMapViewMetalRenderableResource>();
  auto renderPassDescriptor = resource.getRenderPassDescriptor().get();

  return [[MLNBackendResource alloc] initWithMTKView:resource.mtlView
                                              device:resource.mtlView.device
                                renderPassDescriptor:[MTLRenderPassDescriptor renderPassDescriptor]
                                       commandBuffer:resource.commandBuffer];
}
