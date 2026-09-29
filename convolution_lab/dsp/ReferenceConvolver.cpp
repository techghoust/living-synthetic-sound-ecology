#include "ReferenceConvolver.h"
#include <algorithm>
#include <cmath>

namespace lsse::convolution
{
void ReferenceConvolver::setImpulse (std::vector<float> impulse)
{
    if (impulse.empty()) impulse = { 1.0f };
    if (impulse.size() > 131072) impulse.resize (131072);
    for (auto& x : impulse) if (! std::isfinite (x)) x = 0.0f;
    ir = std::move (impulse); history.assign (ir.size(), 0.0f); write = 0;
}
void ReferenceConvolver::reset() { std::fill (history.begin(), history.end(), 0.0f); write = 0; }
float ReferenceConvolver::process (float input)
{
    history[write] = std::isfinite (input) ? input : 0.0f;
    double sum = 0.0; auto index = write;
    for (const auto coefficient : ir) { sum += coefficient * history[index]; index = index == 0 ? history.size() - 1 : index - 1; }
    write = (write + 1) % history.size();
    return static_cast<float> (std::clamp (sum, -16.0, 16.0));
}
std::vector<float> ReferenceConvolver::resampleLinear (const std::vector<float>& source, double sourceRate, double targetRate)
{
    if (source.empty() || sourceRate <= 0.0 || targetRate <= 0.0) return {};
    const auto count = std::max<std::size_t> (1, static_cast<std::size_t> (std::llround (source.size() * targetRate / sourceRate)));
    std::vector<float> result (count);
    for (std::size_t i = 0; i < count; ++i) { const auto p = i * sourceRate / targetRate; const auto a = std::min<std::size_t> (static_cast<std::size_t> (p), source.size() - 1); const auto b = std::min (a + 1, source.size() - 1); const auto f = p - a; result[i] = static_cast<float> (source[a] + (source[b] - source[a]) * f); }
    return result;
}
std::vector<float> ReferenceConvolver::transformImpulse (const std::vector<float>& source, float start, float end,
                                                          bool reverse, float stretch, float decay)
{
    if (source.empty()) return { 1.0f };
    const auto a = std::clamp (start, 0.0f, 1.0f), b = std::clamp (end, a, 1.0f);
    const auto first = std::min (source.size() - 1, static_cast<std::size_t> (a * source.size()));
    const auto last = std::max (first + 1, std::min (source.size(), static_cast<std::size_t> (std::ceil (b * source.size()))));
    std::vector<float> trimmed (source.begin() + static_cast<std::ptrdiff_t> (first), source.begin() + static_cast<std::ptrdiff_t> (last));
    if (reverse) std::reverse (trimmed.begin(), trimmed.end());
    const auto factor = std::clamp (stretch, 0.25f, 4.0f);
    auto result = resampleLinear (trimmed, 1.0, factor);
    const auto d = std::clamp (decay, 0.0f, 2.0f);
    for (std::size_t i = 0; i < result.size(); ++i)
    {
        const auto t = result.size() > 1 ? static_cast<float> (i) / static_cast<float> (result.size() - 1) : 0.0f;
        result[i] *= std::exp (-t * (1.0f - d) * 5.0f);
    }
    return result;
}
}
