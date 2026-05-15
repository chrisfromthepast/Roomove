#include <juce_core/juce_core.h>

#include "../dsp/RoomoveDSP.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
    struct FunctionBody
    {
        std::string name;
        std::string body;
    };

    bool assertCondition(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << std::endl;
            return false;
        }
        return true;
    }

    bool readTextFile(const std::string& path, std::string& content)
    {
        std::ifstream sourceFile(path.c_str(), std::ios::in | std::ios::binary);
        if (!sourceFile.is_open())
            return false;

        content.assign((std::istreambuf_iterator<char>(sourceFile)), std::istreambuf_iterator<char>());
        return true;
    }

    std::size_t findMatchingBrace(const std::string& text, std::size_t openBrace)
    {
        int depth = 0;
        for (std::size_t index = openBrace; index < text.size(); ++index)
        {
            if (text[index] == '{')
                ++depth;
            else if (text[index] == '}')
            {
                --depth;
                if (depth == 0)
                    return index;
            }
        }

        return std::string::npos;
    }

    bool extractFunctionBody(const std::string& text, const std::string& token, FunctionBody& result)
    {
        const std::size_t tokenPosition = text.find(token);
        if (tokenPosition == std::string::npos)
            return false;

        const std::size_t openBrace = text.find('{', tokenPosition);
        if (openBrace == std::string::npos)
            return false;

        const std::size_t closeBrace = findMatchingBrace(text, openBrace);
        if (closeBrace == std::string::npos)
            return false;

        result.name = token;
        result.body = text.substr(openBrace + 1, closeBrace - openBrace - 1);
        return true;
    }

    bool assertNoForbiddenTokens(
        const FunctionBody& function,
        const std::vector<std::pair<std::string, std::string>>& forbidden,
        const std::string& context)
    {
        bool ok = true;
        for (const auto& entry : forbidden)
        {
            if (function.body.find(entry.first) != std::string::npos)
                ok = assertCondition(false, function.name + " contains " + entry.second + " in " + context) && ok;
        }
        return ok;
    }

    bool testProcessBlockGuardrails()
    {
        const std::string sourcePath = std::string(ROOMOVE_REPO_ROOT) + "/app/PluginProcessor.cpp";
        std::string content;
        if (!readTextFile(sourcePath, content))
            return assertCondition(false, "Unable to open PluginProcessor.cpp for realtime guardrail checks");

        FunctionBody processBlock;
        if (!extractFunctionBody(content, "ArmorAudioProcessor::processBlock", processBlock))
            return assertCondition(false, "processBlock was not found in PluginProcessor.cpp");

        const std::vector<std::pair<std::string, std::string>> forbiddenAllocations{
            { "new ", "dynamic allocation" },
            { ".push_back", "container growth" },
            { ".emplace_back", "container growth" },
            { ".resize", "container resize" },
            { ".reserve", "container reserve" },
            { "malloc(", "malloc" },
            { "calloc(", "calloc" },
            { "realloc(", "realloc" },
            { "free(", "free" },
        };
        const std::vector<std::pair<std::string, std::string>> forbiddenLocks{
            { "std::mutex", "std::mutex" },
            { "std::lock_guard", "std::lock_guard" },
            { "std::unique_lock", "std::unique_lock" },
            { "std::scoped_lock", "std::scoped_lock" },
            { "juce::CriticalSection", "juce::CriticalSection" },
            { "juce::SpinLock", "juce::SpinLock" },
            { "juce::ScopedLock", "juce::ScopedLock" },
        };
        const std::vector<std::pair<std::string, std::string>> forbiddenBlocking{
            { "sleep_for(", "sleep call" },
            { "sleep_until(", "sleep call" },
            { ".wait(", "wait call" },
            { "MessageManagerLock", "message-manager blocking lock" },
            { ".join(", "thread join" },
        };

        bool ok = true;
        ok = assertCondition(
                 processBlock.body.find("juce::ScopedNoDenormals") != std::string::npos,
                 "processBlock does not contain juce::ScopedNoDenormals")
             && ok;
        ok = assertNoForbiddenTokens(processBlock, forbiddenAllocations, "processBlock") && ok;
        ok = assertNoForbiddenTokens(processBlock, forbiddenLocks, "processBlock") && ok;
        ok = assertNoForbiddenTokens(processBlock, forbiddenBlocking, "processBlock") && ok;
        return ok;
    }

    bool testPrepareToPlayOwnsWarmupAllocation()
    {
        const std::string sourcePath = std::string(ROOMOVE_REPO_ROOT) + "/app/PluginProcessor.cpp";
        std::string content;
        if (!readTextFile(sourcePath, content))
            return assertCondition(false, "Unable to open PluginProcessor.cpp for warmup checks");

        FunctionBody prepareToPlay;
        FunctionBody processBlock;
        if (!extractFunctionBody(content, "ArmorAudioProcessor::prepareToPlay", prepareToPlay))
            return assertCondition(false, "prepareToPlay was not found in PluginProcessor.cpp");
        if (!extractFunctionBody(content, "ArmorAudioProcessor::processBlock", processBlock))
            return assertCondition(false, "processBlock was not found in PluginProcessor.cpp");

        bool ok = true;
        ok = assertCondition(
                 prepareToPlay.body.find("dspStates.resize") != std::string::npos,
                 "prepareToPlay should size dspStates during warmup")
             && ok;
        ok = assertCondition(
                 processBlock.body.find("dspStates.resize") == std::string::npos,
                 "processBlock must not resize dspStates after warmup")
             && ok;
        return ok;
    }

    bool testDspProcessFunctionsAvoidRealtimeHazards()
    {
        const std::string sourcePath = std::string(ROOMOVE_REPO_ROOT) + "/dsp/RoomoveDSP.cpp";
        std::string content;
        if (!readTextFile(sourcePath, content))
            return assertCondition(false, "Unable to open RoomoveDSP.cpp for realtime guardrail checks");

        const std::vector<std::pair<std::string, std::string>> forbiddenAllocations{
            { "new ", "dynamic allocation" },
            { ".push_back", "container growth" },
            { ".emplace_back", "container growth" },
            { ".resize", "container resize" },
            { ".reserve", "container reserve" },
            { "malloc(", "malloc" },
            { "calloc(", "calloc" },
            { "realloc(", "realloc" },
            { "free(", "free" },
        };
        const std::vector<std::pair<std::string, std::string>> forbiddenLocks{
            { "std::mutex", "std::mutex" },
            { "std::lock_guard", "std::lock_guard" },
            { "std::unique_lock", "std::unique_lock" },
            { "std::scoped_lock", "std::scoped_lock" },
            { "juce::CriticalSection", "juce::CriticalSection" },
            { "juce::SpinLock", "juce::SpinLock" },
            { "juce::ScopedLock", "juce::ScopedLock" },
        };
        const std::vector<std::pair<std::string, std::string>> forbiddenBlocking{
            { "sleep_for(", "sleep call" },
            { "sleep_until(", "sleep call" },
            { ".wait(", "wait call" },
            { "MessageManagerLock", "message-manager blocking lock" },
            { ".join(", "thread join" },
        };

        bool ok = true;
        for (const auto& functionName : { "roomoveDspStateProcessAudioNoAlias", "roomoveDspStateProcessAudio" })
        {
            FunctionBody function;
            if (!extractFunctionBody(content, functionName, function))
                return assertCondition(false, std::string("Function not found: ") + functionName);

            ok = assertNoForbiddenTokens(function, forbiddenAllocations, functionName) && ok;
            ok = assertNoForbiddenTokens(function, forbiddenLocks, functionName) && ok;
            ok = assertNoForbiddenTokens(function, forbiddenBlocking, functionName) && ok;
        }

        return ok;
    }
}

int main()
{
    bool ok = true;
    ok = testProcessBlockGuardrails() && ok;
    ok = testPrepareToPlayOwnsWarmupAllocation() && ok;
    ok = testDspProcessFunctionsAvoidRealtimeHazards() && ok;

    if (!ok)
        return 1;

    std::cout << "PASS: all Roomove real-time validation tests succeeded." << std::endl;
    return 0;
}
