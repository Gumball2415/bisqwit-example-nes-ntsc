// modified from https://bisqwit.iki.fi/jutut/kuvat/programming_examples/nesemu1/ntsc-small.cc
// by Persune 2024

#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdio>

namespace
{
    // This function generates a testcard resembling Philips PM5544 testcard, but with NES colors.
    // Inputs:  x = x coordinate, y = y coordinate
    // Output:  NES color index (0x00-0x3F = main palette color, plus 3 bits of color de-emphasis).
    /* The only purpose of this function is to generate a sample NES screen without any special processing. */
    unsigned TestCard(int x,int y)
    {
        const float aspect = 8./7;
        // Define the geometry
        const int width   = 256,     height  = 240;
        const int xcenter = width/2, ycenter = height/2;
        // Define colors
        const int black = 0x1D, white = 0x30;
        // Define background grid
        int xgrid = ((x+8)>>4)-1, xgridsub = (x+8)&15, ygrid = ((y+8)>>4)-1, ygridsub = (y+8)&15;
        int c = (xgridsub == 0 || ygridsub == 0) ? white // white grid
            : ((ygrid < 0 || ygrid >= 14)
               ? (xgrid & 1) ? white : black // white or black checkerboard at top & bottom
               : 0x00); // gray otherwise

        // Define some lobes
        if(y > 24 && ygrid < 13)
        {
            static const char lobe[] = {0x15/*90*/,0x1A/*250*/, 0x17/*326*/,0x1C/*146*/, 0x14/*0*/,0x18/*180*/};
            bool nook = (ygrid < 3 || (ygrid >= 11 && (ygrid>11 || ygridsub>0)));
            if(xgrid >= 0 && xgrid <= 1
            && (xgrid == 0 ? xgridsub > 0 : nook)
            ) c = lobe[(y < ycenter) + 2*(xgrid==1)];

            if(xgrid >= 13 && xgrid <= 14
            && (xgrid == 14 ? xgridsub > 0 || nook : (nook && xgridsub > 0))
            ) c = lobe[(y < ycenter) + 4 - 2*(xgrid==13)];
        }

        // Define circle
        const int xdist = std::abs(x-xcenter);
        const int ydist = std::abs(y-ycenter);
        float xdista = xdist*aspect;
        if(std::sqrt(xdista*xdista + ydist*ydist) >= 6.7*16) return c;

        c = white; // white for circle
        if((xdist < 8 && ydist < 24 && y < 7*16) || ydist <= 8)
        {
            if(xgridsub != 0 && ygridsub != 0) c = black;
            if(xdist == 0) c = white; // white vertical line for center
            return c;
        }
        // Define some exceptional content
        switch(ygrid)
        {
            case 1: if( xdist < width*0.12 )
                c = 0x0E + 0x10 * ((x>>3) & 3); // Use four different blacks for the station ID background
                break;
            case 2: if( xdist > width*0.18 || x == (int)(width*0.35)) c = black; break;
            case 11: if( xdist <= width*0.18 && x != (int)(width*0.35))
                c = 0x0F + 0x10 * ((x>>3) & 3); // Use four different blacks for the station ID background
                break;
            case 3: c = (x+5)%30 < 15 ? 0x10 : black; break; // gray or black
            case 4: case 5: case 6:
            {
                int freq = 3 - (int)(x / 64);
                int sub = (y - 8 - 4*16);
                c = (((x >> freq) ^ (int)(sub / 20)) & 1) ? white : black;
                c |= 0x40 * (int)(sub / 6); // Add vertically changing color de-emphasis
                break;
            }
            case 7: case 8: case 9:
            {
                int sub = ((y - 8*16) >> 3);
                float firstx = xcenter - 96*aspect;
                float lastx    = xcenter + 96*aspect;
                float xpos = (x-firstx) / (lastx-firstx);
                if(sub < 4) c = ((int)(14 * xpos)&15) + 16 * (sub&3);
                break;
            }
            case 10:
            {
                static const char gradient[] = {0x0D,0x1D,0x2D,0x00, 0x10,0x3D,0x20,0x30};
                float firstx = xcenter - 67*aspect;
                float lastx    = xcenter + 67*aspect;
                float xpos = (x-firstx) / (lastx-firstx);
                c = gradient[(int)(0.5 + (sizeof(gradient)-1) * xpos)];
                break;
            }
            case 12: case 13: c = xdist < 8 ? 0x16 : 0x28; break;
        }
        return c & 0x1FF;
    }
    /* End testcard generator. */

    // PNGencoder: A minimal PNG encoder for demonstration purposes.
    class PNGencoder
    {
        std::vector<unsigned char> output;
    private:
        static void PutWord(unsigned char* target, unsigned dword, bool msb_first)
        {
            for(int p=0; p<4; ++p) target[p] = dword >> (msb_first ? 24-p*8 : p*8);
        }
        static unsigned adler32(const unsigned char* data, unsigned size, unsigned res=1)
        {
            for(unsigned s1,a=0; a<size; ++a)
                s1 = ((res & 0xFFFF) + (data[a] & 0xFF)) % 65521,
                    res = s1 + ((((res>>16) + s1) % 65521) << 16);
            return res;
        }
        static unsigned crc32(const unsigned char* data, unsigned size, unsigned res=0)
        {
            for(unsigned tmp,n,a=0; a<size; ++a, res = ~((~res>>8) ^ tmp))
                for(tmp = (~res ^ data[a]) & 0xFF, n=0; n<8; ++n)
                    tmp = (tmp>>1) ^ ((tmp&1) ? 0xEDB88320u : 0);
            return res;
        }
        void Deflate(const unsigned char* source, unsigned srcsize)
        {
            /* A bare-bones deflate-compressor (as in gzip) */
            int algo=8, windowbits=8 /* 8 to 15 allowed */, flevel=0 /* 0 to 3 */;
            /* Put RFC1950 header: */
            unsigned h0 = algo + (windowbits-8)*16, h1 = flevel*64 + 31-((256*h0)%31);
            output.push_back(h0);
            output.push_back(h1); /* checksum and compression level */
            /* Compress data using a lossless algorithm (RFC1951): */
            for(unsigned begin=0; ; )
            {
                unsigned eat = std::min(65535u, srcsize-begin);

                std::size_t o = output.size(); output.resize(o+5);
                output[o+0] = (begin+eat) >= srcsize; /* bfinal bit and btype: 0=uncompressed */
                PutWord(&output[o+1], eat|(~eat<<16), false);
                output.insert(output.end(), source+begin, source+begin+eat);
                begin += eat;
                if(begin >= srcsize) break;
            }
            /* After compressed data, put the checksum (adler-32): */
            std::size_t o = output.size(); output.resize(o+4);
            PutWord(&output[o], adler32(source, srcsize), true);
        }
    public:
        void EncodeImage(unsigned width,unsigned height, const unsigned char* rgbdata)
        {
            std::size_t o;
            #define BeginChunk(n_reserve_extra) o = output.size(); output.resize(o+8+n_reserve_extra)
            #define EndChunk(type) do { \
                unsigned ncopy=output.size()-(o+8); \
                static const char t[4+1] = type; \
                output.resize(o+8+ncopy+4); \
                PutWord(&output[o+0], ncopy, true); /* chunk length */ \
                std::copy(t+0, t+4, &output[o+4]);  /* chunk type */   \
                /* in the chunk, put crc32 of the type and data (below) */ \
                PutWord(&output[o+8+ncopy], crc32(&output[o+4], 4+ncopy), true); \
            } while(0)
            /* Put PNG header (always const) */
            static const char header[8+1] = "\x89PNG\15\12\x1A\12";
            output.insert(output.end(), header, header+8);
            /* Put IHDR chunk */
            BeginChunk(13);
              PutWord(&output[o+8+0], width,  true); /* Put image width    */
              PutWord(&output[o+8+4], height, true); /* and height in IHDR */
              PutWord(&output[o+8+8], 0x08020000, true);
              /* Meaning of above: 8-bit,rgb-triple,deflate,std filters,no interlacing */
            EndChunk("IHDR");
            /* Put IDAT chunk */
            BeginChunk(0);
              std::vector<unsigned char> idat;
              for(unsigned y=0; y<height; ++y)
              {
                  idat.push_back(0x00); // filter type for this scanline
                  idat.insert(idat.end(), rgbdata+y*width*3, rgbdata+y*width*3+width*3);
              }
              Deflate(&idat[0], idat.size());
            EndChunk("IDAT");
            /* Put IEND chunk */
            BeginChunk(0);
            EndChunk("IEND");
            #undef BeginChunk
            #undef EndChunk
        }
        void SaveTo(const char* fn)
        {
            std::FILE* fp = std::fopen(fn, "wb");
            if(!fp) { std::perror(fn); return; }
            std::fwrite(&output[0], 1, output.size(), fp);
            std::fclose(fp);
        }
    };
    /* End PNG encoder. */

    // Define the NTSC voltage levels corresponding to each of the four different pixel colors.
    // terminated composite levels, from https://forums.nesdev.org/viewtopic.php?p=159266#p159266
    // normalized via the equation:
    // (COMPOSITE_LEVELS - NTSC_BLACK) / (NTSC_WHITE - NTSC_BLACK);
    static const float Voltages[16] = {
       -0.10659898477157359,  0.00000000000000000, 0.30456852791878175, 0.72081218274111680,  // Signal low
        0.38578680203045684,  0.67005076142131981, 1.00000000000000000, 1.00000000000000000,  // Signal high
       -0.15228426395939085, -0.07106598984771573, 0.17258883248730966, 0.50761421319796951,  // Signal low, attenuated
        0.23857868020304568,  0.46192893401015234, 0.74111675126903565, 0.74111675126903565,  // Signal high, attenuated
    };

    // sine LUTs specifically made for QAM demodulation
    // factor of 2 is applied to the LUTs for correct saturation
    // https://www.nesdev.org/wiki/NTSC_video#Decoding_chroma_information_(UV)
    static const float sin_table[12] = {
        // =SIN(PI() * (PHASE + 3 - 0.5) / 6) * 2
        1.9318516525781366,
        1.9318516525781366,
        1.4142135623730951,
        0.517638090205042,
       -0.5176380902050416,
       -1.4142135623730943,
       -1.9318516525781366,
       -1.9318516525781368,
       -1.4142135623730954,
       -0.5176380902050431,
        0.5176380902050421,
        1.4142135623730947,
    };

    static const float cos_table[12] = {
        // =COS(PI() * (PHASE + 3 - 0.5) / 6) * 2
        0.5176380902050415,
       -0.5176380902050413,
       -1.414213562373095,
       -1.9318516525781364,
       -1.9318516525781366,
       -1.4142135623730958,
       -0.5176380902050413,
        0.5176380902050406,
        1.4142135623730947,
        1.9318516525781362,
        1.9318516525781364,
        1.4142135623730954,
    };
}

/* MAIN PROGRAM */
int main(int argc, char** argv)
{
    if(argc != 7)
    {
        std::fprintf(stderr, "NTSC NES test cart, modified by Persune\n");
        std::fprintf(stderr, "Usage: prog <phase> <signalsperpixel> <saturation> <width> <height> <output filename>\n");
        return 0;
    }
    unsigned output_width  = std::atoi(argv[4]);
    unsigned output_height = std::atoi(argv[5]);

    std::vector<unsigned char> rgbdata;
    rgbdata.resize( output_width*output_height*3 );

    // The NTSC signal decoder maintains a buffer of previous values.
    const float Saturation = std::atoi(argv[3]) != 0 ? 1.f : 0.f;
    const unsigned SignalBufferWidth = 12, YtuningQuality = 0, IQtuningQuality = 0, SignalsPerPixel = std::atoi(argv[2]);
    float SignalHistory[SignalBufferWidth]{}, SumY = 0.f, SumU = 0.f, SumV = 0.f;

    // Initial phase value for this frame.
    unsigned DecodePhase = std::atoi(argv[1]) % SignalBufferWidth;

    // Generate picture. 240 scanlines.
    for(unsigned y=0; y<240; ++y)
    {
        /* For horizontal position in output pixture: */
        unsigned pixelcarry = 0, hpos = 0;
        /* For vertical position in output picture: */
        unsigned vpos = y*output_height/240, n_lines = (y+1)*output_height/240 - vpos;

        // Render 256 columns.
        for(unsigned x=0; x<256; ++x)
        {
            // Retrieve current pixel color from the PPU.
            unsigned pixel = TestCard(x,y);

            // Decode the NES color.
            int color = (pixel & 0x0F);    // 0..15 "cccc"
            int level = (pixel >> 4) & 3;  // 0..3  "ll"
            int emphasis = (pixel >> 6);   // 0..7  "eee"
            if(color > 13) { level = 1;  } // For colors 14..15, level 1 is forced.

            // Now generate the SignalsPerPixel samples.
            for(unsigned n=0; n<SignalsPerPixel; ++n)
            {
                if(!DecodePhase--) DecodePhase = SignalBufferWidth-1;

                auto InColorPhase = [=](int color) { return (color + DecodePhase) % 12 < 6; }; // Inline function

                // When de-emphasis bits are set, some parts of the signal are attenuated:
                // colors 14 .. 15 are not affected by de-emphasis
                int attenuation = (
                    ((emphasis & 1) && InColorPhase(0xC))
                ||  ((emphasis & 2) && InColorPhase(0x4))
                ||  ((emphasis & 4) && InColorPhase(0x8)) && (color < 0xE)) ? 8 : 0;

                // The square wave for this color alternates between these two voltages:
                float low  = Voltages[0 + level + attenuation];
                float high = Voltages[4 + level + attenuation];
                if(color == 0) { low = high; } // For color 0, only high level is emitted
                if(color > 12) { high = low; } // For colors 13..15, only low level is emitted
                float sample = InColorPhase(color) ? high : low;

                // Done generating the NTSC signal sample ("sample").

                // Begin decoding the NTSC signal!
                float oldspot1 = SignalHistory[(DecodePhase + SignalBufferWidth - YtuningQuality) % SignalBufferWidth];
                float oldspot2 = SignalHistory[(DecodePhase + SignalBufferWidth - IQtuningQuality) % SignalBufferWidth];
                SignalHistory[DecodePhase] = sample;
                float spot1 = sample - oldspot1;
                float spot2 = sample - oldspot2;
                // Note that this calculation completely cancels out any DC component in the input signal,
                // only keeping track of the magnitudes of deltas between signal level changes.
                SumY += spot1;

                // precalculated the std::cos() values into a small compile-time constant array
                SumU += spot2 * sin_table[DecodePhase % 12] * Saturation;
                SumV += spot2 * cos_table[DecodePhase % 12] * Saturation;
                // Done decoding NTSC signal: It has been decomposed into Y, U and V.

                // Check if we need to render a pixel now.
                pixelcarry += output_width;
                while(pixelcarry >= 256*SignalsPerPixel)
                {
                    // Yes, render pixel
                    pixelcarry -= 256*SignalsPerPixel;

                    // Convert YUV into RGB:
                    float r = (SumY + SumV * 1.139883f) / SignalBufferWidth;
                    float g = (SumY - SumU * 0.394642f - SumV * 0.580622f) / SignalBufferWidth;
                    float b = (SumY + SumU * 2.032062f) / SignalBufferWidth;

                    // Convert the float RGB into RGB24:
                    int rr = r*255; if(rr<0) rr=0; else if(rr>255) rr=255;
                    int gg = g*255; if(gg<0) gg=0; else if(gg>255) gg=255;
                    int bb = b*255; if(bb<0) bb=0; else if(bb>255) bb=255;

                    // Save the RGB into the frame buffer. n_lines = number of times to duplicate that pixel vertically.
                    for(unsigned l=0; l<n_lines; ++l)
                    {
                        rgbdata[ ((vpos+l) * output_width + hpos) * 3 + 0 ] = rr;
                        rgbdata[ ((vpos+l) * output_width + hpos) * 3 + 1 ] = gg;
                        rgbdata[ ((vpos+l) * output_width + hpos) * 3 + 2 ] = bb;
                    }
                    ++hpos;
                }
            }
        }
        std::fill(std::begin(SignalHistory), std::end(SignalHistory), 0.f);
        SumY = SumU = SumV = 0.f;
    }

    PNGencoder enc;
    enc.EncodeImage(output_width, output_height, &rgbdata[0]);
    enc.SaveTo(argv[6]);
    return 0;
}
