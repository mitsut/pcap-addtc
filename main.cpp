#include <pcap/pcap.h>

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
const uint8_t SPW_ESC = 0xFC;
const int DLT_USER2_VALUE = 149;
// デフォルトの周波数: 64 Hz
const int DEFAULT_FREQUENCY = 64;

struct EpochTimeUs {
  std::int64_t us = 0;
  bool valid = false;
};

// パケット構造体（ソート用）
struct Packet {
  std::int64_t timestamp_us = 0;
  std::vector<uint8_t> data;
  uint32_t orig_len = 0;
  int datalink_type = 0;  // 0: 元のパケット, DLT_USER2: TimeCodeパケット

  bool operator<(const Packet &other) const {
    return timestamp_us < other.timestamp_us;
  }
};

// TimeCodeデータ生成: ESC + データキャラクタ
std::vector<uint8_t> generate_timecode_data(uint8_t timecode_value) {
  std::vector<uint8_t> data;
  data.push_back(SPW_ESC);
  // データキャラクタ: T0-T5 (6ビット時刻情報) + T6-T7 (2ビット固定 = 0)
  uint8_t data_char = timecode_value & 0x3F;  // T0-T5に時刻値を設定、T6-T7は0固定
  data.push_back(data_char);
  return data;
}

std::int64_t to_epoch_us(const timeval &ts, int precision) {
  std::int64_t subsec_us = 0;
  if (precision == PCAP_TSTAMP_PRECISION_NANO) {
    subsec_us = static_cast<std::int64_t>(ts.tv_usec) / 1000;
  } else {
    subsec_us = static_cast<std::int64_t>(ts.tv_usec);
  }
  return static_cast<std::int64_t>(ts.tv_sec) * 1000000LL + subsec_us;
}

std::string format_jst_iso(std::int64_t epoch_us) {
  std::int64_t epoch_sec = epoch_us / 1000000LL;
  std::int64_t subsec_us = epoch_us % 1000000LL;
  if (subsec_us < 0) {
    subsec_us += 1000000LL;
    epoch_sec -= 1;
  }

  std::time_t jst_sec = static_cast<std::time_t>(epoch_sec + 9 * 3600);
  std::tm tm_utc{};
#if defined(_WIN32)
  gmtime_s(&tm_utc, &jst_sec);
#else
  gmtime_r(&jst_sec, &tm_utc);
#endif

  char buffer[64];
  std::snprintf(
      buffer,
      sizeof(buffer),
      "%04d-%02d-%02d %02d:%02d:%02d.%06" PRId64 "+09:00",
      tm_utc.tm_year + 1900,
      tm_utc.tm_mon + 1,
      tm_utc.tm_mday,
      tm_utc.tm_hour,
      tm_utc.tm_min,
      tm_utc.tm_sec,
      subsec_us);
  return std::string(buffer);
}

void print_usage(const char *prog) {
  std::fprintf(stderr, "Usage: %s --pcap <input-pcap> [--start <epoch_us> --end <epoch_us> --file <output-pcap>] [--freq <frequency_hz>]\n", prog);
  std::fprintf(stderr, "  --pcap <input-pcap>      : Input PCAP file (required)\n");
  std::fprintf(stderr, "  --start <epoch_us>       : Start time in microseconds (optional, for TimeCode generation)\n");
  std::fprintf(stderr, "  --end <epoch_us>         : End time in microseconds (optional, for TimeCode generation)\n");
  std::fprintf(stderr, "  --file <output-pcap>     : Output PCAP file (optional, for TimeCode generation)\n");
  std::fprintf(stderr, "  --freq <frequency_hz>    : TimeCode frequency in Hz (default: 64)\n");
}
}  // namespace

int main(int argc, char **argv) {
  const char *pcap_path = nullptr;
  const char *output_path = nullptr;
  std::int64_t start_us = -1;
  std::int64_t end_us = -1;
  int frequency = DEFAULT_FREQUENCY;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--pcap") == 0) {
      if (i + 1 < argc) {
        pcap_path = argv[i + 1];
        ++i;
      } else {
        std::fprintf(stderr, "Error: --pcap requires a file path\n");
        print_usage(argv[0]);
        return 1;
      }
    } else if (std::strcmp(argv[i], "--start") == 0) {
      if (i + 1 < argc) {
        start_us = std::atoll(argv[i + 1]);
        ++i;
      } else {
        std::fprintf(stderr, "Error: --start requires a value\n");
        print_usage(argv[0]);
        return 1;
      }
    } else if (std::strcmp(argv[i], "--end") == 0) {
      if (i + 1 < argc) {
        end_us = std::atoll(argv[i + 1]);
        ++i;
      } else {
        std::fprintf(stderr, "Error: --end requires a value\n");
        print_usage(argv[0]);
        return 1;
      }
    } else if (std::strcmp(argv[i], "--file") == 0) {
      if (i + 1 < argc) {
        output_path = argv[i + 1];
        ++i;
      } else {
        std::fprintf(stderr, "Error: --file requires a file path\n");
        print_usage(argv[0]);
        return 1;
      }
    } else if (std::strcmp(argv[i], "--freq") == 0) {
      if (i + 1 < argc) {
        frequency = std::atoi(argv[i + 1]);
        if (frequency <= 0) {
          std::fprintf(stderr, "Error: --freq must be a positive integer\n");
          print_usage(argv[0]);
          return 1;
        }
        ++i;
      } else {
        std::fprintf(stderr, "Error: --freq requires a value\n");
        print_usage(argv[0]);
        return 1;
      }
    } else {
      std::fprintf(stderr, "Error: Unknown option: %s\n", argv[i]);
      print_usage(argv[0]);
      return 1;
    }
  }

  if (!pcap_path) {
    std::fprintf(stderr, "Error: --pcap option is required\n");
    print_usage(argv[0]);
    return 1;
  }

  // TimeCode生成時は必須オプション
  bool generate_timecode = (start_us >= 0 && end_us >= 0 && output_path);
  if (generate_timecode && start_us >= end_us) {
    std::fprintf(stderr, "Error: --start must be less than --end\n");
    return 1;
  }

  char errbuf[PCAP_ERRBUF_SIZE];
  std::memset(errbuf, 0, sizeof(errbuf));

  pcap_t *handle = pcap_open_offline(pcap_path, errbuf);
  if (!handle) {
    std::fprintf(stderr, "Failed to open PCAP: %s\n", errbuf[0] ? errbuf : "unknown error");
    return 1;
  }

  int precision = PCAP_TSTAMP_PRECISION_MICRO;
#ifdef PCAP_TSTAMP_PRECISION_NANO
  precision = pcap_get_tstamp_precision(handle);
#endif

  // TimeCode生成時は、既存パケットを全て読み込んでメモリに蓄積
  std::vector<Packet> packets;
  
  std::int64_t frame_count = 0;
  EpochTimeUs first;
  EpochTimeUs last;

  const u_char *packet_data = nullptr;
  struct pcap_pkthdr *header = nullptr;
  int result = 0;

  while ((result = pcap_next_ex(handle, &header, &packet_data)) >= 0) {
    if (result == 0) {
      continue;
    }
    ++frame_count;
    std::int64_t epoch_us = to_epoch_us(header->ts, precision);
    if (!first.valid) {
      first.us = epoch_us;
      first.valid = true;
    }
    last.us = epoch_us;
    last.valid = true;

    // TimeCode生成が必要な場合、パケットをメモリに蓄積
    if (generate_timecode) {
      Packet pkt;
      pkt.timestamp_us = epoch_us;
      pkt.orig_len = header->len;
      pkt.datalink_type = 0;  // 元のパケット
      pkt.data.assign(packet_data, packet_data + header->caplen);
      packets.push_back(pkt);
    }
  }

  if (result == -1) {
    std::fprintf(stderr, "Error while reading PCAP: %s\n", pcap_geterr(handle));
    pcap_close(handle);
    return 1;
  }

  int original_datalink = pcap_datalink(handle);
  pcap_close(handle);

  std::printf("File: %s\n", pcap_path);
  std::printf("Frame count: %" PRId64 "\n", frame_count);

  if (!first.valid || !last.valid) {
    std::printf("First frame time: N/A\n");
    std::printf("Last frame time: N/A\n");
    std::printf("Duration: N/A\n");
    return 0;
  }

  std::string first_jst = format_jst_iso(first.us);
  std::string last_jst = format_jst_iso(last.us);

  std::printf("First frame time:\n");
  std::printf("  JST: %s\n", first_jst.c_str());
  std::printf("  epoch_us: %" PRId64 "\n", first.us);

  std::printf("Last frame time:\n");
  std::printf("  JST: %s\n", last_jst.c_str());
  std::printf("  epoch_us: %" PRId64 "\n", last.us);

  std::int64_t duration_us = last.us - first.us;
  double duration_sec = static_cast<double>(duration_us) / 1000000.0;
  std::printf("Duration: %.6f s\n", duration_sec);

  // TimeCode生成とマージ処理
  if (generate_timecode) {
    std::printf("\nGenerating TimeCode packets...\n");
    
    // 周波数に基づいてTimeCodeピリオドを計算
    // 例: 64 Hz の場合、1/64秒 = 15625マイクロ秒
    std::int64_t timecode_period_us = 1000000LL / frequency;
    std::printf("TimeCode frequency: %d Hz (period: %" PRId64 " us)\n", frequency, timecode_period_us);
    
    // TimeCodeパケットを生成
    uint8_t timecode_value = 0;
    for (std::int64_t ts = start_us; ts <= end_us; ts += timecode_period_us) {
      Packet pkt;
      pkt.timestamp_us = ts;
      pkt.datalink_type = DLT_USER2_VALUE;
      pkt.data = generate_timecode_data(timecode_value);
      pkt.orig_len = pkt.data.size();
      packets.push_back(pkt);
      
      timecode_value = (timecode_value + 1) % 64;  // 0-63でサイクル
    }

    // タイムスタンプでソート
    std::sort(packets.begin(), packets.end());

    std::printf("Total packets (original + TimeCode): %zu\n", packets.size());

    // 出力PCAPファイルを作成
    pcap_t *out_handle = pcap_open_dead(DLT_USER2_VALUE, 65535);
    if (!out_handle) {
      std::fprintf(stderr, "Failed to create output PCAP handle\n");
      return 1;
    }

    pcap_dumper_t *dumper = pcap_dump_open(out_handle, output_path);
    if (!dumper) {
      std::fprintf(stderr, "Failed to open output PCAP file: %s\n", pcap_geterr(out_handle));
      pcap_close(out_handle);
      return 1;
    }

    // パケットを出力
    for (const auto &pkt : packets) {
      struct pcap_pkthdr hdr{};
      hdr.ts.tv_sec = pkt.timestamp_us / 1000000LL;
      hdr.ts.tv_usec = pkt.timestamp_us % 1000000LL;
      hdr.len = pkt.orig_len;
      hdr.caplen = pkt.data.size();

      pcap_dump(reinterpret_cast<u_char *>(dumper), &hdr, pkt.data.data());
    }

    pcap_dump_close(dumper);
    pcap_close(out_handle);

    std::printf("Output PCAP file created: %s\n", output_path);
  }

  return 0;
}
