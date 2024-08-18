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
    std::string in_file;
    std::string out_file;
    uint32_t start = 0;
    uint32_t length = 0;
    uint32_t frequency = 22050;
    uint32_t header_size = 2048;

    // Read config
    auto [config_freq, config_header] = readConfig("yadpcm2wav.ini");
    frequency = config_freq;
    header_size = config_header;

    if (argc < 3) {
        std::cout << "Usage: yadpcm2wav inputFile outputFile [-start=] [-length=] [-freq=] [-header=]" << std::endl;
        std::cout << "Outputs 22050hz WAV by default. If outputFile extension is .pcm you get raw PCM instead." << std::endl;
        std::cout << std::endl;
        std::cout << "The following arguments are optional:" << std::endl;
        std::cout << "  -start=### specify a start offset" << std::endl;
        std::cout << "  -length=### specify a length to convert" << std::endl;
        std::cout << "  -freq=### override the default frequency for WAV output" << std::endl;
        std::cout << "  -header=### specify the header size to skip" << std::endl;
        std::cout << "No arguments provided. Opening file dialog..." << std::endl;

        in_file = openFileDialog();
        if (in_file.empty()) {
            std::cerr << "No file selected. Exiting." << std::endl;
            return 1;
        }
        out_file = in_file + ".wav";
    } else {
        in_file = argv[1];
        out_file = argv[2];

        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg.find("-start=") == 0) {
                start = std::stoi(arg.substr(7));
            } else if (arg.find("-length=") == 0) {
                length = std::stoi(arg.substr(8));
            } else if (arg.find("-freq=") == 0) {
                frequency = std::stoi(arg.substr(6));
            } else if (arg.find("-header=") == 0) {
                header_size = std::stoi(arg.substr(8));
            }
        }
    }

    if (frequency < 8000 || frequency > 48000) {
        std::cout << "WAV Frequency should be between 8000 and 48000. Example: 44100" << std::endl;
        return 1;
    }

    std::ifstream input(in_file, std::ios::binary);
    if (!input) {
        std::cerr << "Error opening input file." << std::endl;
        return 1;
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();

    start += header_size;

    if (start > data.size() || (length > 0 && start + length > data.size())) {
        std::cout << "Data range specified is larger than the input file. Can't do anything with that." << std::endl;
        return 1;
    }

    if (length == 0) {
        length = data.size() - start;
    }

    std::cout << "Processing file from offset " << start << " with length " << length << "..." << std::endl;

    std::vector<uint8_t> pcm_data = adpcm2pcm(data, start, length);

    if (out_file.find(".pcm") == std::string::npos) {
        pcm_data = add_wav_header(pcm_data, frequency);
    }

    std::ofstream output(out_file, std::ios::binary);
    if (!output) {
        std::cerr << "Error opening output file." << std::endl;
        return 1;
    }

    output.write(reinterpret_cast<const char*>(pcm_data.data()), pcm_data.size());
    output.close();

    std::cout << "Done!" << std::endl;

    return 0;
}