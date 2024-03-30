make
./ntsc-small 0 12 1 256 240 Nes_ntsc_perpixel_small.png
./ntsc-small 0 8 1 2048 240 Nes_ntsc_perscanline_0.png
./ntsc-small 4 8 1 2048 240 Nes_ntsc_perscanline_1.png
./ntsc-small 8 8 1 2048 240 Nes_ntsc_perscanline_2.png
./ntsc-small 0 12 0 256 240 Nes_ntsc_perpixel_small_bw.png
./ntsc-small 0 8 1 256 240 Nes_ntsc_perscanline_small_0.png
./ntsc-small 4 8 1 256 240 Nes_ntsc_perscanline_small_1.png
./ntsc-small 8 8 1 256 240 Nes_ntsc_perscanline_small_2.png
./ntsc-small 0 8 0 256 240 Nes_ntsc_perscanline_small_bw_0.png
./ntsc-small 4 8 0 256 240 Nes_ntsc_perscanline_small_bw_1.png
./ntsc-small 8 8 0 256 240 Nes_ntsc_perscanline_small_bw_2.png

ffmpeg -framerate 50 -i "Nes_ntsc_perpixel_small.png" -vf "scale=0:480:flags=neighbor,scale=2048:0:flags=neighbor,scale=584:0:flags=lanczos" "Nes_ntsc_perpixel.png" -y
ffmpeg -framerate 50 -i "Nes_ntsc_perscanline_%01d.png" -filter_complex "scale=0:480:flags=neighbor,scale=584:0:flags=lanczos,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse=dither=none" "Nes_ntsc_perscanline.gif" -y
ffmpeg -framerate 50 -i "Nes_ntsc_perscanline_small_%01d.png" -filter_complex "split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse=dither=none" "Nes_ntsc_perscanline_small.gif" -y
ffmpeg -framerate 50 -i "Nes_ntsc_perscanline_small_bw_%01d.png" -filter_complex "split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse=dither=none" "Nes_ntsc_perscanline_small_bw.gif" -y
