#include <juce_core/juce_core.h>

#include "../dsp/RoomoveDSP.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{
    bool assertCondition(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << std::endl;
            return false;
        }
        return true;
    }

    bool testAbstractFifoHighContention()
    {
        constexpr int iterations = 20000;
        constexpr int fifoCapacity = 1024;
        constexpr std::uint64_t maxYieldIterations = 10000000ULL;
        juce::AbstractFifo fifo(fifoCapacity);
        std::vector<int> storage((size_t) fifoCapacity, -1);

        std::atomic<int> produced{0};
        std::atomic<int> consumed{0};
        std::atomic<bool> mismatch{false};
        std::atomic<bool> producerDone{false};
        std::atomic<bool> consumerDone{false};

        std::thread producer([&]()
        {
            std::uint64_t yieldIterations = 0;
            while (produced.load() < iterations)
            {
                int start1 = 0;
                int size1 = 0;
                int start2 = 0;
                int size2 = 0;
                fifo.prepareToWrite(1, start1, size1, start2, size2);
                if (size1 > 0)
                {
                    const int value = produced.fetch_add(1);
                    storage[(size_t) start1] = value;
                    fifo.finishedWrite(1);
                }
                else
                {
                    ++yieldIterations;
                    std::this_thread::yield();
                }
                if (yieldIterations > maxYieldIterations)
                {
                    mismatch.store(true);
                    break;
                }
            }
            producerDone.store(true);
        });

        std::thread consumer([&]()
        {
            std::uint64_t yieldIterations = 0;
            while (consumed.load() < iterations)
            {
                int start1 = 0;
                int size1 = 0;
                int start2 = 0;
                int size2 = 0;
                fifo.prepareToRead(1, start1, size1, start2, size2);
                if (size1 > 0)
                {
                    const int expected = consumed.fetch_add(1);
                    const int actual = storage[(size_t) start1];
                    fifo.finishedRead(1);
                    if (actual != expected)
                    {
                        mismatch.store(true);
                        break;
                    }
                }
                else
                {
                    ++yieldIterations;
                    std::this_thread::yield();
                }

                if (yieldIterations > maxYieldIterations && producerDone.load())
                {
                    mismatch.store(true);
                    break;
                }
            }
            consumerDone.store(true);
        });

        producer.join();
        consumer.join();

        return assertCondition(producerDone.load() && consumerDone.load(), "AbstractFifo threads did not complete") &&
               assertCondition(!mismatch.load(), "AbstractFifo contention test detected mismatch/deadlock");
    }

    bool testScopedNoDenormalsPresent()
    {
        const std::string sourcePath = std::string(ROOMOVE_REPO_ROOT) + "/app/PluginProcessor.cpp";
        std::ifstream sourceFile(sourcePath.c_str(), std::ios::in | std::ios::binary);
        if (!sourceFile.is_open())
            return assertCondition(false, "Unable to open PluginProcessor.cpp for denormal guard check");

        std::string content((std::istreambuf_iterator<char>(sourceFile)), std::istreambuf_iterator<char>());
        const std::size_t processBlockPos = content.find("processBlock");
        if (processBlockPos == std::string::npos)
            return assertCondition(false, "processBlock was not found in PluginProcessor.cpp");

        const std::size_t denormalPos = content.find("juce::ScopedNoDenormals", processBlockPos);
        return assertCondition(denormalPos != std::string::npos, "processBlock does not contain juce::ScopedNoDenormals");
    }

    bool testInferenceLatencyBudget()
    {
        constexpr int sampleRate = 48000;
        constexpr int blockSize = 512;
        constexpr int iterations = 1000;
        constexpr long long latencyBudgetNanoseconds = 4000000LL;

        RoomoveDspState state;
        roomoveDspStateInit(&state, (float) sampleRate);
        roomoveDspStateSetArmorStrength(&state, 1.0f);

        std::vector<float> input((size_t) blockSize, 0.01f);
        std::vector<float> output((size_t) blockSize, 0.0f);
        std::atomic<long long> maxLatencyNs{0};

        std::thread worker([&]()
        {
            for (int i = 0; i < iterations; ++i)
            {
                input[(size_t) (i % blockSize)] = (float) (0.005 * (i % 7));
                const auto start = std::chrono::high_resolution_clock::now();
                roomoveDspStateProcessAudioNoAlias(&state, input.data(), output.data(), blockSize);
                const auto end = std::chrono::high_resolution_clock::now();
                const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

                long long current = maxLatencyNs.load();
                while (elapsed > current && !maxLatencyNs.compare_exchange_weak(current, elapsed))
                {
                }
            }
        });

        worker.join();
        const long long observed = maxLatencyNs.load();
        if (!assertCondition(observed > 0, "Latency benchmark did not execute"))
            return false;

        std::cout << "Latency max ns: " << observed << std::endl;
        return assertCondition(observed < latencyBudgetNanoseconds, "Background DSP inference exceeded 4.0ms budget");
    }
}

int main()
{
    bool ok = true;
    ok = testAbstractFifoHighContention() && ok;
    ok = testScopedNoDenormalsPresent() && ok;
    ok = testInferenceLatencyBudget() && ok;

    if (!ok)
        return 1;

    std::cout << "PASS: all Roomove real-time validation tests succeeded." << std::endl;
    return 0;
}
