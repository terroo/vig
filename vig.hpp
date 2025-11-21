#pragma once

#include <iostream>
#include <vector>
#include <memory>
#include <filesystem>

#include "stb_truetype.h"
#include "font.hpp"

extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  #include <libswscale/swscale.h>
}

class Vig {

  std::string input;
  int VIG_COLS;
  int VIG_ROWS;
  const int VIG_SAMPLES = VIG_COLS * VIG_ROWS;

  static const int VIG_THUMB_W = 480;
  static const int VIG_THUMB_H = 270;
  static const int VIG_THUMB_PAD = 20; // 12

  static const int VIG_THUMB_BORDER = 4;
  static const uint8_t VIG_BORDER_R = 0;
  static const uint8_t VIG_BORDER_G = 0;
  static const uint8_t VIG_BORDER_B = 0;

  static const int VIG_PAD_LEFT = 30;
  static const int VIG_PAD_RIGHT = 30;
  static const int VIG_PAD_TOP = 30;
  static const int VIG_PAD_BOTTOM = 30;

  static const int VIG_HEADER_H = 80;
  static constexpr float VIG_HEADER_FONT_SIZE = 38.0f;
  static const uint8_t VIG_HEADER_COLOR_R = 0;
  static const uint8_t VIG_HEADER_COLOR_G = 0;
  static const uint8_t VIG_HEADER_COLOR_B = 0;

  static const uint8_t VIG_BG_R = 239;
  static const uint8_t VIG_BG_G = 239;
  static const uint8_t VIG_BG_B = 239;

  stbtt_fontinfo font;

  void draw_text(std::vector<uint8_t>& canvas, int VIG_W, int VIG_H,
      const std::string& text, int x, int y,
      float size_px,
      uint8_t VIG_R, uint8_t VIG_G, uint8_t VIG_B);

  static void save_jpg(uint8_t* rgb, int w, int h, const std::string& path);
  std::string datetime();

  public:
    Vig(const std::filesystem::path& input,
      std::optional<int> cols = std::nullopt,
      std::optional<int> rows = std::nullopt);
    void run();
};
