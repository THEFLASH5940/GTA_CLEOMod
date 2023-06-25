#include <mod/amlmod.h>
#include <mod/logger.h>

// CLEO
#include "cleo.h"
extern eGameIdent* nGameIdent;
extern cleo_ifs_t* cleo;
#define CLEO_RegisterOpcode(x, h) cleo->RegisterOpcode(x, h); cleo->RegisterOpcodeFunction(#h, h)
#define CLEO_Fn(h) void h (void *handle, uint32_t *ip, uint16_t opcode, const char *name)

void (*UpdateCompareFlag)(void*, uint8_t);
int ValueForGame(int for3, int forvc, int forsa, int forlcs, int forvcs = 0);
uint8_t* ScriptSpace;

struct GTAVector3D
{
    float x, y, z;
    float SqrMagnitude() { return x*x + y*y + z*z; }
    inline GTAVector3D operator-(const GTAVector3D& v) { return { x - v.x, y - v.y, z - v.z }; }
};
struct GTAMatrix
{
    GTAVector3D  right;
    unsigned int flags;
    GTAVector3D  up;
    unsigned int pad1;
    GTAVector3D  at;
    unsigned int pad2;
    GTAVector3D  pos;
    unsigned int pad3;

    void* ptr;
    bool bln;
};

int (*GetPedFromRef)(int);
CLEO_Fn(GET_PED_POINTER)
{
    int ref = cleo->ReadParam(handle)->i;
    cleo->GetPointerToScriptVar(handle)->i = GetPedFromRef(ref);
}

int (*GetVehicleFromRef)(int);
CLEO_Fn(GET_VEHICLE_POINTER)
{
    int ref = cleo->ReadParam(handle)->i;
    cleo->GetPointerToScriptVar(handle)->i = GetVehicleFromRef(ref);
}

int (*GetObjectFromRef)(int);
CLEO_Fn(GET_OBJECT_POINTER)
{
    int ref = cleo->ReadParam(handle)->i;
    cleo->GetPointerToScriptVar(handle)->i = GetObjectFromRef(ref);
}

CLEO_Fn(GET_THIS_SCRIPT_STRUCT)
{
    cleo->GetPointerToScriptVar(handle)->i = (int)handle;
}

CLEO_Fn(GOSUB_IF_FALSE)
{
    int offset = cleo->ReadParam(handle)->i;
    bool condition = *(bool*)((uintptr_t)handle + ValueForGame(120, 121, 229, 525, 521));
    if(!condition)
    {
        uint8_t** stack = *(uint8_t***)((uintptr_t)handle + ValueForGame(20, 20, 24, 28, 20));
        uint8_t*& bytePtr = *(uint8_t**)((uintptr_t)handle + ValueForGame(16, 16, 20, 24, 16));
        uint16_t& stackDepth = *(uint16_t*)((uintptr_t)handle + ValueForGame(44, 44, 56, 92, 516));

        stack[stackDepth++] = bytePtr;
        int baseOffset = ValueForGame(0, 0, 16, 20, 0);
        if(!baseOffset)
        {
            bytePtr = (uint8_t*)((offset < 0) ? (ValueForGame(0x20000, 0x370E8, 0, 0) - offset) : offset);
        }
        else
        {
            uint8_t* basePtr = *(uint8_t**)((uintptr_t)handle + baseOffset);
            bytePtr = (uint8_t*)((offset < 0) ? (basePtr - offset) : (ScriptSpace + offset));
        }
    }
}

CLEO_Fn(IS_CAR_SIREN_ON)
{
    int ref = cleo->ReadParam(handle)->i;
    int vehiclePtr = GetVehicleFromRef(ref);
    UpdateCompareFlag(handle, *(uint8_t*)(vehiclePtr + 1073) >> 7);
}

CLEO_Fn(IS_CAR_ENGINE_ON)
{
    int ref = cleo->ReadParam(handle)->i;
    int vehiclePtr = GetVehicleFromRef(ref);
    UpdateCompareFlag(handle, (*(uint8_t *)(vehiclePtr + 1068) >> 4) & 1);
}

struct tByteFlag
{
    uint8_t nId : 7;
    bool    bEmpty : 1;
};
struct GTAEntity
{
    inline int AsInt() { return (int)this; }
    inline int& IntAt(int off) { return *(int*)(AsInt() + off); }
    inline uint32_t& UIntAt(int off) { return *(uint32_t*)(AsInt() + off); }
    inline uint8_t& UInt8At(int off) { return *(uint8_t*)(AsInt() + off); }
    inline GTAVector3D& GetPos()
    {
        if(*(void**)(AsInt() + 24) == NULL)
            return (*(GTAMatrix**)(AsInt() + 24))->pos;
        else
            return *(GTAVector3D*)(AsInt() + 4);
    }
};

struct GTAPedSA : GTAEntity
{
    char structure[1996];
    bool Player() { return UIntAt(1436) < 2; }
};
uintptr_t* pedPool;
int (*GetPedRef)(int);
CLEO_Fn(GET_RANDOM_CHAR_IN_SPHERE_NO_SAVE_RECURSIVE)
{
    GTAVector3D center;
    center.x = cleo->ReadParam(handle)->f;
    center.y = cleo->ReadParam(handle)->f;
    center.z = cleo->ReadParam(handle)->f;
    float radius = cleo->ReadParam(handle)->f, sqrradius = radius*radius;
    int next = cleo->ReadParam(handle)->i, passDeads = cleo->ReadParam(handle)->i;

    static int lastFound = 0;
    if(!next) lastFound = 0;

    auto objects = *(GTAPedSA**)(*pedPool + 0);
    tByteFlag* flags = *(tByteFlag**)(*pedPool + 4);
    int size = *(int*)(*pedPool + 8);

    for(int i = lastFound; i < size; ++i)
    {
        if(flags[i].bEmpty) continue;
        auto& ent = objects[i];
        if(passDeads != -1 && (ent.Player() || (passDeads && !((ent.IntAt(1100) & 0xFFFFFFFE) != 54)) || (ent.IntAt(1160) >> 3) & 1)) continue;
        if(radius >= 1000.0f || (ent.GetPos() - center).SqrMagnitude() <= sqrradius)
        {
            lastFound = i + 1;
            cleo->GetPointerToScriptVar(handle)->i = GetPedRef(ent.AsInt());
            UpdateCompareFlag(handle, true);
            return;
        }
    }

    cleo->GetPointerToScriptVar(handle)->i = -1;
    UpdateCompareFlag(handle, false);
    lastFound = 0;
}

struct GTAVehicleSA : GTAEntity
{
    char structure[2604];
};
uintptr_t* vehiclePool;
int (*GetVehicleRef)(int);
CLEO_Fn(GET_RANDOM_CAR_IN_SPHERE_NO_SAVE_RECURSIVE)
{
    GTAVector3D center;
    center.x = cleo->ReadParam(handle)->f;
    center.y = cleo->ReadParam(handle)->f;
    center.z = cleo->ReadParam(handle)->f;
    float radius = cleo->ReadParam(handle)->f, sqrradius = radius*radius;
    int next = cleo->ReadParam(handle)->i, passWrecked = cleo->ReadParam(handle)->i;

    static int lastFound = 0;
    if(!next) lastFound = 0;

    auto objects = *(GTAVehicleSA**)(*pedPool + 0);
    tByteFlag* flags = *(tByteFlag**)(*pedPool + 4);
    int size = *(int*)(*pedPool + 8);

    for(int i = lastFound; i < size; ++i)
    {
        if(flags[i].bEmpty) continue;
        auto& ent = objects[i];
        if((passWrecked && ((ent.UInt8At(58) & 0xF8) == 40 || (ent.UInt8At(1071) >> 6) & 1)) || ((ent.UInt8At(1070) >> 2) & 1)) continue;
        if(radius >= 1000.0f || (ent.GetPos() - center).SqrMagnitude() <= sqrradius)
        {
            lastFound = i + 1;
            cleo->GetPointerToScriptVar(handle)->i = GetVehicleRef(ent.AsInt());
            UpdateCompareFlag(handle, true);
            return;
        }
    }

    cleo->GetPointerToScriptVar(handle)->i = -1;
    UpdateCompareFlag(handle, false);
    lastFound = 0;
}

uintptr_t* objectPool;
int (*GetObjectRef)(int);
struct GTAObjectSA : GTAEntity
{
    char structure[420];
};
CLEO_Fn(GET_RANDOM_OBJECT_IN_SPHERE_NO_SAVE_RECURSIVE)
{
    GTAVector3D center;
    center.x = cleo->ReadParam(handle)->f;
    center.y = cleo->ReadParam(handle)->f;
    center.z = cleo->ReadParam(handle)->f;
    float radius = cleo->ReadParam(handle)->f, sqrradius = radius*radius;
    int next = cleo->ReadParam(handle)->i;

    static int lastFound = 0;
    if(!next) lastFound = 0;

    auto objects = *(GTAObjectSA**)(*pedPool + 0);
    tByteFlag* flags = *(tByteFlag**)(*pedPool + 4);
    int size = *(int*)(*pedPool + 8);

    for(int i = lastFound; i < size; ++i)
    {
        if(flags[i].bEmpty) continue;
        auto& ent = objects[i];
        if((ent.UInt8At(326) >> 6) & 1) continue;
        if(radius >= 1000.0f || (ent.GetPos() - center).SqrMagnitude() <= sqrradius)
        {
            lastFound = i + 1;
            cleo->GetPointerToScriptVar(handle)->i = GetObjectRef(ent.AsInt());
            UpdateCompareFlag(handle, true);
            return;
        }
    }

    cleo->GetPointerToScriptVar(handle)->i = -1;
    UpdateCompareFlag(handle, false);
    lastFound = 0;
}

CLEO_Fn(GET_PED_REF)
{
    int ref = cleo->ReadParam(handle)->i;
    cleo->GetPointerToScriptVar(handle)->i = GetPedRef(ref);
}

CLEO_Fn(GET_VEHICLE_REF)
{
    int ref = cleo->ReadParam(handle)->i;
    cleo->GetPointerToScriptVar(handle)->i = GetVehicleRef(ref);
}

CLEO_Fn(GET_OBJECT_REF)
{
    int ref = cleo->ReadParam(handle)->i;
    cleo->GetPointerToScriptVar(handle)->i = GetObjectRef(ref);
}

CLEO_Fn(POW)
{
    float base = cleo->ReadParam(handle)->f;
    float arg = cleo->ReadParam(handle)->f;
    cleo->GetPointerToScriptVar(handle)->f = powf(base, arg);
}

CLEO_Fn(LOG)
{
    float arg = cleo->ReadParam(handle)->f;
    float base = cleo->ReadParam(handle)->f;
    cleo->GetPointerToScriptVar(handle)->f = (float)(logf(arg) / logf(base));
}

void Init4Opcodes()
{
    SET_TO(ScriptSpace, cleo->GetMainLibrarySymbol("_ZN11CTheScripts11ScriptSpaceE"));
    SET_TO(UpdateCompareFlag, cleo->GetMainLibrarySymbol("_ZN14CRunningScript17UpdateCompareFlagEh"));

    SET_TO(GetPedFromRef, cleo->GetMainLibrarySymbol("_ZN6CPools6GetPedEi"));
    CLEO_RegisterOpcode(0x0A96, GET_PED_POINTER); // 0A96=2,%2d% = actor %1d% struct

    SET_TO(GetVehicleFromRef, cleo->GetMainLibrarySymbol("_ZN6CPools10GetVehicleEi"));
    CLEO_RegisterOpcode(0x0A97, GET_VEHICLE_POINTER); // 0A97=2,%2d% = car %1d% struct

    SET_TO(GetObjectFromRef, cleo->GetMainLibrarySymbol("_ZN6CPools9GetObjectEi"));
    CLEO_RegisterOpcode(0x0A98, GET_OBJECT_POINTER); // 0A98=2,%2d% = object %1d% struct

    CLEO_RegisterOpcode(0x0A9F, GET_THIS_SCRIPT_STRUCT); // 0A9F=1,%1d% = current_thread_pointer

    CLEO_RegisterOpcode(0x0AA0, GOSUB_IF_FALSE); // 0AA0=1,gosub_if_false %1p%

    if(*nGameIdent == GTASA)
    {
        CLEO_RegisterOpcode(0x0ABD, IS_CAR_SIREN_ON); // 0ABD=1,  vehicle %1d% siren_on
        CLEO_RegisterOpcode(0x0ABE, IS_CAR_ENGINE_ON); // 0ABE=1,  vehicle %1d% engine_on

        SET_TO(pedPool, cleo->GetMainLibrarySymbol("_ZN6CPools11ms_pPedPoolE"));
        CLEO_RegisterOpcode(0x0AE1, GET_RANDOM_CHAR_IN_SPHERE_NO_SAVE_RECURSIVE); // 0AE1=7,%7d% = find_actor_near_point %1d% %2d% %3d% in_radius %4d% find_next %5h% pass_deads %6h% //IF and SET
        SET_TO(vehiclePool, cleo->GetMainLibrarySymbol("_ZN6CPools15ms_pVehiclePoolE"));
        CLEO_RegisterOpcode(0x0AE2, GET_RANDOM_CAR_IN_SPHERE_NO_SAVE_RECURSIVE); // 0AE2=7,%7d% = find_vehicle_near_point %1d% %2d% %3d% in_radius %4d% find_next %5h% pass_wrecked %6h% //IF and SET
        SET_TO(objectPool, cleo->GetMainLibrarySymbol("_ZN6CPools14ms_pObjectPoolE"));
        CLEO_RegisterOpcode(0x0AE3, GET_RANDOM_OBJECT_IN_SPHERE_NO_SAVE_RECURSIVE); // 0AE3=6,%6d% = find_object_near_point %1d% %2d% %3d% in_radius %4d% find_next %5h% //IF and SET
    }

    SET_TO(GetPedRef, cleo->GetMainLibrarySymbol("_ZN6CPools9GetPedRefEP4CPed"));
    CLEO_RegisterOpcode(0x0AEA, GET_PED_REF); // 0AEA=2,%2d% = actor_struct %1d% handle

    SET_TO(GetVehicleRef, cleo->GetMainLibrarySymbol("_ZN6CPools13GetVehicleRefEP8CVehicle"));
    CLEO_RegisterOpcode(0x0AEB, GET_VEHICLE_REF); // 0AEB=2,%2d% = car_struct %1d% handle

    SET_TO(GetObjectRef, cleo->GetMainLibrarySymbol("_ZN6CPools12GetObjectRefEP7CObject"));
    CLEO_RegisterOpcode(0x0AEC, GET_OBJECT_REF); // 0AEC=2,%2d% = object_struct %1d% handle

    CLEO_RegisterOpcode(0x0AEE, POW); // 0AEE=3,%3d% = %1d% exp %2d% //all floats
    CLEO_RegisterOpcode(0x0AEF, LOG); // 0AEF=3,%3d% = log %1d% base %2d% //all floats
}