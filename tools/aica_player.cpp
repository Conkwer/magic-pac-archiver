#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <string>
#include <cstdlib>
#include <filesystem>
#include <cstdio>
#include <sstream>
#include <windows.h>
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

namespace fs = std::filesystem;

bool fileExists(const std::string& filename) {
    std::ifstream file(filename);
    return file.good();
}

std::pair<int, int> readConfig(const std::string& configFile) {
    std::ifstream file(configFile);
    std::string line;
    int freq = 22050; // default value
    int header = 2048; // default value

    while (std::getline(file, line)) {
        std::istringstream is_line(line);
        std::string key;
        if (std::getline(is_line, key, '=')) {
            std::string value;
            if (std::getline(is_line, value)) {
                if (key == "freq") {
                    freq = std::stoi(value);
                } else if (key == "header") {
                    header = std::stoi(value);
                }
            }
        }
    }
    return {freq, header};
}

void playWav(const std::string& filename) {
    ma_result result;
    ma_engine engine;
    ma_sound sound;

    result = ma_engine_init(NULL, &engine);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to initialize audio engine. Error code: " << result << std::endl;
        return;
    }

    result = ma_sound_init_from_file(&engine, filename.c_str(), 0, NULL, NULL, &sound);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to load sound file. Error code: " << result << std::endl;
        ma_engine_uninit(&engine);
        return;
    }

    ma_sound_start(&sound);

    while (!ma_sound_at_end(&sound)) {
        ma_sleep(100); // Sleep for a short duration to avoid busy-waiting
    }

    ma_sound_uninit(&sound);
    ma_engine_uninit(&engine);
}

std::vector<int> diff_lookup = {
    1, 3, 5, 7, 9, 11, 13, 15,
    -1, -3, -5, -7, -9, -11, -13, -15,
};

std::vector<int> index_scale = {
    0x0e6, 0x0e6, 0x0e6, 0x0e6, 0x133, 0x199, 0x200, 0x266
};

std::vector<uint8_t> adpcm2pcm(const std::vector<uint8_t>& data, uint32_t start, uint32_t length) {
    std::vector<uint8_t> dst(length * 4);
    uint32_t dst_loc = 0;
    int cur_quant = 0x7f;
    int cur_sample = 0;
    bool high_nybble = false;

    while (dst_loc < dst.size()) {
        int shift1 = high_nybble ? 4 : 0;
        int delta = (data[start] >> shift1) & 0xf;

        int x = cur_quant * diff_lookup[delta & 15];
        x = cur_sample + ((x + (x >> 29)) >> 3);
        cur_sample = std::max(-32768, std::min(32767, x));
        cur_quant = (cur_quant * index_scale[delta & 7]) >> 8;
        cur_quant = std::max(0x7f, std::min(0x6000, cur_quant));

        dst[dst_loc++] = cur_sample & 0xFF;
        dst[dst_loc++] = (cur_sample >> 8) & 0xFF;

        cur_sample = cur_sample * 254 / 256;

        high_nybble = !high_nybble;
        if (!high_nybble) {
            start++;
        }
    }

    return dst;
}

std::vector<uint8_t> add_wav_header(const std::vector<uint8_t>& data, uint32_t frequency, uint16_t bit_depth = 16) {
    std::vector<uint8_t> output(44 + data.size());
    std::memcpy(output.data(), "RIFF", 4);
    uint32_t file_size = output.size() - 8;
    std::memcpy(output.data() + 4, &file_size, 4);
    std::memcpy(output.data() + 8, "WAVE", 4);
    std::memcpy(output.data() + 12, "fmt ", 4);
    uint32_t header_size = 16;
    std::memcpy(output.data() + 16, &header_size, 4);
    uint16_t audio_format = 1;
    std::memcpy(output.data() + 20, &audio_format, 2);
    uint16_t num_channels = 1;
    std::memcpy(output.data() + 22, &num_channels, 2);
    std::memcpy(output.data() + 24, &frequency, 4);
    uint32_t byte_rate = frequency * (bit_depth / 8);
    std::memcpy(output.data() + 28, &byte_rate, 4);
    uint16_t block_align = bit_depth / 8;
    std::memcpy(output.data() + 32, &block_align, 2);
    std::memcpy(output.data() + 34, &bit_depth, 2);
    std::memcpy(output.data() + 36, "data", 4);
    uint32_t data_size = data.size();
    std::memcpy(output.data() + 40, &data_size, 4);
    std::memcpy(output.data() + 44, data.data(), data.size());

    return output;
}

std::string openFileDialog() {
    OPENFILENAME ofn;
    char szFile[260] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn) == TRUE) {
        return std::string(ofn.lpstrFile);
    } else {
        return "";
    }
}

int main(int argc, char* argv[]) {
    std::string binFile;

    if (argc != 2) {
        binFile = openFileDialog();
        if (binFile.empty()) {
            std::cerr << "No file selected. Exiting." << std::endl;
            return 1;
        }
    } else {
        binFile = argv[1];
    }

    std::string wavFile = binFile + ".wav";

    // Read config
    auto [freq, header] = readConfig("player.ini");

    // Read input file
    std::ifstream input(binFile, std::ios::binary);
    if (!input) {
        std::cerr << "Error opening input file." << std::endl;
        return 1;
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();

    uint32_t start = header;
    uint32_t length = data.size() - start;

    if (start > data.size()) {
        std::cerr << "Data range specified is larger than the input file. Can't do anything with that." << std::endl;
        return 1;
    }

    std::cout << "Processing file from offset " << start << " with length " << length << "..." << std::endl;

    std::vector<uint8_t> pcm_data = adpcm2pcm(data, start, length);
    pcm_data = add_wav_header(pcm_data, freq);

    std::ofstream output(wavFile, std::ios::binary);
    if (!output) {
        std::cerr << "Error opening output file." << std::endl;
        return 1;
    }

    output.write(reinterpret_cast<const char*>(pcm_data.data()), pcm_data.size());
    output.close();

    // Play the WAV file
    playWav(wavFile);

    return 0;
}
