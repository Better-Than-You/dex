#include "provider/SpscRingBuffer.hpp"

#include <QtTest>

#include <vector>

using dex::SpscRingBuffer;

class TestAudio : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip();
    void wrapsAround();
    void dropsWhenFull();
    void resetClears();
};

void TestAudio::roundTrip()
{
    SpscRingBuffer<float, 16> ring;
    const float input[] = {1.0f, 2.0f, 3.0f, 4.0f};
    QCOMPARE(ring.push(input, 4), std::size_t(4));
    QCOMPARE(ring.available(), std::size_t(4));

    float output[4] = {};
    QCOMPARE(ring.pop(output, 4), std::size_t(4));
    QCOMPARE(output[0], 1.0f);
    QCOMPARE(output[3], 4.0f);
    QCOMPARE(ring.available(), std::size_t(0));
}

void TestAudio::wrapsAround()
{
    SpscRingBuffer<float, 8> ring;
    std::vector<float> input(6, 1.0f);
    QCOMPARE(ring.push(input.data(), input.size()), input.size());

    std::vector<float> output(4, 0.0f);
    QCOMPARE(ring.pop(output.data(), output.size()), output.size());

    const float more[] = {2.0f, 2.0f, 2.0f, 2.0f};
    QCOMPARE(ring.push(more, 4), std::size_t(4)); // wraps past the end

    std::vector<float> rest(6, 0.0f);
    QCOMPARE(ring.pop(rest.data(), rest.size()), rest.size());
    QCOMPARE(rest[0], 1.0f);
    QCOMPARE(rest[1], 1.0f);
    QCOMPARE(rest[2], 2.0f);
    QCOMPARE(rest[5], 2.0f);
}

void TestAudio::dropsWhenFull()
{
    SpscRingBuffer<float, 4> ring;
    const float input[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    QCOMPARE(ring.push(input, 6), std::size_t(4));
    QCOMPARE(ring.available(), std::size_t(4));
    QCOMPARE(ring.dropped(), std::size_t(2));
}

void TestAudio::resetClears()
{
    SpscRingBuffer<float, 4> ring;
    const float input[] = {1.0f, 2.0f};
    ring.push(input, 2);
    ring.reset();
    QCOMPARE(ring.available(), std::size_t(0));
    QCOMPARE(ring.dropped(), std::size_t(0));
}

QTEST_APPLESS_MAIN(TestAudio)
#include "tst_audio.moc"
