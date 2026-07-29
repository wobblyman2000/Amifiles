#include <sidplayfp/sidplayfp.h>
#include <sidplayfp/SidTune.h>
#include <sidplayfp/SidConfig.h>
#include <sidplayfp/builders/residfp.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <string>

struct WavHeader {
    char chunkId[4] = {'R', 'I', 'F', 'F'};
    uint32_t chunkSize = 0;
    char format[4] = {'W', 'A', 'V', 'E'};
    char subchunk1Id[4] = {'f', 'm', 't', ' '};
    uint32_t subchunk1Size = 16;
    uint16_t audioFormat = 1; // PCM
    uint16_t numChannels = 2; // Stereo
    uint32_t sampleRate = 44100;
    uint32_t byteRate = 44100 * 2 * 2;
    uint16_t blockAlign = 4;
    uint16_t bitsPerSample = 16;
    char subchunk2Id[4] = {'d', 'a', 't', 'a'};
    uint32_t subchunk2Size = 0;
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input.sid> <output.wav> [duration_seconds]" << std::endl;
        return 1;
    }
    
    const char* inputPath = argv[1];
    const char* outputPath = argv[2];
    int duration = (argc >= 4) ? std::stoi(argv[3]) : 180; // default 3 minutes
    
    // Load tune
    SidTune tune(inputPath);
    if (!tune.getStatus()) {
        std::cerr << "Failed to load SID tune: " << tune.statusString() << std::endl;
        return 1;
    }
    
    // Select default song
    tune.selectSong(0);
    
    // Setup player
    sidplayfp player;
    
    // Setup builder
    ReSIDfpBuilder resid("residfp");
    
    // Configure player
    SidConfig cfg = player.config();
    cfg.frequency = 44100;
    cfg.sidEmulation = &resid;
    cfg.samplingMethod = SidConfig::RESAMPLE_INTERPOLATE;
    cfg.defaultSidModel = SidConfig::MOS6581;
    cfg.forceSidModel = false;
    
    if (!player.config(cfg)) {
        std::cerr << "Player configuration failed: " << player.error() << std::endl;
        return 1;
    }
    
    // Load tune to player
    if (!player.load(&tune)) {
        std::cerr << "Failed to load tune into player: " << player.error() << std::endl;
        return 1;
    }
    
    // Initialize mixer (stereo)
    player.initMixer(true);
    
    // Output WAV file
    std::ofstream out(outputPath, std::ios::binary);
    if (!out) {
        std::cerr << "Failed to open output file: " << outputPath << std::endl;
        return 1;
    }
    
    // Placeholder WAV header
    WavHeader header;
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    
    // Render loop
    uint32_t totalSamples = 0;
    uint32_t sampleRate = 44100;
    uint32_t targetSamples = duration * sampleRate;
    
    // Buffer size
    // Under PAL, C64 speed is 985248 cycles/second.
    // Each C64 frame runs for roughly 19704 cycles (50 Hz).
    unsigned int cyclesPerChunk = 19704;
    std::vector<short> outputBuffer(4096 * 2);
    
    while (totalSamples < targetSamples) {
        int played = player.play(cyclesPerChunk);
        if (played < 0) {
            std::cerr << "Emulation error: " << player.error() << std::endl;
            break;
        }
        if (played == 0) {
            played = 900; // fallback
        }
        
        unsigned int mixed = player.mix(outputBuffer.data(), played);
        if (mixed == 0) break;
        
        out.write(reinterpret_cast<const char*>(outputBuffer.data()), mixed * sizeof(short));
        totalSamples += (mixed / 2); // 2 channels
    }
    
    // Write correct sizes to header
    uint32_t dataSize = totalSamples * 2 * sizeof(short); // Stereo, 16-bit
    header.subchunk2Size = dataSize;
    header.chunkSize = 36 + dataSize;
    
    out.seekp(0);
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    out.close();
    
    std::cout << "Successfully rendered " << duration << " seconds of SID to WAV." << std::endl;
    return 0;
}
