#ifndef VGABITMAP_H
#define VGABITMAP_h

#include<string>

inline uint32_t rgb2Color(uint8_t r, uint8_t g, uint8_t b)
{
	return uint32_t(r<<16)+(g<<8)+b;
}
bool exportBMPToFile(uint32_t* bmp, const std::string& fileName, int h, int w);

#endif
