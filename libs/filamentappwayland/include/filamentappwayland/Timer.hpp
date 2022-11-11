#pragma once

#include <chrono>
#include <algorithm>

template<typename precision = double, typename ratio = std::micro>
class Timer final {
public:
    using timeDuration_t = std::chrono::duration<precision, ratio>;
    using timePoint_t = std::chrono::time_point<std::chrono::high_resolution_clock, timeDuration_t>;
    using this_type = Timer<precision, ratio>;

    inline void Start() {
        m_start = std::chrono::high_resolution_clock::now();
    }

    inline void End() {
        m_end = std::chrono::high_resolution_clock::now();
    }

    void CalcDuration() {
        m_duration = std::max(m_start, m_end) - std::min(m_start, m_end);
    }

    timeDuration_t const &GetDuration() const {
        return m_duration;
    }

    void Zero() {
        m_start = timeDuration_t::zero();
        m_end = m_start;
        m_duration = m_end;
    }

    static timeDuration_t TestLatency(size_t const i_count = 512) {
        this_type t;
        timeDuration_t tSum = timeDuration_t::duration::zero();
        for (size_t i = 0; i < i_count; ++i) {
            t.Start();
            t.End();
            t.CalcDuration();
            tSum += t.GetDuration();
        }
        return tSum / i_count;
    }

private:
    timePoint_t m_start;
    timePoint_t m_end;
    timeDuration_t m_duration;
};

using Timer_t = Timer<>;
