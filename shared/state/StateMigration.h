#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace lsse::state
{
inline juce::ValueTree mergeParameterState (const juce::ValueTree& defaults,
                                             const juce::ValueTree& restored)
{
    auto merged = defaults.createCopy();
    for (int property = 0; property < restored.getNumProperties(); ++property)
    {
        const auto name = restored.getPropertyName (property);
        merged.setProperty (name, restored.getProperty (name), nullptr);
    }
    for (int index = 0; index < restored.getNumChildren(); ++index)
    {
        const auto source = restored.getChild (index);
        if (! source.hasProperty ("id"))
            continue;
        auto destination = merged.getChildWithProperty ("id", source.getProperty ("id"));
        if (destination.isValid() && source.hasProperty ("value"))
            destination.setProperty ("value", source.getProperty ("value"), nullptr);
    }
    return merged;
}
}
