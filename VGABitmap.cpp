#include "VGABitmap.h"

#include <fstream>

using namespace std;

#pragma pack(push,2)
struct BitmapFileHeader
{
	uint16_t  type;
	uint32_t  size;
	uint16_t  reserved1;
	uint16_t  reserved2;
	uint32_t  offBits;
};
#pragma pack(pop)

static_assert(sizeof(BitmapFileHeader)==14, "No padding is allowed in BitmapFileHeader");

struct BitmapInfoHeader
{
	uint32_t size;
	uint32_t width;
	uint32_t height;
	uint16_t planes;
	uint16_t bitCount;
	uint32_t compression;
	uint32_t sizeImage;
	uint32_t XPelsPerMeter;
	uint32_t YPelsPerMeter;
	uint32_t clrUsed;
	uint32_t clrImportant;
} __attribute__((packed));


bool exportBMPToFile(uint32_t* bmp,const string& fileName, int h, int w)
{
	const int BufSize = h*w*4;
	BitmapFileHeader bmpfh = {};
	BitmapInfoHeader bmpih = {};
	ofstream out(fileName, ios::binary);
	if (out.is_open() == false)
		return false;
	bmpfh.type=0x4D42;
	bmpfh.offBits=sizeof(BitmapFileHeader)+sizeof(BitmapInfoHeader);
	bmpfh.size=bmpfh.offBits+BufSize;
	bmpih.size=sizeof(BitmapInfoHeader);
	bmpih.width=w;
	bmpih.height=h;
	bmpih.planes=1;
	bmpih.bitCount=32;
	bmpih.compression=0; //BI_RGB
	out.write((const char*)&bmpfh, sizeof(BitmapFileHeader));
    out.write((const char*)&bmpih, sizeof(BitmapInfoHeader));
    out.write((const char*)bmp, BufSize);
	return !out.bad();
}
