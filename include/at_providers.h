#ifndef AT_AT_PROVIDERS_H
#define AT_AT_PROVIDERS_H

#include "at_types.h"

namespace at {

class HeatmapProvider {
public:
    virtual ~HeatmapProvider() = default;
    virtual HeatmapObservation Infer(const FrameContext &context) = 0;
};

class DecodeProvider {
public:
    virtual ~DecodeProvider() = default;
    virtual DecodeFeedback Decode(const FrameContext &context) = 0;
};

class PreprocessPlugin {
public:
    virtual ~PreprocessPlugin() = default;
    virtual PreprocessReport Apply(FrameContext &context) = 0;
};

class NullHeatmapProvider final : public HeatmapProvider {
public:
    HeatmapObservation Infer(const FrameContext &) override;
};

class NullDecodeProvider final : public DecodeProvider {
public:
    DecodeFeedback Decode(const FrameContext &) override;
};

class SimplePreprocessPlugin final : public PreprocessPlugin {
public:
    PreprocessReport Apply(FrameContext &context) override;
};

} // namespace at

#endif // AT_AT_PROVIDERS_H
