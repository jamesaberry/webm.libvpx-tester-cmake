#define _CRT_SECURE_NO_WARNINGS
#include "on2-vpx-shim.h"
#include "onyx.h"
#include "ivf.h"
#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdarg.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "vpx_mem.h"
#include "vp8cx.h"
#include <cstdio>
using namespace std;

char TesterExePath[256];//Temporary solution for DSHOW

extern unsigned int GetTime();
extern void FolderName2(char *DirIn, char *DirOut);

extern double IVFPSNR(char *inputFile1, char *inputFile2, int forceUVswap, int frameStats, int printvar, double *SsimOut);
extern unsigned int TimeCompressIVFtoIVF(char *inputFile, char *outputFile2, int speed, int BitRate, VP8_CONFIG &oxcf, char *CompressString, int CompressInt, int RunQCheck);

extern int CompressIVFtoIVF_TimeOuput(char *inputFile, char *outputFile2, int speed, int BitRate, VP8_CONFIG &opt, char *CompressString, int CompressInt);
extern int DecompressIVFtoIVF(char *inputFile, char *outputFile2);
extern int FauxDecompress(char *inputChar);
extern int FauxCompress();

extern int FormatFrameHeaderRead(IVF_FRAME_HEADER &ivf_fh);
extern int FormatIVFHeaderRead(IVF_HEADER *ivf);
extern char *itoa_custom(int value, char *result, int base);
extern int FormatFrameHeaderRead(IVF_FRAME_HEADER &ivf_fh);
extern int MakeDirVPX(string CreateDir2);

#include "onyxd.h"

typedef unsigned char BYTE;

// very ugly quick and dirty parameter parsing...
// this constitutes all member variables in VP8_CONFIG structure we care about
#define ALLPARMS(O,DOTHIS) \
    DOTHIS(O,  TargetBandwidth)\
    DOTHIS(O,  NoiseSensitivity)\
    DOTHIS(O,  Sharpness)\
    DOTHIS(O,  CpuUsed)\
    DOTHIS(O,  Mode)\
    DOTHIS(O,  AutoKey)\
    DOTHIS(O,  KeyFreq)\
    DOTHIS(O,  EndUsage)\
    DOTHIS(O,  UnderShootPct)\
    DOTHIS(O,  StartingBufferLevel)\
    DOTHIS(O,  OptimalBufferLevel)\
    DOTHIS(O,  MaximumBufferSize)\
    DOTHIS(O,  FixedQ)\
    DOTHIS(O,  WorstAllowedQ)\
    DOTHIS(O,  BestAllowedQ)\
    DOTHIS(O,  AllowSpatialResampling)\
    DOTHIS(O,  ResampleDownWaterMark)\
    DOTHIS(O,  ResampleUpWaterMark)\
    DOTHIS(O,  AllowDF)\
    DOTHIS(O,  DropFramesWaterMark)\
    DOTHIS(O,  AllowLag)\
    DOTHIS(O,  PlayAlternate)\
    DOTHIS(O,  AltQ)\
    DOTHIS(O,  AltFreq)\
    DOTHIS(O,  GoldQ)\
    DOTHIS(O,  KeyQ)\
    DOTHIS(O,  Version)\
    DOTHIS(O,  LagInFrames)\
    DOTHIS(O,  TwoPassVBRBias)\
    DOTHIS(O,  TwoPassVBRMinSection)\
    DOTHIS(O,  TwoPassVBRMaxSection)\
    DOTHIS(O,  EncodeBreakout)\
    DOTHIS(O,  TokenPartitions) \
    DOTHIS(O,  MultiThreaded) \
    DOTHIS(O,  ErrorResilientMode)

// expands GET(oxf,AllowDF) to oxcf->AllowDF = x["AllowDF"] (x is our parmmap)
#define GET(O,V) O->V = x[#V];

// expands PUT(oxf,AllowDF) to x["AllowDF"] = oxcf->AllowDF (x is our parmmap)
#define PUT(O,V) x[#V] = O->V;

// expands SHOW(oxf,AllowDF) to cout << "AllowDF" << " = " oxcf->AllowDF
#define SHOW(O,V) cout << "  " << #V << " = " << O->V << endl;

// expands HELP(oxf,AllowDF) to cout << "AllowDF"
#define HELP(O,V) cout << "  " << #V << endl;




////////////////////////Global Slash Character Definion for multiplat////////////////////////
#if defined(_WIN32)
char slashChar = '\\';
string slashCharStr = "\\";
int Platform = 1;
#define snprintf _snprintf
#elif defined(linux)
char slashChar = '/';
string slashCharStr = "/";
int Platform = 2;
#elif defined(__APPLE__)
char slashChar = '/';
string slashCharStr = "/";
#elif defined(__POWERPC__)
char slashChar = '/';
string slashCharStr = "/";
#endif
/////////////////////////////////////////////////////////////////////////////////////////////


int parse(VP8_CONFIG *ocf, istream &ins, bool show = false)
{
    typedef map<string, int> Parms;
    Parms x;

    ALLPARMS(ocf, PUT);

    while (!ins.eof())
    {
        string var;

        ins >> var;

        if (var == ",")
            continue;

        int val;
        ins >> val;

        x[var] = val;
    }


    ALLPARMS(ocf, GET);

    if (show)
    {
        cout << "Current VP8 Settings: " << endl;
        ALLPARMS(ocf, SHOW);
    }

    return 0;
}

struct Logger
{
    virtual void Log(const char *format, ...) = 0;
    virtual void LogV(const char *format, va_list) = 0;
};

extern "C"
{

    void *on2Logger = 0;

    void ON2_Log(const char *format, ...)
    {
        Logger *logger = (Logger *)(on2Logger);
        va_list arglist;

        va_start(arglist, format);

        if (logger)
        {
            logger->LogV(format, arglist);
        }

        va_end(arglist);
    }

    void  ON2SetLogger(void *pLogger)
    {
        on2Logger = pLogger;
    }

}

extern void VP8DefaultParms(VP8_CONFIG &opt);
extern VP8_CONFIG InPutSettings(char *inputFile);
int IVF2Raw(char *inputFile, char *outputDir)
{
    int WriteIndFrames = 5;//atoi(argv[4]);

    FILE *in = fopen(inputFile, "rb");
    ///////////////////////////////////

    if (in == NULL)
    {
        printf("\nInput file does not exist");
        fprintf(stderr, "\nInput file does not exist");
        fclose(in);
        return 0;
    }

    int currentVideoFrame = 0;
    int frameCount = 0;
    int byteRec = 0;

    IVF_HEADER ivfhRaw;

    InitIVFHeader(&ivfhRaw);
    fread(&ivfhRaw, 1, sizeof(ivfhRaw), in);
    FormatIVFHeaderRead(&ivfhRaw);

    /*printf( "IVF DataRate\n\n"
    "FILE HEADER \n\n"
    "File Header            - %c%c%c%c \n"
    "File Format Version    - %i \n"
    "File Header Size       - %i \n"
    "Video Data FourCC      - %i \n"
    "Video Image Width      - %i \n"
    "Video Image Height     - %i \n"
    "Frame Rate Rate        - %i \n"
    "Frame Rate Scale       - %i \n"
    "Video Length in Frames - %i \n"
    "Unused                 - %c \n"
    "\n\n"
    ,ivfhRaw.signature[0],ivfhRaw.signature[1],ivfhRaw.signature[2],ivfhRaw.signature[3]
    ,ivfhRaw.version,ivfhRaw.headersize,ivfhRaw.FourCC,ivfhRaw.width,ivfhRaw.height,ivfhRaw.rate
    ,ivfhRaw.scale,ivfhRaw.length,ivfhRaw.unused);*/

    IVF_FRAME_HEADER ivf_fhRaw;

    fread(&ivf_fhRaw.frameSize, 1, 4, in);
    fread(&ivf_fhRaw.timeStamp, 1, 8, in);
    FormatFrameHeaderRead(ivf_fhRaw);

    frameCount = ivfhRaw.length;

    long nSamples = frameCount;
    long lRateNum = ivfhRaw.rate;
    long lRateDenom = ivfhRaw.scale;

    long nSamplesPerBlock = 1;

    long nBytes = 0;
    long nBytesMin = 999999;
    long nBytesMax = 0;

    fpos_t position;
    fgetpos(in, &position);
    cout << "\n";

    string OutputDirStrwithQuotes = outputDir;

    if (WriteIndFrames != 5)
    {
        OutputDirStrwithQuotes.append("\"");
        OutputDirStrwithQuotes.insert(0, "md \"");
        MakeDirVPX(OutputDirStrwithQuotes);
    }

    char *inbuff = new char[ivfhRaw.width * ivfhRaw.height * 3/2];

    string outputDirStr2 = outputDir;
    char outputDirChar2[255];

    if (WriteIndFrames != 5)
    {
        outputDirStr2.append(slashCharStr);
        outputDirStr2.append("AllFrames.raw");
        snprintf(outputDirChar2, 255, "%s", outputDirStr2.c_str());
    }
    else
    {
        snprintf(outputDirChar2, 255, "%s", outputDirStr2.c_str());
    }

    FILE *out2 = fopen(outputDirChar2, "wb");

    cout << "\n\nConverting to Raw\n";

    while (currentVideoFrame < frameCount)
    {
        cout << ".";
        memset(inbuff, 0, ivfhRaw.width * ivfhRaw.height * 3 / 2);
        fread(inbuff, 1, ivf_fhRaw.frameSize, in);

        string outputDirStr = outputDir;
        char currentVideoFrameStr[10];
        itoa_custom(currentVideoFrame, currentVideoFrameStr, 10);
        outputDirStr.append(slashCharStr);
        outputDirStr.append("Frame_");
        outputDirStr.append(currentVideoFrameStr);

        char outputDirChar[255];
        snprintf(outputDirChar, 255, "%s", outputDirStr.c_str());

        if (WriteIndFrames == 0 || WriteIndFrames == 5)
        {
            fwrite(inbuff, 1, ivf_fhRaw.frameSize, out2);
        }

        if (WriteIndFrames == 1)
        {
            FILE *out = fopen(outputDirChar, "wb");
            fwrite(inbuff, 1, ivf_fhRaw.frameSize, out);
            fwrite(inbuff, 1, ivf_fhRaw.frameSize, out2);
            fclose(out);
        }

        fread(&ivf_fhRaw.frameSize, 1, 4, in);
        fread(&ivf_fhRaw.timeStamp, 1, 8, in);
        FormatFrameHeaderRead(ivf_fhRaw);

        currentVideoFrame ++;
    }

    fclose(in);
    fclose(out2);

    cout << "\n";

    return 0;
}
int main(int argc, char *argv[])
{

    snprintf(TesterExePath, 255, "%s", argv[0]);

    int FrameStats = 0;
    int forceUVswap2 = 0;
    int frameStats2 = 0;
    int PSNRRun = 0;
    int RunNTimes = 0;

    if (argc < 6)
    {
        printf("\n"
               "  IVF VP8 Compress Release\n\n"
               "    <Inputfile>\n"
               "    <Outputfile>\n"
               "    <Par File Origin 7 VP7 8 VP8>\n"
               "    <Par File>\n"
               "    <Extra Commands>\n"
               "      <0 No Extra Commands>\n"
               "      <1 Run PSNR only>\n"
               "      <2 Record Compression Time only>\n"
               "      <3 Record Compression Time and Run PSNR>\n"
               "\n"
               "\n"
               "   Debug Exe using: %s\n", on2_codec_iface_name(&on2_codec_vp8_cx_algo));
        return 0;
    }

    printf("\nDebug Exe using: %s\n", on2_codec_iface_name(&on2_codec_vp8_cx_algo));
    fprintf(stderr, "\nDebug Exe using: %s\n", on2_codec_iface_name(&on2_codec_vp8_cx_algo));

    char *inputFile = argv[1];
    char *outputFile = argv[2];
    int ParVer = atoi(argv[3]);
    char *parfile = argv[4];
    int ExtraCommand = atoi(argv[5]);

    VP8_CONFIG opt;
    VP8DefaultParms(opt);


    if (ParVer == 7)
    {
        cout << "\n\nNot Yet Supported\n\n";
    }

    if (ParVer == 8)
    {
        opt = InPutSettings(parfile);
    }

    if (ExtraCommand == 4)
    {
        //This handles the Mem Leak Check Test.

        char *MemLeakCheckTXT = argv[6];

        on2_MemoryTrackerSetLogType(0, MemLeakCheckTXT);
        TimeCompressIVFtoIVF(inputFile, outputFile, opt.MultiThreaded, opt.TargetBandwidth, opt, "VP8 Debug", 0, 0);
        on2_MemoryTrackerDump();

        return 0;
    }

    if (ExtraCommand == 5)
    {
        string MemLeakCheckTXTStr1 = argv[6];
        MemLeakCheckTXTStr1.append("_Encode.txt");
        char MemLeakCheckTXT1[255];
        snprintf(MemLeakCheckTXT1, 255, "%s", MemLeakCheckTXTStr1.c_str());

        on2_MemoryTrackerSetLogType(0, MemLeakCheckTXT1);
        int x = 0;
        int n = 0;
#ifdef API
        printf("\nAPI - Testing Faux Compressions:\n");
        fprintf(stderr, "\nAPI - Testing Faux Compressions:\n");
#else
        printf("\nTesting Faux Compressions:\n");
        fprintf(stderr, "\nTesting Faux Compressions:\n");
#endif

        while (x < 10000)
        {
            FauxCompress();

            if (n == 125)
            {
                printf(".");
                fprintf(stderr, ".");
                n = 0;
            }

            x++;
            n++;
        }

        on2_MemoryTrackerDump();
        return 0;
    }

    if (ExtraCommand == 6)
    {
        string MemLeakCheckTXTStr2 = argv[6];
        char *DecinputChar = argv[7];
        MemLeakCheckTXTStr2.append("_Decode.txt");
        char MemLeakCheckTXT2[255];
        snprintf(MemLeakCheckTXT2, 255, "%s", MemLeakCheckTXTStr2.c_str());

        on2_MemoryTrackerSetLogType(0, MemLeakCheckTXT2);
        int x = 0;
        int n = 0;
#ifdef API
        printf("\n\nAPI - Testing Faux Decompressions:\n");
        fprintf(stderr, "\n\nAPI - Testing Faux Decompressions:\n");
#else
        printf("\n\nTesting Faux Decompressions:\n");
        fprintf(stderr, "\n\nTesting Faux Decompressions:\n");
#endif

        while (x < 10000)
        {
            FauxDecompress(DecinputChar);

            if (n == 125)
            {
                printf(".");
                fprintf(stderr, ".");
                n = 0;
            }

            x++;
            n++;
        }

        on2_MemoryTrackerDump();

        return 0;
    }

    cout << "inputFile: " << inputFile << "\n";
    cout << "outputFile: " << outputFile << "\n";
    cout << "parfile: " << parfile << "\n";
    cerr << "inputFile: " << inputFile << "\n";
    cerr << "outputFile: " << outputFile << "\n";
    cerr << "parfile: " << parfile << "\n";

    string outputFile2 = outputFile;
    outputFile2.append("DEC.ivf");

    char outputFile2Char [256];

    snprintf(outputFile2Char, 255, "%s", outputFile2.c_str());

    TimeCompressIVFtoIVF(inputFile, outputFile, 0, opt.TargetBandwidth, opt, "VP8 Release", 0, 0);

    double totalPsnr;

    if (ExtraCommand == 1 || ExtraCommand == 3)
    {
        cout << "\n\n";
        double ssimDummyVar = 0;
        totalPsnr = IVFPSNR(inputFile, outputFile, 0, 0, 1, NULL);

        char TextFilechar1[255];

        FolderName2(outputFile, TextFilechar1);

        char *FullName = strcat(TextFilechar1, "OLD_PSNR.txt");

        ofstream outfile2(FullName);
        outfile2 << totalPsnr;
        outfile2.close();
    }

    return 0;
}