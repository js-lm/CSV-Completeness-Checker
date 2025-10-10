#pragma once

#include <chrono>

namespace Constants{

    constexpr const char *Title{"CSV-Completeness-Checker"};
    constexpr const char *Version{PROJECT_VERSION};

    constexpr const char *DefaultBaseJsonName{"csv_completeness_checker"};

    constexpr std::chrono::seconds ProgressUpdateInterval{1};
    constexpr std::chrono::milliseconds ProgressMinimumPollInterval{50};
    constexpr std::chrono::milliseconds ProgressDefaultPollInterval{250};

} // namespace Constants