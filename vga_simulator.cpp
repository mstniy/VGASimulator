#include<iostream>
#include<sstream>
#include <assert.h>
#include <string>
#include <vector>
#include <fstream>
#include <memory>
#include "VGABitmap.h"

using namespace std;

class Line
{
public:
    int64_t time; // In ns
    bool hsync;
    bool vsync;
    uint32_t col;
};

int64_t syncOffset;
ifstream file;
vector<Line> linesCache;
int linesCacheCurLine = 0;

int PORCH_H; // 800
int PORCH_V; // 525/521
constexpr int VSYNC_END = 492;

void Usage()
{
    cerr << "Usage: vga_simulator <output file> <PORCH_H> <PORCH_V>" << endl;
}

int readDecimal(char* buf, int len, int& index)
{
	int res=0;
	while (index < len && '0'<=buf[index] && buf[index]<='9')
	{
		res *= 10;
		res += buf[index]-'0';
		index++;
	}
	return res;
}

int readBinary(char* buf, int len, int& index)
{
    int res=0;
    while (index < len && ('0'==buf[index] || buf[index]=='1'))
	{
		res *= 2;
		res += buf[index]-'0';
		index++;
	}
	if (index < len)
		assert(buf[index] != 'U' && buf[index] != 'u');
    return res;
}

void skipLineFeed(char* buf, int len, int& index)
{
	while (index<len && (buf[index]=='\r' || buf[index]=='\n'))
		index++;
}

void parseLines(vector<Line>& v, int numLine)
{
    unique_ptr<char[]> buf(new char[30*numLine]);
    int lastLineStart = file.tellg();
    const int fileOffsetStart = file.tellg();
    file.read(buf.get(), 30*numLine);
    const int numRead = file.gcount();
    int bufIndex = 0;
    for (int i=0; i<numLine; i++)
    {
        Line curLine;
        curLine.time = readDecimal(buf.get(), numRead, bufIndex);
		bufIndex+=5;
		if (bufIndex >= numRead-1) break;
		assert(buf[bufIndex] != 'U' && buf[bufIndex] != 'u');
        curLine.hsync = buf[bufIndex++]-'0';
		bufIndex++;
		if (bufIndex >= numRead-1) break;
		assert(buf[bufIndex] != 'U' && buf[bufIndex] != 'u');
        curLine.vsync = buf[bufIndex++]-'0';
		bufIndex++;
		int r = readBinary(buf.get(), numRead, bufIndex); // We need not check if bufIndex<numRead here, read*/skip* functions already check for that.
		bufIndex++;
		int g = readBinary(buf.get(), numRead, bufIndex);
		bufIndex++;
		if (bufIndex >= numRead) break;
		int b = readBinary(buf.get(), numRead, bufIndex);
		curLine.col = rgb2Color(r/7.0*255, g/7.0*255, b/3.0*255);
		skipLineFeed(buf.get(), numRead, bufIndex);
		lastLineStart = fileOffsetStart + bufIndex;
		v.push_back(curLine);
		if (bufIndex >= numRead) break;
    }
    file.seekg(lastLineStart);
}

bool findPixelAtTime(vector<Line>& lines, int64_t time, int& curLineInd, uint32_t& col)
{
    while(1)
    {
    	if (curLineInd >= int(lines.size())-1)
		{
			if (lines.size())
			{
				lines[0] = lines[lines.size()-1];
				lines.resize(1);
			}
			curLineInd = 0;
            parseLines(lines, PORCH_H*PORCH_V*4);
		}
		if (curLineInd >= int(lines.size())-1) // If parseLines failed to parse new lines, we reached the end of the input file.
			return false;
        const Line& curLine = lines[curLineInd];
        const Line& nextLine = lines[curLineInd+1];
        curLineInd++;
        if (nextLine.time > time)
        {
            col = curLine.col;
            return true;
        }
    }
}

int parseNextFrame(int curFrame, uint32_t* buf)
{
    for (int i=0;i<480; i++)
	{
    	for (int j=0;j<640; j++)
    	{
        	const uint64_t targetTime = 40*(curFrame*PORCH_H*PORCH_V+i*PORCH_H+j)+syncOffset; //TODO: For a more faithful simulation, we must consider all sync and vsync's in all lines. Not just during the initial sync.
        	uint32_t pixel;
        	if (findPixelAtTime(linesCache, targetTime, linesCacheCurLine, pixel) == false)
        		return i*640+j;
			buf[640 * (480-i-1) + j] = pixel;
    	}
	}
    return 640*480;
}

void syncFirstFrame()
{
	int& curLineInd = linesCacheCurLine;
    while (1)
	{
		if (curLineInd >= int(linesCache.size())-1)
		{
			if (linesCache.size())
			{
				linesCache[0] = linesCache[linesCache.size()-1];
				linesCache.resize(1);
			}
			curLineInd = 0;
            parseLines(linesCache, PORCH_H*PORCH_V*4);
		}
		if (curLineInd >= int(linesCache.size())-1) // If parseLines failed to parse new lines, we reached the end of the input file.
		{
			cerr << "EOF reached before first frame could be synchronized." << endl;
			return ;
		}
        const Line& curLine = linesCache[curLineInd];
        const Line& nextLine = linesCache[curLineInd+1];
        curLineInd++;
        if (curLine.vsync== 0 && nextLine.vsync)
		{
			cout << "Sync trigger found at time " << nextLine.time << endl;
        	break;
		}
	}
	const Line& syncLine = linesCache[curLineInd+1];
	syncOffset = syncLine.time + 40*PORCH_H*(PORCH_V-VSYNC_END);
	cout << "First frame start at time " << syncOffset <<endl;
}

int main(int argc, char* argv[])
{
    if (argc != 4)
    {
        Usage();
        return 1;
    }

    PORCH_H = atoi(argv[2]);
    PORCH_V = atoi(argv[3]);
    if(PORCH_H < 640)
    {
    	cerr << "PORCH_H too small" << endl;
    	return 1;
    }
	if(PORCH_V < 480)
	{
		cerr << "PORCH_V too small" << endl;
		return 1;
	}

    file.open(argv[1], ios::binary);
    if (file.is_open() == false)
	{
		cerr << "Failed to open input file." << endl;
		return 1;
	}
    syncFirstFrame();
    int curFrame = 0;
    while(1)
    {
        unique_ptr<uint32_t[]> frame(new uint32_t[640*480]);
		cout << "Parsing " << curFrame << "th frame..." << endl;
        const int numPixels = parseNextFrame(curFrame, frame.get());
		if (numPixels > 0)
		{
        	if (exportBMPToFile(frame.get(), ("frame"+to_string(curFrame)+".bmp").c_str(), 480, 640)==false)
        	{
            	cerr << "ExportBMPToFile failed." << endl;
	            return 1;
        	}
        	curFrame++;
		}
        if (numPixels < 640*480) // An incomplete frame means that we reached the end of the file.
        	break;
    }
    cout << "Parsed " << curFrame << "frames in total." << endl;

    return 0;
}
