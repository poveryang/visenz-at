#ifndef AT_CORE_CANDIDATE_POOL_H
#define AT_CORE_CANDIDATE_POOL_H

#include "at/at_trace.h"

namespace at {

CandidateRecord MakeCandidate(const FrameInput &input,
                              const cv::Rect &roi,
                              const ImageQuality &quality,
                              SceneDifficulty difficulty,
                              StepPhase phase,
                              TuneAction action);

void UpdateBestCandidate(const CandidateRecord &candidate,
                         std::optional<CandidateRecord> &best_candidate);

} // namespace at

#endif // AT_CORE_CANDIDATE_POOL_H
