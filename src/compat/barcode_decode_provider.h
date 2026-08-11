#pragma once

#include "ar_base.h"
#include "at_providers.h"

namespace at {
namespace compat {

/** DecodeProvider bridge over legacy BarcodeWrapperBase (non-owning). */
class BarcodeDecodeProvider final : public DecodeProvider {
public:
    explicit BarcodeDecodeProvider(BarcodeWrapperBase *wrapper);

    DecodeFeedback Decode(const FrameContext &context) override;

    const ARInfo &LastArInfo() const { return last_ar_info_; }

private:
    BarcodeWrapperBase *wrapper_{nullptr};
    mutable ARInfo last_ar_info_{};
};

}  // namespace compat
}  // namespace at
