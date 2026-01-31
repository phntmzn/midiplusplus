// midi_writer.cpp - minimal MIDI writer (no libs)
// g++ -std=c++17 -O2 midi_writer.cpp -o midi_writer
// ./midi_writer   -> creates out.mid

#include <cstdint>
#include <fstream>
#include <vector>
#include <string>

using Byte = uint8_t;

static void writeBE16(std::vector<Byte>& b, uint16_t v) {
    b.push_back((v >> 8) & 0xFF);
    b.push_back(v & 0xFF);
}
static void writeBE32(std::vector<Byte>& b, uint32_t v) {
    b.push_back((v >> 24) & 0xFF);
    b.push_back((v >> 16) & 0xFF);
    b.push_back((v >> 8) & 0xFF);
    b.push_back(v & 0xFF);
}
static void writeStr(std::vector<Byte>& b, const char* s) {
    while (*s) b.push_back((Byte)*s++);
}

// MIDI variable-length quantity (VLQ)
static void writeVLQ(std::vector<Byte>& b, uint32_t v) {
    Byte out[5];
    int n = 0;
    out[n++] = (Byte)(v & 0x7F);
    while ((v >>= 7) != 0) out[n++] = (Byte)((v & 0x7F) | 0x80);
    for (int i = n - 1; i >= 0; --i) b.push_back(out[i]);
}

// Track event helpers
static void metaEvent(std::vector<Byte>& t, uint32_t delta, Byte type, const std::vector<Byte>& data) {
    writeVLQ(t, delta);
    t.push_back(0xFF);
    t.push_back(type);
    writeVLQ(t, (uint32_t)data.size());
    t.insert(t.end(), data.begin(), data.end());
}

static void midiEvent(std::vector<Byte>& t, uint32_t delta, Byte status, Byte d1, Byte d2) {
    writeVLQ(t, delta);
    t.push_back(status);
    t.push_back(d1);
    t.push_back(d2);
}

static std::vector<Byte> makeTrackChunk(const std::vector<Byte>& trackData) {
    std::vector<Byte> chunk;
    writeStr(chunk, "MTrk");
    writeBE32(chunk, (uint32_t)trackData.size());
    chunk.insert(chunk.end(), trackData.begin(), trackData.end());
    return chunk;
}

int main() {
    // ----- MIDI header -----
    // Format 1, 2 tracks, division = 480 ticks per quarter note
    std::vector<Byte> file;
    writeStr(file, "MThd");
    writeBE32(file, 6);
    writeBE16(file, 1);
    writeBE16(file, 2);
    writeBE16(file, 480);

    // ----- Track 0: tempo + time signature -----
    std::vector<Byte> t0;

    // Tempo: 120 BPM => 500,000 microseconds per quarter note
    // Meta tempo: FF 51 03 tt tt tt
    metaEvent(t0, 0, 0x51, {0x07, 0xA1, 0x20}); // 500000

    // Time signature: 4/4 => FF 58 04 nn dd cc bb
    // dd is power of 2: 2 means 2^2 = 4
    metaEvent(t0, 0, 0x58, {0x04, 0x02, 0x18, 0x08});

    // End of track
    metaEvent(t0, 0, 0x2F, {});

    auto c0 = makeTrackChunk(t0);
    file.insert(file.end(), c0.begin(), c0.end());

    // ----- Track 1: simple melody on channel 0 -----
    std::vector<Byte> t1;

    const Byte ch = 0;                 // channel 0
    const Byte NOTE_ON  = (Byte)(0x90 | ch);
    const Byte NOTE_OFF = (Byte)(0x80 | ch);
    const uint32_t q = 480;            // quarter note in ticks
    const Byte vel = 100;

    // C major up: C4 D4 E4 F4 G4 A4 B4 C5
    int notes[] = {60, 62, 64, 65, 67, 69, 71, 72};

    // Play twice
    for (int rep = 0; rep < 2; rep++) {
        for (int n : notes) {
            midiEvent(t1, 0, NOTE_ON,  (Byte)n, vel);
            midiEvent(t1, q, NOTE_OFF, (Byte)n, 0);
        }
        // small rest (quarter)
        writeVLQ(t1, q);
        // (no event; add a harmless meta marker: sequence/track name could be used, but keep minimal)
        // Instead, do a "note off" for a note with velocity 0? Not valid without a note on.
        // We'll just do a zero-length meta text:
        t1.push_back(0xFF); t1.push_back(0x01); t1.push_back(0x00);
    }

    // End of track
    metaEvent(t1, 0, 0x2F, {});
    auto c1 = makeTrackChunk(t1);
    file.insert(file.end(), c1.begin(), c1.end());

    // ----- Write file -----
    std::ofstream out("out.mid", std::ios::binary);
    out.write(reinterpret_cast<const char*>(file.data()), (std::streamsize)file.size());
    out.close();
    return 0;
}
