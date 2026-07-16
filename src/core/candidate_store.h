#ifndef AT_CORE_CANDIDATE_STORE_H
#define AT_CORE_CANDIDATE_STORE_H

#include "at_trace.h"
#include "at_types.h"

namespace at {

CandidateRecord MakeCandidate(const FrameContext &context,
                              const cv::Rect &roi,
                              const ImageQuality &quality,
                              StepPhase phase,
                              TuneAction action,
                              const FlowConfig &flow);

void UpdateBestCandidate(const CandidateRecord &candidate,
                         std::optional<CandidateRecord> &best_candidate);

} // namespace at

#endif // AT_CORE_CANDIDATE_STORE_H
