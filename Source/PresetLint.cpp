// VaporKeyPresetLint - validates Source/Presets.json against the parameter
// registry exposed by Parameters.h. Run as a CMake target (or directly from
// CI) to catch:
//   - missing/empty preset name or category
//   - duplicate preset names
//   - unknown category strings
//   - unknown parameter ids
//   - values outside the parameter's declared range / choice index space
//
// Exits 0 on success, 1 on validation failure, 2 on usage / I/O failure.

#include "Parameters.h"

#include <iostream>

namespace
{

bool isNumericVar (const juce::var& v) noexcept
{
    return v.isInt() || v.isInt64() || v.isDouble();
}

bool isWholeNumber (double d) noexcept
{
    return d == (double) (long long) d;
}

bool validateValue (const Parameters::Spec& s,
                    const juce::var& v,
                    juce::String& outMessage)
{
    using Kind = Parameters::Spec::Kind;
    switch (s.kind)
    {
        case Kind::Float:
        {
            if (! isNumericVar (v))
            {
                outMessage = "expected a number";
                return false;
            }
            const float fv = (float) (double) v;
            const float lo = s.floatRange.start;
            const float hi = s.floatRange.end;
            if (fv < lo || fv > hi)
            {
                outMessage = "value " + juce::String (fv, 6)
                             + " outside [" + juce::String (lo, 6)
                             + ", " + juce::String (hi, 6) + "]";
                return false;
            }
            return true;
        }

        case Kind::Int:
        {
            if (! isNumericVar (v))
            {
                outMessage = "expected an integer";
                return false;
            }
            const double dv = (double) v;
            if (! isWholeNumber (dv))
            {
                outMessage = "value " + juce::String (dv) + " is not an integer";
                return false;
            }
            const int iv = (int) dv;
            if (iv < s.intMin || iv > s.intMax)
            {
                outMessage = "value " + juce::String (iv)
                             + " outside [" + juce::String (s.intMin)
                             + ", " + juce::String (s.intMax) + "]";
                return false;
            }
            return true;
        }

        case Kind::Bool:
        {
            if (v.isBool()) return true;
            if (isNumericVar (v))
            {
                const double dv = (double) v;
                if (dv == 0.0 || dv == 1.0) return true;
            }
            outMessage = "expected 0/1 or false/true";
            return false;
        }

        case Kind::Choice:
        {
            if (! isNumericVar (v))
            {
                outMessage = "expected a choice index (integer)";
                return false;
            }
            const double dv = (double) v;
            if (! isWholeNumber (dv))
            {
                outMessage = "choice value " + juce::String (dv)
                             + " is not an integer index";
                return false;
            }
            const int iv = (int) dv;
            if (iv < 0 || iv >= s.choices.size())
            {
                outMessage = "choice index " + juce::String (iv)
                             + " outside [0, "
                             + juce::String (s.choices.size() - 1) + "]";
                return false;
            }
            return true;
        }
    }

    outMessage = "unknown parameter kind";
    return false;
}

void usage (const char* prog)
{
    std::cerr << "Usage: " << prog << " <path-to-Presets.json>\n";
}

} // namespace

int main (int argc, char** argv)
{
    if (argc != 2)
    {
        usage (argc > 0 ? argv[0] : "VaporKeyPresetLint");
        return 2;
    }

    const juce::File jsonFile { juce::String::fromUTF8 (argv[1]) };
    if (! jsonFile.existsAsFile())
    {
        std::cerr << "Cannot read " << jsonFile.getFullPathName().toRawUTF8() << "\n";
        return 2;
    }

    const auto raw = jsonFile.loadFileAsString();
    juce::var root = juce::JSON::parse (raw);
    if (! root.isObject())
    {
        std::cerr << jsonFile.getFileName().toRawUTF8()
                  << ": top-level JSON must be an object\n";
        return 2;
    }

    juce::StringArray declaredCategories;
    {
        const juce::var catsVar = root.getProperty ("categories", juce::var());
        if (auto* a = catsVar.getArray())
            for (const auto& c : *a)
                declaredCategories.add (c.toString());
    }
    if (declaredCategories.isEmpty())
    {
        std::cerr << jsonFile.getFileName().toRawUTF8()
                  << ": top-level \"categories\" array is missing or empty\n";
        return 2;
    }

    const juce::var presetsVar = root.getProperty ("presets", juce::var());
    auto* presets = presetsVar.getArray();
    if (presets == nullptr)
    {
        std::cerr << jsonFile.getFileName().toRawUTF8()
                  << ": top-level \"presets\" array missing\n";
        return 2;
    }

    juce::StringArray errors;
    juce::StringArray seenNames;
    int validatedPresets = 0;

    for (int presetIndex = 0; presetIndex < presets->size(); ++presetIndex)
    {
        const juce::var& item = (*presets).getReference (presetIndex);
        const juce::String location = "preset[" + juce::String (presetIndex) + "]";

        if (! item.isObject())
        {
            errors.add (location + ": not an object");
            continue;
        }

        const juce::String name     = item.getProperty ("name", "").toString();
        const juce::String category = item.getProperty ("category", "").toString();
        const juce::var    values   = item.getProperty ("values", juce::var());

        const juce::String tag = name.isNotEmpty()
            ? location + " (\"" + name + "\")"
            : location;

        if (name.isEmpty())     errors.add (location + ": missing or empty \"name\"");
        if (category.isEmpty()) errors.add (tag + ": missing or empty \"category\"");

        if (name.isNotEmpty())
        {
            if (seenNames.contains (name))
                errors.add (tag + ": duplicate preset name");
            else
                seenNames.add (name);
        }

        if (category.isNotEmpty() && ! declaredCategories.contains (category, true))
            errors.add (tag + ": unknown category \"" + category + "\"");

        if (! values.isObject())
        {
            errors.add (tag + ": missing or non-object \"values\"");
            continue;
        }

        auto* obj = values.getDynamicObject();
        if (obj == nullptr) continue;

        const auto& props = obj->getProperties();
        for (int i = 0; i < props.size(); ++i)
        {
            const juce::String key = props.getName (i).toString();
            const juce::var&   val = props.getValueAt (i);

            const auto* spec = Parameters::findSpec (key);
            if (spec == nullptr)
            {
                errors.add (tag + ": unknown parameter id \"" + key + "\"");
                continue;
            }

            juce::String why;
            if (! validateValue (*spec, val, why))
                errors.add (tag + ": \"" + key + "\": " + why);
        }

        ++validatedPresets;
    }

    if (! errors.isEmpty())
    {
        std::cerr << "VaporKey preset lint failed with " << errors.size()
                  << " error(s) in " << jsonFile.getFileName().toRawUTF8() << ":\n";
        for (const auto& e : errors)
            std::cerr << "  - " << e.toRawUTF8() << "\n";
        return 1;
    }

    std::cout << "VaporKey preset lint OK: validated " << validatedPresets
              << " presets across " << declaredCategories.size()
              << " categories against " << Parameters::allSpecs().size()
              << " registered parameters\n";
    return 0;
}
