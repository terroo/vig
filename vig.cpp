#include "vig.hpp"

// From: 
//   https://github.com/nothings/stb
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace fs = std::filesystem;

Vig::Vig(const std::filesystem::path& input_path,
    std::optional<int> cols,
    std::optional<int> rows)
  : input(input_path), 
  VIG_COLS(cols.value_or(VIG_COLS)),
  VIG_ROWS(rows.value_or(VIG_ROWS)){
    if(VIG_COLS > 10 || VIG_ROWS > 10){
      std::cerr << "The maximum number of rows or columns is: 10.\n";
      std::exit(1);
    }
  }

void Vig::draw_text(std::vector<uint8_t>& canvas, int VIG_W, int VIG_H,
    const std::string& text, int x, int y,
    float size_px,
    uint8_t VIG_R, uint8_t VIG_G, uint8_t VIG_B){

  float scale = stbtt_ScaleForPixelHeight(&font, size_px);

  int ascent, descent, line_gap;
  stbtt_GetFontVMetrics(&font, &ascent, &descent, &line_gap);
  int baseline = y + static_cast<int>(std::lround(static_cast<float>(ascent) * scale));

  int cx = x;

  for(char c : text){
    int gw, gh, xoff, yoff;
    unsigned char* bmp = stbtt_GetCodepointBitmap(&font, 0, scale, c, &gw, &gh, &xoff, &yoff);

    for(int iy = 0; iy < gh; iy++){
      for(int ix = 0; ix < gw; ix++){
        int dst_x = cx + ix + xoff;
        int dst_y = baseline + iy + yoff;
        if (dst_x < 0 || dst_x >= VIG_W || dst_y < 0 || dst_y >= VIG_H)
          continue;

        uint8_t alpha = bmp[iy * gw + ix];
        float a = alpha / 255.0f;

        size_t idx = static_cast<size_t>((dst_y * VIG_W + dst_x) * 3);
        canvas[idx + 0] = static_cast<uint8_t>((1 - a) * canvas[idx + 0] + a * VIG_R);
        canvas[idx + 1] = static_cast<uint8_t>((1 - a) * canvas[idx + 1] + a * VIG_G);
        canvas[idx + 2] = static_cast<uint8_t>((1 - a) * canvas[idx + 2] + a * VIG_B);
      }
    }

    int adv, lsb;
    stbtt_GetCodepointHMetrics(&font, c, &adv, &lsb);
    cx += static_cast<int>(std::lround(static_cast<float>(adv) * scale));

    stbtt_FreeBitmap(bmp, nullptr);
  }
}

void Vig::save_jpg(uint8_t* rgb, int w, int h, const std::string& path){
  stbi_write_jpg(path.c_str(), w, h, 3, rgb, 90);
}

void Vig::run(){
  if(!fs::exists(input)){
    std::cerr << "Error: file: '" << input << "' does not exist.\n";
    std::exit(1);
  }

  const fs::path dir = "/tmp/frames-vig";
  fs::remove_all(dir);
  fs::create_directories(dir);

  stbtt_InitFont(&font, data.data(), stbtt_GetFontOffsetForIndex(data.data(), 0));

  avformat_network_init();

  AVFormatContext* fmt = nullptr;

  if(avformat_open_input(&fmt, input.c_str(), nullptr, nullptr) < 0){
    std::cerr << "I was unable to open the file.\n";
    avformat_close_input(&fmt);
    std::exit(1);
  }

  if(avformat_find_stream_info(fmt, nullptr) < 0){
    std::cerr << "Could not read info from the stream.\n";
    avformat_close_input(&fmt);
    std::exit(1);
  }

  std::string demuxer = fmt->iformat->name;
  if(demuxer.find("mov") == std::string::npos &&
      demuxer.find("mp4") == std::string::npos &&
      demuxer.find("asf") == std::string::npos){
    std::cerr << "Only MP4, MOV, and WMV files.\n";
    avformat_close_input(&fmt);
    std::exit(1);
  }

  int videoStream = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
  AVStream* st = fmt->streams[videoStream];
  AVCodecParameters* cp = st->codecpar;

  const AVCodec* codec = avcodec_find_decoder(cp->codec_id);
  AVCodecContext* cc = avcodec_alloc_context3(codec);
  avcodec_parameters_to_context(cc, cp);
  avcodec_open2(cc, codec, nullptr);

  int video_w = cp->width;
  int video_h = cp->height;

  double duration = static_cast<double>(fmt->duration) / AV_TIME_BASE;
  int VIG_HH = static_cast<int>(duration / 3600);
  int VIG_MM = static_cast<int>((static_cast<int>(duration) % 3600) / 60);
  int VIG_SS = static_cast<int>(duration) % 60;

  int64_t total_frames = st->nb_frames;
  if(total_frames == 0){
    double dur = static_cast<double>(st->duration) * av_q2d(st->time_base);
    double fps = av_q2d(st->r_frame_rate);
    total_frames = static_cast<int64_t>(dur * fps);
  }

  int64_t step = total_frames / VIG_SAMPLES;
  std::vector<int64_t> targets(static_cast<size_t>(VIG_SAMPLES));
  for(size_t i = 0; i < static_cast<size_t>(VIG_SAMPLES); i++){
    targets[i] = static_cast<int64_t>(static_cast<int64_t>(i) * step);
  }

  AVPacket* pkt = av_packet_alloc();
  AVFrame* frame = av_frame_alloc();
  SwsContext* sws = nullptr;

  int64_t currentFrame = 0;
  size_t index = 0;

  while(av_read_frame(fmt, pkt) >= 0 && index < static_cast<size_t>(VIG_SAMPLES)){
    if(pkt->stream_index == videoStream){
      avcodec_send_packet(cc, pkt);

      while(avcodec_receive_frame(cc, frame) == 0){
        if(currentFrame == targets[index]){
          AVFrame* rgb = av_frame_alloc();
          rgb->format = AV_PIX_FMT_RGB24;
          rgb->width = frame->width;
          rgb->height = frame->height;
          av_frame_get_buffer(rgb, 32);

          if(!sws){
            sws = sws_getContext(frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
                frame->width, frame->height, AV_PIX_FMT_RGB24,
                SWS_BICUBIC, nullptr, nullptr, nullptr);
          }

          sws_scale(sws, frame->data, frame->linesize, 0, frame->height, rgb->data, rgb->linesize);

          // CORREÇÃO: Copiar os dados considerando o linesize
          std::vector<uint8_t> rgb_buffer(frame->width * frame->height * 3);
          for(int y = 0; y < frame->height; y++){
            memcpy(&rgb_buffer[y * frame->width * 3], 
                &rgb->data[0][y * rgb->linesize[0]], 
                frame->width * 3);
          }

          std::string name = dir.string() + "/frame_" + std::to_string(index) + ".jpg";
          save_jpg(rgb_buffer.data(), frame->width, frame->height, name);

          av_frame_free(&rgb);
          ++index;
        }
        ++currentFrame;
      }
    }
    av_packet_unref(pkt);
  }

  sws_freeContext(sws);
  av_frame_free(&frame);
  av_packet_free(&pkt);
  avcodec_free_context(&cc);
  avformat_close_input(&fmt);

  int VIG_W = VIG_PAD_LEFT + (VIG_COLS * VIG_THUMB_W + (VIG_COLS - 1) * VIG_THUMB_PAD) + VIG_PAD_RIGHT;
  int VIG_H = VIG_PAD_TOP + VIG_HEADER_H + (VIG_ROWS * VIG_THUMB_H + (VIG_ROWS - 1) * VIG_THUMB_PAD) + VIG_PAD_BOTTOM;

  std::vector<uint8_t> canvas(static_cast<size_t>(VIG_W) * static_cast<size_t>(VIG_H) * 3, 0);

  for(size_t i = 0; i < canvas.size(); i += 3){
    canvas[i + 0] = VIG_BG_R;
    canvas[i + 1] = VIG_BG_G;
    canvas[i + 2] = VIG_BG_B;
  }

  for(size_t i = 0; i < static_cast<size_t>(VIG_SAMPLES); i++){
    int w, h, n;
    std::string path = dir.string() + "/frame_" + std::to_string(i) + ".jpg";
    unsigned char* img = stbi_load(path.c_str(), &w, &h, &n, 3);
    if(!img) continue;

    std::vector<uint8_t> resized(static_cast<size_t>(VIG_THUMB_W) * VIG_THUMB_H * 3);

    for(size_t y = 0; y < static_cast<size_t>(VIG_THUMB_H); y++){
      for(size_t x = 0; x < static_cast<size_t>(VIG_THUMB_W); x++){
        size_t sx = x * static_cast<size_t>(w) / VIG_THUMB_W;
        size_t sy = y * static_cast<size_t>(h) / VIG_THUMB_H;
        size_t dst_idx = (y * VIG_THUMB_W + x) * 3;
        size_t src_idx = (sy * static_cast<size_t>(w) + sx) * 3;
        memcpy(&resized[dst_idx], &img[src_idx], 3);
      }
    }
    stbi_image_free(img);

    int col = static_cast<int>(i % static_cast<size_t>(VIG_COLS));
    int row = static_cast<int>(i / static_cast<size_t>(VIG_COLS));
    int ox = VIG_PAD_LEFT + col * (VIG_THUMB_W + VIG_THUMB_PAD);
    int oy = VIG_PAD_TOP + VIG_HEADER_H + row * (VIG_THUMB_H + VIG_THUMB_PAD);

    for(int y = -VIG_THUMB_BORDER; y < VIG_THUMB_H + VIG_THUMB_BORDER; y++){
      for(int x = -VIG_THUMB_BORDER; x < VIG_THUMB_W + VIG_THUMB_BORDER; x++){
        if(x < 0 || y < 0 || x >= VIG_THUMB_W || y >= VIG_THUMB_H){
          int dst_x = ox + x;
          int dst_y = oy + y;
          if(dst_x >= 0 && dst_x < VIG_W && dst_y >= 0 && dst_y < VIG_H){
            size_t idx = static_cast<size_t>((dst_y * VIG_W + dst_x) * 3);
            canvas[idx + 0] = VIG_BORDER_R;
            canvas[idx + 1] = VIG_BORDER_G;
            canvas[idx + 2] = VIG_BORDER_B;
          }
        }
      }
    }

    for(int y = 0; y < VIG_THUMB_H; y++){
      for(int x = 0; x < VIG_THUMB_W; x++){
        size_t dst_idx = static_cast<size_t>(((oy + y) * VIG_W + (ox + x)) * 3);
        size_t src_idx = static_cast<size_t>((y * VIG_THUMB_W + x) * 3);
        memcpy(&canvas[dst_idx], &resized[src_idx], 3);
      }
    }
  }

  std::array<char, 256> header;
  fs::path video(input);

  if(VIG_COLS <= 2){
    snprintf(header.data(), sizeof(header),
        "  %dx%d   |   %02d:%02d:%02d",
        video_w, video_h, VIG_HH, VIG_MM, VIG_SS);
  }else{
    snprintf(header.data(), sizeof(header),
        "  %s  |   %dx%d   |   %02d:%02d:%02d",
        video.filename().string().c_str(), video_w, video_h, VIG_HH, VIG_MM, VIG_SS);
  }

  draw_text(canvas, VIG_W, VIG_H,
      header.data(),
      VIG_PAD_LEFT,
      VIG_PAD_TOP + 10,
      VIG_HEADER_FONT_SIZE,
      VIG_HEADER_COLOR_R, VIG_HEADER_COLOR_G, VIG_HEADER_COLOR_B);

  const std::string output = "gallery-" + datetime() + "-" + video.stem().string() + ".jpg";
  save_jpg(canvas.data(), VIG_W, VIG_H, output);

  std::cout << "Image generated: '" << output << "'\n";
}

std::string Vig::datetime(){
  std::time_t t = std::time(nullptr);
  std::tm local_tm = *std::localtime(&t);
  std::ostringstream oss;
  oss << std::put_time(&local_tm, "%d-%m-%Y-%H-%M-%S");
  return oss.str();
}

