#include "../Log.h"
#include "Iop_Cdvdman.h"

#define LOG_NAME            "iop_cdvdman"

#define FUNCTION_CDREAD         "CdRead"
#define FUNCTION_CDGETERROR     "CdGetError"
#define FUNCTION_CDSYNC         "CdSync"
#define FUNCTION_CDGETDISKTYPE  "CdGetDiskType"
#define FUNCTION_CDDISKREADY    "CdDiskReady"

using namespace Iop;
using namespace std;

CCdvdman::CCdvdman(uint8* ram) :
m_ram(ram),
m_image(NULL)
{

}

CCdvdman::~CCdvdman()
{

}

string CCdvdman::GetId() const
{
    return "cdvdman";
}

string CCdvdman::GetFunctionName(unsigned int functionId) const
{
    switch(functionId)
    {
    case 6:
        return FUNCTION_CDREAD;
        break;
    case 8:
        return FUNCTION_CDGETERROR;
        break;
    case 11:
        return FUNCTION_CDSYNC;
        break;
    case 12:
        return FUNCTION_CDGETDISKTYPE;
        break;
    case 13:
        return FUNCTION_CDDISKREADY;
        break;
    default:
        return "unknown";
        break;
    }
}

void CCdvdman::Invoke(CMIPS& ctx, unsigned int functionId)
{
    switch(functionId)
    {
    case 6:
        ctx.m_State.nGPR[CMIPS::V0].nV0 = CdRead(
            ctx.m_State.nGPR[CMIPS::A0].nV0,
            ctx.m_State.nGPR[CMIPS::A1].nV0,
            ctx.m_State.nGPR[CMIPS::A2].nV0,
            ctx.m_State.nGPR[CMIPS::A3].nV0);
        break;
    case 8:
        ctx.m_State.nGPR[CMIPS::V0].nV0 = CdGetError();
        break;
    case 11:
        ctx.m_State.nGPR[CMIPS::V0].nV0 = CdSync(ctx.m_State.nGPR[CMIPS::A0].nV0);
        break;
    case 12:
        ctx.m_State.nGPR[CMIPS::V0].nV0 = CdGetDiskType();
        break;
    case 13:
        ctx.m_State.nGPR[CMIPS::V0].nV0 = CdDiskReady(ctx.m_State.nGPR[CMIPS::A0].nV0);
        break;
    default:
        CLog::GetInstance().Print(LOG_NAME, "Unknown function called (%d).\r\n", 
            functionId);
        break;
    }
}

void CCdvdman::SetIsoImage(CISO9660* image)
{
    m_image = image;
}

uint32 CCdvdman::CdRead(uint32 startSector, uint32 sectorCount, uint32 bufferPtr, uint32 modePtr)
{
    CLog::GetInstance().Print(LOG_NAME, FUNCTION_CDREAD "(startSector = 0x%X, sectorCount = 0x%X, bufferPtr = 0x%0.8X, modePtr = 0x%0.8X);\r\n",
        startSector, sectorCount, bufferPtr, modePtr);
    if(modePtr != 0)
    {
        uint8* mode = &m_ram[modePtr];
        //Does that make sure it's 2048 byte mode?
        assert(mode[2] == 0);
    }
    if(m_image != NULL && bufferPtr != 0)
    {
        uint8* buffer = &m_ram[bufferPtr];
        uint32 sectorSize = 2048;
        for(unsigned int i = 0; i < sectorCount; i++)
        {
            m_image->ReadBlock(startSector + i, buffer);
            buffer += sectorSize;
        }
    }
    return 1;
}

uint32 CCdvdman::CdGetError()
{
    CLog::GetInstance().Print(LOG_NAME, FUNCTION_CDGETERROR "();\r\n");
    return 0;
}

uint32 CCdvdman::CdSync(uint32 mode)
{
    CLog::GetInstance().Print(LOG_NAME, FUNCTION_CDSYNC "(mode = %i);\r\n",
        mode);
    return 0;
}

uint32 CCdvdman::CdGetDiskType()
{
    CLog::GetInstance().Print(LOG_NAME, FUNCTION_CDGETDISKTYPE "();\r\n");
    //0x14 = PS2DVD
    return 0x14;
}

uint32 CCdvdman::CdDiskReady(uint32 mode)
{
    CLog::GetInstance().Print(LOG_NAME, FUNCTION_CDDISKREADY "(mode = %i);\r\n",
        mode);
    return 2;
}
