# `vig`
Video Image Gallery
> Create an Image Gallery From a Video

![vig](vig.jpg) 

---

## Dependencies
> `C++23`

Use your system's package manager, for example:

APT:
```bash
sudo apt update
sudo apt install \
    libavformat-dev \
    libavcodec-dev \
    libavutil-dev \
    libswscale-dev \
    libswresample-dev \
    libavdevice-dev
```

Pacman:
```bash
sudo pacman -S ffmpeg
```

---

## Compile and Install
```bash
git clone https://github.com/terroo/vig
cd vig
cmake . -B build
cmake --build build
sudo cmake --install build
cd ..
rm -rf vig
```

---

## Usage
Examples:
> The images are generated in the same directory where you ran the `vig`, with the following name format:
> 
> **gallery-[date]-[hour]-[videoname].jpg**

```bash
# Basic
vig video.mp4

# Generate 4 cols and rows = 16 frames 
vig --res=4x4 video.mp4

# Generate 5 cools x 3 rows = 15 frames
vig --res=5x3 video.mp4

# Help
vig --help
```

---

## Uninstall
```bash
sudo rm $(which vig)
```
